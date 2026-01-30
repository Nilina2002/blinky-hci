#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "driver/i2s.h"
#include <math.h>

// ---------------- OLED ----------------
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 17, 18);

// ---------------- PINS ----------------
const int btnLeft = 11;
const int btnRight = 10;
const int btnWake = 37;

const int led1 = 13;
const int led2 = 16;

// ---------------- I2S (SAFE PINS) ----------------
#define I2S_BCLK  5
#define I2S_LRC   6
#define I2S_DOUT  7

#define SAMPLE_RATE 44100
#define AMPLITUDE   12000

// ---------------- AUDIO ----------------
struct Note {
  int freq;
  int duration;
};

Note melody[] = {
  {262, 300}, {330, 300}, {392, 300}, {523, 600}, {0, 400}
};
const int melodyLength = sizeof(melody) / sizeof(melody[0]);

int currentNote = 0;
unsigned long noteStartTime = 0;
bool melodyActive = false;

// ---------------- SYSTEM STATE ----------------
enum SystemState {
  STUDYING,
  SLEEP_ALERT,
  GAME_RUNNING
};

SystemState currentState = STUDYING;

// ---------------- AKNO EYES ----------------
const int eyeWidth = 28;
const int eyeHeight = 38;
const int eyeGap = 10;
const int eyeRadius = 6;

const int leftEyeX = 26;
const int rightEyeX = 74;
const int eyeY = 12;

int currentHeight = eyeHeight;
int blinkState = 0;
unsigned long nextBlinkTime = 0;
unsigned long nextSaccadeTime = 0;

int pupilOffsetX = 0;
int targetPupilX = 0;

// ---------------- GAME ----------------
int blockX, blockY = 0;
int blockSpeed = 3;
int catcherX = 54;
const int catcherWidth = 20;
const int catcherHeight = 4;
int score = 0;

unsigned long lastGameUpdate = 0;

// ---------------- ALERT ----------------
unsigned long alertStartTime = 0;
unsigned long lastLedBlink = 0;
bool ledState = false;

// ---------------- I2S SETUP ----------------
void initI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
}

// ---------------- AUDIO PLAY ----------------
void playTone(int freq, int durationMs) {
  if (freq == 0) {
    i2s_zero_dma_buffer(I2S_NUM_0);
    return;
  }

  int samples = SAMPLE_RATE * durationMs / 1000;
  float phase = 0;
  float inc = 2.0 * PI * freq / SAMPLE_RATE;
  int16_t buffer[256];

  while (samples > 0) {
    int n = min(samples, 256);
    for (int i = 0; i < n; i++) {
      buffer[i] = (int16_t)(AMPLITUDE * sin(phase));
      phase += inc;
      if (phase > 2 * PI) phase -= 2 * PI;
    }
    size_t written;
    i2s_write(I2S_NUM_0, buffer, n * sizeof(int16_t), &written, portMAX_DELAY);
    samples -= n;
  }
}

// ---------------- NON-BLOCKING MELODY ----------------
void updateMelody() {
  if (!melodyActive) return;

  if (millis() - noteStartTime >= melody[currentNote].duration) {
    currentNote++;
    if (currentNote >= melodyLength) {
      melodyActive = false;
      i2s_zero_dma_buffer(I2S_NUM_0);
      return;
    }
    playTone(melody[currentNote].freq, melody[currentNote].duration);
    noteStartTime = millis();
  }
}

// ---------------- EYES ----------------
void drawAknoEyes(int height, int lookX, bool angry) {
  int y = eyeY + (eyeHeight - height) / 2;
  int r = height < 2 * eyeRadius ? height / 2 : eyeRadius;

  u8g2.drawRBox(leftEyeX + lookX, y, eyeWidth, height, r);
  u8g2.drawRBox(rightEyeX + lookX, y, eyeWidth, height, r);

  if (angry) {
    u8g2.setDrawColor(0);
    u8g2.drawTriangle(leftEyeX - 5, y - 5, leftEyeX + eyeWidth + 5, y - 5, leftEyeX, y + 15);
    u8g2.drawTriangle(rightEyeX - 5, y - 5, rightEyeX + eyeWidth + 5, y - 5, rightEyeX + eyeWidth, y + 15);
    u8g2.setDrawColor(1);
  }
}

void updateAknoPhysics() {
  unsigned long now = millis();

  if (now > nextSaccadeTime) {
    targetPupilX = random(-8, 9);
    nextSaccadeTime = now + random(1000, 4000);
  }

  pupilOffsetX += (pupilOffsetX < targetPupilX) - (pupilOffsetX > targetPupilX);

  if (blinkState == 0 && now > nextBlinkTime) blinkState = 1;

  if (blinkState == 1) {
    currentHeight -= 4;
    if (currentHeight <= 2) blinkState = 2;
  } else if (blinkState == 2) {
    currentHeight += 4;
    if (currentHeight >= eyeHeight) {
      currentHeight = eyeHeight;
      blinkState = 0;
      nextBlinkTime = now + random(2000, 6000);
    }
  }
}

// ---------------- GAME ----------------
void resetGame() {
  score = 0;
  blockSpeed = 3;
  blockY = 0;
  blockX = random(0, 124);
  catcherX = 54;
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(9600);
  Wire.setClock(400000);

  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);
  pinMode(btnWake, INPUT_PULLUP);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);

  u8g2.begin();
  initI2S();
  randomSeed(analogRead(0));
}

// ---------------- LOOP ----------------
void loop() {
  if (Serial.available()) {
    if (Serial.readStringUntil('\n').indexOf("SLEEP") >= 0)
      currentState = SLEEP_ALERT;
  }

  if (digitalRead(btnWake) == LOW) {
    currentState = STUDYING;
    digitalWrite(led1, LOW);
    digitalWrite(led2, LOW);
    return;
  }

  u8g2.clearBuffer();

  if (currentState == STUDYING) {
    updateAknoPhysics();
    drawAknoEyes(currentHeight, pupilOffsetX, false);
  }

  else if (currentState == SLEEP_ALERT) {
    drawAknoEyes(eyeHeight, 0, true);

    if ((millis() / 200) % 2 == 0)
      u8g2.drawStr(35, 60, "WAKE UP!");

    if (millis() - alertStartTime > 3000) {
      currentState = GAME_RUNNING;
      melodyActive = true;
      currentNote = 0;
      noteStartTime = millis();
      playTone(melody[0].freq, melody[0].duration);
      resetGame();
    }
  }

  else if (currentState == GAME_RUNNING) {
    updateMelody();

    if (millis() - lastGameUpdate > 30) {
      lastGameUpdate = millis();
      blockY += blockSpeed;
    }

    if (digitalRead(btnLeft) == LOW) catcherX -= 6;
    if (digitalRead(btnRight) == LOW) catcherX += 6;

    u8g2.setCursor(0, 10);
    u8g2.print("Score: ");
    u8g2.print(score);

    u8g2.drawBox(blockX, blockY, 4, 4);
    u8g2.drawBox(catcherX, 60, catcherWidth, catcherHeight);
  }

  u8g2.sendBuffer();
}
