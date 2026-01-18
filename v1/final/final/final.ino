#include <Arduino.h>
#include <U8g2lib.h>

// ---------------- OLED ----------------
// Hardware I2C for better performance
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
  IDLE,
  SLEEP_ALERT,
  GAME_RUNNING
};

SystemState currentState = IDLE;

// ---------------- GAME VARIABLES ----------------
int blockX;
int blockY = 0;
int blockSpeed = 3;

int catcherX = 54;
const int catcherWidth = 20;
const int catcherHeight = 4;

int score = 0;

// ---------------- TIMING ----------------
unsigned long lastBlink = 0;
bool ledState = false;

unsigned long beepStart = 0;
bool beeping = false;

// ---------------- FUNCTIONS ----------------
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
  // Only trigger if we are currently IDLE
  if (currentState == IDLE) {
    currentState = SLEEP_ALERT;
    beeping = true;
    beepStart = millis();
    resetGame();
  }
}

void stopEverything() {
  currentState = IDLE;
  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);
  digitalWrite(buzzerPin, LOW);

  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

// ---------------- SETUP ----------------
void setup() {
  // FIXED: Changed to 9600 to match your working test code
  Serial.begin(9600); 

  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);
  pinMode(btnWake, INPUT_PULLUP);

  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  u8g2.begin();
  randomSeed(analogRead(0));

  stopEverything();
}

// ---------------- LOOP ----------------
void loop() {

  // ---- SERIAL INPUT HANDLING (FIXED) ----
  if (Serial.available()) {
    // We prefer reading strings, but let's be flexible
    String cmd = Serial.readStringUntil('\n');
    cmd.trim(); // Remove whitespace

    // Accept "SLEEP" OR "1" to ensure it triggers
    if (cmd == "SLEEP" || cmd == "1") {
      startSleepAlert();
    }
  }

  // ---- WAKE BUTTON (GLOBAL) ----
  if (digitalRead(btnWake) == LOW) {
    stopEverything();
    // Debounce delay so we don't accidentally re-trigger
    delay(300); 
    return;
  }

  // ================= SLEEP ALERT =================
  if (currentState == SLEEP_ALERT) {

    // LED BLINK (Every 200ms for faster urgency)
    if (millis() - lastBlink > 200) {
      lastBlink = millis();
      ledState = !ledState;
      digitalWrite(led1, ledState);
      digitalWrite(led2, !ledState);
    }

    // BUZZER FOR 3 SECONDS
    if (beeping) {
      digitalWrite(buzzerPin, HIGH);
      if (millis() - beepStart > 3000) {
        digitalWrite(buzzerPin, LOW);
        beeping = false;
        currentState = GAME_RUNNING;
      }
    }
  }

  // ================= GAME RUNNING =================
  if (currentState == GAME_RUNNING) {

    // ---- INPUT ----
    if (digitalRead(btnLeft) == LOW && catcherX > 0) catcherX -= 6; // Increased speed slightly
    if (digitalRead(btnRight) == LOW && catcherX < (128 - catcherWidth)) catcherX += 6;

    // ---- GAME PHYSICS ----
    blockY += blockSpeed;

    if (blockY + 4 >= 64 - catcherHeight) {
      // Collision Detection
      if (blockX + 4 > catcherX && blockX < catcherX + catcherWidth) {
        score++;
        resetBlock();
        if (score % 5 == 0) blockSpeed++;
      } 
      // Missed the block
      else if (blockY >= 64) {
        // Game Over logic - just restart for now, or go back to IDLE
        resetGame(); 
      }
    }

    // ---- DRAW ----
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    
    // Draw Score
    u8g2.setCursor(0, 10);
    u8g2.print("Score: ");
    u8g2.print(score);

    // Draw Block
    u8g2.drawBox(blockX, blockY, 4, 4);
    
    // Draw Catcher
    u8g2.drawBox(catcherX, 64 - catcherHeight, catcherWidth, catcherHeight);

    u8g2.sendBuffer();
  }

  delay(10); // Small delay for stability
}