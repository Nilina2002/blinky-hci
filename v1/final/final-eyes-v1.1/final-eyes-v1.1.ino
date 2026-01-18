#include <Arduino.h>
#include <Wire.h>   // <--- FIXED: Added this for Wire.setClock
#include <U8g2lib.h>

// ---------------- OLED ----------------
// Hardware I2C is REQUIRED for animation speed
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
  STUDYING,     // Intellar Eyes Active
  SLEEP_ALERT,  // Angry Eyes + Alarm
  GAME_RUNNING  // Reflex Game
};

SystemState currentState = STUDYING;

// ---------------- INTELLAR EYE VARIABLES ----------------
// Eye Geometry
const int eyeRadius = 18;
const int eyeX_L = 32;
const int eyeX_R = 96;
const int eyeY = 32;
const int pupilRadius = 6;

// Animation State
int currentPupilX = 0;      // Current pupil offset X
int currentPupilY = 0;      // Current pupil offset Y
int targetPupilX = 0;
int targetPupilY = 0;

// Blinking State
int eyelidHeight = 0;       // 0 = Open, eyeRadius = Closed
int blinkState = 0;         // 0=Open, 1=Closing, 2=Opening
unsigned long nextBlinkTime = 0;
unsigned long nextSaccadeTime = 0; // Next eye movement time

// ---------------- GAME VARIABLES ----------------
int blockX, blockY = 0, blockSpeed = 3;
int catcherX = 54;
const int catcherWidth = 20;
const int catcherHeight = 4;
int score = 0;

// ---------------- ALERT VARS ----------------
unsigned long alertStartTime = 0;
unsigned long lastLedBlink = 0;
bool ledState = false;
bool beeping = false;

// ---------------- FUNCTIONS ----------------

// --- 1. EYE DRAWING ENGINE ---
void drawOneEye(int centerX, int centerY, int pX, int pY, int lidHeight) {
  // 1. Draw Sclera (The White Part)
  // FIXED: Changed drawFilledCircle to drawDisc
  u8g2.drawDisc(centerX, centerY, eyeRadius);
  
  // 2. Draw Pupil (Black Circle) - constrained within the eye
  // We limit movement so pupil doesn't leave the eye
  int drawPx = constrain(pX, -10, 10); 
  int drawPy = constrain(pY, -10, 10);
  
  // Set color to Black (Erase mode) for the pupil inside the white eye
  u8g2.setDrawColor(0); 
  // FIXED: Changed drawFilledCircle to drawDisc
  u8g2.drawDisc(centerX + drawPx, centerY + drawPy, pupilRadius);
  u8g2.setDrawColor(1); // Restore White

  // 3. Draw Eyelids (Black Boxes) to simulate blinking/emotions
  if (lidHeight > 0) {
    u8g2.setDrawColor(0); // Black to "erase" top/bottom of eye
    // Top Lid
    u8g2.drawBox(centerX - eyeRadius, centerY - eyeRadius, eyeRadius * 2 + 1, lidHeight);
    // Bottom Lid
    u8g2.drawBox(centerX - eyeRadius, centerY + eyeRadius - lidHeight, eyeRadius * 2 + 1, lidHeight);
    u8g2.setDrawColor(1);
  }
}

void updateEyePhysics() {
  unsigned long now = millis();

  // A. SACCADES (Eye Movement)
  if (now > nextSaccadeTime) {
    // Pick a new random spot to look at
    targetPupilX = random(-12, 13);
    targetPupilY = random(-10, 11);
    
    // Schedule next movement (random time between 0.5s and 3s)
    nextSaccadeTime = now + random(500, 3000);
  }

  // Smoothly move current pupil towards target (Easing)
  if (currentPupilX < targetPupilX) currentPupilX++;
  if (currentPupilX > targetPupilX) currentPupilX--;
  if (currentPupilY < targetPupilY) currentPupilY++;
  if (currentPupilY > targetPupilY) currentPupilY--;

  // B. BLINKING
  if (blinkState == 0 && now > nextBlinkTime) {
    blinkState = 1; // Start closing
  }
  
  if (blinkState == 1) { // Closing
    eyelidHeight += 4; // Close speed
    if (eyelidHeight >= eyeRadius) {
      blinkState = 2; // Switch to opening
    }
  } else if (blinkState == 2) { // Opening
    eyelidHeight -= 4; // Open speed
    if (eyelidHeight <= 0) {
      eyelidHeight = 0;
      blinkState = 0; // Finished
      nextBlinkTime = now + random(2000, 6000); // Next blink in 2-6s
    }
  }
}

// --- 2. GAME LOGIC ---
void resetGame() {
  score = 0; blockSpeed = 3; catcherX = 54;
  blockY = 0; blockX = random(0, 124);
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
  digitalWrite(led1, LOW); digitalWrite(led2, LOW); digitalWrite(buzzerPin, LOW);
  eyelidHeight = 0; // Reset eyes
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(9600);

  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);
  pinMode(btnWake, INPUT_PULLUP);

  pinMode(led1, OUTPUT); pinMode(led2, OUTPUT); pinMode(buzzerPin, OUTPUT);

  u8g2.begin();
  
  // FIXED: Moved setClock AFTER begin() so it doesn't get overwritten
  // This makes the animation smooth
  Wire.setClock(400000); 
  
  randomSeed(analogRead(0));

  resetToStudying();
}

// ---------------- LOOP ----------------
void loop() {

  // 1. SERIAL CHECK
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "SLEEP" || cmd == "1") startSleepAlert();
  }

  // 2. WAKE BUTTON
  if (digitalRead(btnWake) == LOW) {
    resetToStudying();
    delay(300); return;
  }

  u8g2.clearBuffer();

  // ================= STATE: STUDYING (INTELLAR EYES) =================
  if (currentState == STUDYING) {
    updateEyePhysics();
    // Draw Left and Right eyes with current physics state
    drawOneEye(eyeX_L, eyeY, currentPupilX, currentPupilY, eyelidHeight);
    drawOneEye(eyeX_R, eyeY, currentPupilX, currentPupilY, eyelidHeight);
  }

  // ================= STATE: SLEEP ALERT (ANGRY EYES) =================
  else if (currentState == SLEEP_ALERT) {
    
    // Draw "Angry" Eyes (Half closed, staring straight)
    int angryLid = 10; // Half closed
    drawOneEye(eyeX_L, eyeY, 0, 0, angryLid);
    drawOneEye(eyeX_R, eyeY, 0, 0, angryLid);

    // Draw "WAKE UP" Text over the eyes
    u8g2.setDrawColor(2); // XOR mode to see text over eyes
    u8g2.setFont(u8g2_font_ncenB10_tr);
    if ((millis() / 200) % 2 == 0) u8g2.drawStr(35, 60, "WAKE UP!");

    // LED BLINK
    if (millis() - lastLedBlink > 100) {
      lastLedBlink = millis();
      ledState = !ledState;
      digitalWrite(led1, ledState); digitalWrite(led2, !ledState);
    }

    // BUZZER LOGIC
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
    // Normal Game Logic
    if (digitalRead(btnLeft) == LOW && catcherX > 0) catcherX -= 6;
    if (digitalRead(btnRight) == LOW && catcherX < (128 - catcherWidth)) catcherX += 6;

    blockY += blockSpeed;
    if (blockY + 4 >= 64 - catcherHeight) {
      if (blockX + 4 > catcherX && blockX < catcherX + catcherWidth) {
        score++;
        blockY = 0; blockX = random(0, 124);
        if (score % 5 == 0) blockSpeed++;
      } else if (blockY >= 64) {
        resetGame();
      }
    }

    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(0, 10); u8g2.print("Score: "); u8g2.print(score);
    u8g2.drawBox(blockX, blockY, 4, 4);
    u8g2.drawBox(catcherX, 64 - catcherHeight, catcherWidth, catcherHeight);
  }

  u8g2.sendBuffer();
}