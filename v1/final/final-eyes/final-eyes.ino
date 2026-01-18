#include <Arduino.h>
#include <U8g2lib.h>

// ---------------- OLED ----------------
// Hardware I2C (Pins 17=SDA, 18=SCL approx, depending on board)
// IF THIS DOESN'T WORK, REVERT TO YOUR PREVIOUS SW_I2C LINE
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 17, 18);

// ---------------- PINS ----------------
const int btnLeft = 11;
const int btnRight = 10;
const int btnWake = 37;

const int led1 = 13;
const int led2 = 16;
const int buzzerPin = 15;

// ---------------- SYSTEM STATE ----------------
enum SystemState {
  STUDYING,     // Formerly "IDLE" - Now has eyes
  SLEEP_ALERT,
  GAME_RUNNING
};

SystemState currentState = STUDYING;

// ---------------- EYE ANIMATION VARS ----------------
unsigned long lastBlinkTime = 0;
int blinkInterval = 3000;   // Time between blinks
bool eyesClosed = false;
unsigned long blinkStartTime = 0;

// ---------------- GAME VARIABLES ----------------
int blockX;
int blockY = 0;
int blockSpeed = 3;
int catcherX = 54;
const int catcherWidth = 20;
const int catcherHeight = 4;
int score = 0;

// ---------------- ALERT TIMING ----------------
unsigned long lastLedBlink = 0;
bool ledState = false;
unsigned long alertStartTime = 0;
bool beeping = false;

// ---------------- HELPER FUNCTIONS ----------------

void drawEyes(bool closed) {
  u8g2.clearBuffer();
  
  if (closed) {
    // Draw "Sleeping/Blinking" lines
    u8g2.drawBox(30, 32, 24, 4); // Left Eye Lash
    u8g2.drawBox(74, 32, 24, 4); // Right Eye Lash
  } else {
    // Draw Open Eyes (Outlines)
    u8g2.drawFrame(30, 20, 24, 24); // Left Eye Box
    u8g2.drawFrame(74, 20, 24, 24); // Right Eye Box
    
    // Draw Pupils (Filled Squares)
    // You can make these move later if you want!
    u8g2.drawBox(38, 28, 8, 8); // Left Pupil
    u8g2.drawBox(82, 28, 8, 8); // Right Pupil
  }
  
  u8g2.sendBuffer();
}

void resetBlock() {
  blockY = 0;
  blockX = random(0, 124);
}

void resetGame() {
  score = 0;
  blockSpeed = 3;
  catcherX = 54;
  resetBlock();
}

void startSleepAlert() {
  // Only trigger if we are currently STUDYING
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
  // Reset blink timer so eyes open immediately
  lastBlinkTime = millis();
  eyesClosed = false;
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(9600); // Kept at 9600 for compatibility

  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);
  pinMode(btnWake, INPUT_PULLUP);

  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  u8g2.begin();
  randomSeed(analogRead(0));

  resetToStudying();
}

// ---------------- LOOP ----------------
void loop() {

  // ---- 1. READ SERIAL COMMANDS ----
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "SLEEP" || cmd == "1") {
      startSleepAlert();
    }
  }

  // ---- 2. WAKE / RESET BUTTON ----
  if (digitalRead(btnWake) == LOW) {
    resetToStudying();
    delay(300); // Debounce
    return;
  }

  // ================= STATE: STUDYING (EYES ANIMATION) =================
  if (currentState == STUDYING) {
    unsigned long currentMillis = millis();

    // Check if it is time to blink
    if (!eyesClosed && (currentMillis - lastBlinkTime > blinkInterval)) {
      eyesClosed = true;
      blinkStartTime = currentMillis;
      drawEyes(true); // Draw closed eyes
    }

    // Check if blink is finished (keep eyes closed for 150ms)
    if (eyesClosed && (currentMillis - blinkStartTime > 150)) {
      eyesClosed = false;
      lastBlinkTime = currentMillis;
      // Randomize next blink time (between 2 and 5 seconds)
      blinkInterval = random(2000, 5000); 
      drawEyes(false); // Draw open eyes
    }

    // If we aren't blinking, ensure eyes are drawn open
    // (We only redraw if state changes to save FPS, but simplified here)
    if (!eyesClosed) {
       // Optional: Add pupil movement logic here later
    }
  }

  // ================= STATE: SLEEP ALERT =================
  else if (currentState == SLEEP_ALERT) {
    
    // Flash Screen
    u8g2.clearBuffer();
    if ((millis() / 200) % 2 == 0) { // Text flashes every 200ms
        u8g2.setFont(u8g2_font_ncenB14_tr); 
        u8g2.drawStr(20, 40, "WAKE UP!");
    }
    u8g2.sendBuffer();

    // LED BLINK
    if (millis() - lastLedBlink > 100) {
      lastLedBlink = millis();
      ledState = !ledState;
      digitalWrite(led1, ledState);
      digitalWrite(led2, !ledState);
    }

    // BUZZER TIMEOUT
    if (beeping) {
      digitalWrite(buzzerPin, HIGH);
      if (millis() - alertStartTime > 3000) {
        digitalWrite(buzzerPin, LOW);
        beeping = false;
        currentState = GAME_RUNNING;
      }
    }
  }

  // ================= STATE: GAME RUNNING =================
  else if (currentState == GAME_RUNNING) {
    
    // Input
    if (digitalRead(btnLeft) == LOW && catcherX > 0) catcherX -= 6;
    if (digitalRead(btnRight) == LOW && catcherX < (128 - catcherWidth)) catcherX += 6;

    // Physics
    blockY += blockSpeed;

    if (blockY + 4 >= 64 - catcherHeight) {
      if (blockX + 4 > catcherX && blockX < catcherX + catcherWidth) {
        score++;
        resetBlock();
        if (score % 5 == 0) blockSpeed++;
      } else if (blockY >= 64) {
        resetGame(); // Game Over - restart game
      }
    }

    // Draw Game
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(0, 10);
    u8g2.print("Score: ");
    u8g2.print(score);
    u8g2.drawBox(blockX, blockY, 4, 4);
    u8g2.drawBox(catcherX, 64 - catcherHeight, catcherWidth, catcherHeight);
    u8g2.sendBuffer();
  }
}