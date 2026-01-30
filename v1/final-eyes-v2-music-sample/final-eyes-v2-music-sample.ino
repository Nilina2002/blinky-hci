#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "driver/i2s.h"

// ================= OLED =================
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, 17, 18
);

// ================= PINS =================
const int btnLeft  = 11;
const int btnRight = 10;
const int btnWake  = 37;

const int led1 = 13;
const int led2 = 16;
const int buzzerPin = 15;

// ================= SYSTEM STATE =================
enum SystemState {
  STUDYING,
  SLEEP_ALERT,
  GAME_RUNNING
};

SystemState currentState = STUDYING;

// ================= AKNO EYES =================
const int eyeWidth = 28;
const int eyeHeight = 38;
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

// ================= GAME =================
int blockX, blockY = 0, blockSpeed = 3;
int catcherX = 54;
const int catcherWidth = 20;
const int catcherHeight = 4;
int score = 0;

// ================= ALERT =================
unsigned long alertStartTime = 0;
unsigned long lastLedBlink = 0;
bool ledState = false;
bool beeping = false;

// ================= AUDIO (I2S) =================
#define I2S_PORT I2S_NUM_0
#define PIN_I2S_BCLK 2
#define PIN_I2S_LRC  42
#define PIN_I2S_DIN  1

const int sampleRate = 16000;
const int amplitude  = 8000;

int melody[] = { 523, 659, 784 }; // C5 E5 G5
int noteIndex = 0;
unsigned long lastNoteChange = 0;
const int noteDuration = 250;

// ================= FUNCTIONS =================
void setupI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sampleRate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };

  i2s_pin_config_t pins = {
    .bck_io_num = PIN_I2S_BCLK,
    .ws_io_num = PIN_I2S_LRC,
    .data_out_num = PIN_I2S_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  i2s_set_pin(I2S_PORT, &pins);
}

void playTone(int freq) {
  static float phase = 0;
  int16_t buffer[256];

  for (int i = 0; i < 256; i++) {
    phase += (float)freq / sampleRate;
    if (phase >= 1.0f) phase -= 1.0f;
    buffer[i] = (phase < 0.5f) ? amplitude : -amplitude;
  }

  size_t bytes;
  i2s_write(I2S_PORT, buffer, sizeof(buffer), &bytes, portMAX_DELAY);
}

// ================= EYES =================
void drawAknoEyes(int height, int lookX, bool angry) {
  int y = eyeY + (eyeHeight - height) / 2;
  int r = (height < 2 * eyeRadius) ? height / 2 : eyeRadius;

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

  if (pupilOffsetX < targetPupilX) pupilOffsetX++;
  if (pupilOffsetX > targetPupilX) pupilOffsetX--;

  if (blinkState == 0 && now > nextBlinkTime) blinkState = 1;

  if (blinkState == 1) {
    currentHeight -= 4;
    if (currentHeight <= 2) { currentHeight = 2; blinkState = 2; }
  } else if (blinkState == 2) {
    currentHeight += 4;
    if (currentHeight >= eyeHeight) {
      currentHeight = eyeHeight;
      blinkState = 0;
      nextBlinkTime = now + random(2000, 6000);
    }
  }
}

// ================= GAME HELPERS =================
void resetGame() {
  score = 0;
  blockSpeed = 3;
  catcherX = 54;
  blockY = 0;
  blockX = random(0, 124);
}

void startSleepAlert() {
  if (currentState == STUDYING) {
    currentState = SLEEP_ALERT;
    beeping = true;
    alertStartTime = millis();
    resetGame();
  }
}

void resetToStudying() {
  currentState = STUDYING;
  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);
  digitalWrite(buzzerPin, LOW);
  currentHeight = eyeHeight;
}

// ================= SETUP =================
void setup() {
  Serial.begin(9600);
  Wire.setClock(400000);

  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);
  pinMode(btnWake, INPUT_PULLUP);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  u8g2.begin();
  randomSeed(analogRead(0));

  setupI2S();
  resetToStudying();
}

// ================= LOOP =================
void loop() {
  if (Serial.available()) {
    if (Serial.readStringUntil('\n').startsWith("1")) startSleepAlert();
  }

  if (digitalRead(btnWake) == LOW) {
    resetToStudying();
    delay(300);
    return;
  }

  u8g2.clearBuffer();

  if (currentState == STUDYING) {
    updateAknoPhysics();
    drawAknoEyes(currentHeight, pupilOffsetX, false);
  }

  else if (currentState == SLEEP_ALERT) {
    drawAknoEyes(eyeHeight, 0, true);

    if (millis() - lastLedBlink > 100) {
      lastLedBlink = millis();
      ledState = !ledState;
      digitalWrite(led1, ledState);
      digitalWrite(led2, !ledState);
    }

    if (beeping) {
      digitalWrite(buzzerPin, HIGH);
      if (millis() - alertStartTime > 3000) {
        digitalWrite(buzzerPin, LOW);
        beeping = false;
        currentState = GAME_RUNNING;
      }
    }
  }

  else if (currentState == GAME_RUNNING) {
    if (digitalRead(btnLeft) == LOW && catcherX > 0) catcherX -= 6;
    if (digitalRead(btnRight) == LOW && catcherX < 108) catcherX += 6;

    blockY += blockSpeed;

    if (millis() - lastNoteChange > noteDuration) {
      lastNoteChange = millis();
      noteIndex = (noteIndex + 1) % 3;
    }
    playTone(melody[noteIndex]);

    if (blockY + 4 >= 64 - catcherHeight) {
      if (blockX + 4 > catcherX && blockX < catcherX + catcherWidth) {
        score++;
        blockY = 0;
        blockX = random(0, 124);
        if (score % 5 == 0) blockSpeed++;
      } else if (blockY >= 64) resetGame();
    }

    u8g2.setCursor(0, 10);
    u8g2.print("Score: ");
    u8g2.print(score);
    u8g2.drawBox(blockX, blockY, 4, 4);
    u8g2.drawBox(catcherX, 60, catcherWidth, catcherHeight);
  }

  u8g2.sendBuffer();
}
