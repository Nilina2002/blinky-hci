#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ---------------- OLED ----------------
// Hardware I2C is REQUIRED for fast animation
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
  STUDYING,     // Akno "Cute" Eyes
  SLEEP_ALERT,  // Akno "Angry" Eyes
  GAME_RUNNING  // Reflex Game
};

SystemState currentState = STUDYING;

// ---------------- AKNO EYE CONFIG ----------------
// Akno uses Rectangular Rounded Eyes
const int eyeWidth = 28;
const int eyeHeight = 38;
const int eyeGap = 10;
const int eyeRadius = 6; // Corner roundness

// Positions
const int leftEyeX = 26;
const int rightEyeX = 74;
const int eyeY = 12;

// Animation Variables
int currentHeight = eyeHeight; // Used for blinking
int blinkState = 0; // 0=Open, 1=Closing, 2=Opening
unsigned long nextBlinkTime = 0;
unsigned long nextSaccadeTime = 0;

// Eye Look Offset (Looking left/right)
int pupilOffsetX = 0; 
int pupilOffsetY = 0; 
int targetPupilX = 0;

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

// --- 1. AKNO DRAWING ENGINE ---
void drawAknoEyes(int height, int lookX, bool isAngry) {
  // Center Y adjustment based on height (so they blink to the middle)
  int currentY = eyeY + (eyeHeight - height) / 2;
  
  // 1. Draw Left Eye (Rounded Box)
  u8g2.drawRBox(leftEyeX + lookX, currentY, eyeWidth, height, eyeRadius);
  
  // 2. Draw Right Eye (Rounded Box)
  u8g2.drawRBox(rightEyeX + lookX, currentY, eyeWidth, height, eyeRadius);

  // 3. ANGRY EYELIDS (The "Akno" Angry look)
  // We draw black triangles over the top corners to create a slant
  if (isAngry) {
    u8g2.setDrawColor(0); // Erase mode
    
    // Left Eye Slant ( / )
    u8g2.drawTriangle(
      leftEyeX - 5, currentY - 5,                 // Top Left (Way out)
      leftEyeX + eyeWidth + 5, currentY - 5,      // Top Right (Way out)
      leftEyeX, currentY + 15                     // Inner point (The slant)
    );
    
    // Right Eye Slant ( \ )
    u8g2.drawTriangle(
      rightEyeX - 5, currentY - 5,                // Top Left
      rightEyeX + eyeWidth + 5, currentY - 5,     // Top Right
      rightEyeX + eyeWidth, currentY + 15         // Inner point
    );
    
    u8g2.setDrawColor(1); // Restore draw mode
  }
}

void updateAknoPhysics() {
  unsigned long now = millis();

  // A. LOOKING AROUND (Saccades)
  if (now > nextSaccadeTime) {
    // Pick a new random horizontal spot (Akno looks mostly L/R)
    targetPupilX = random(-8, 9);
    nextSaccadeTime = now + random(1000, 4000);
  }

  // Smooth movement
  if (pupilOffsetX < targetPupilX) pupilOffsetX += 1;
  if (pupilOffsetX > targetPupilX) pupilOffsetX -= 1;

  // B. BLINKING
  if (blinkState == 0 && now > nextBlinkTime) {
    blinkState = 1; // Start closing
  }
  
  if (blinkState == 1) { // Closing
    currentHeight -= 4; 
    if (currentHeight <= 2) { // Fully closed
        currentHeight = 2;
        blinkState = 2; // Open again
    }
  } else if (blinkState == 2) { // Opening
    currentHeight += 4;
    if (currentHeight >= eyeHeight) {
      currentHeight = eyeHeight;
      blinkState = 0; // Done
      nextBlinkTime = now + random(2000, 6000);
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
  currentHeight = eyeHeight; // Eyes open
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(9600);
  
  // Set I2C Speed (Remove if screen glitches)
  Wire.setClock(400000); 

  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);
  pinMode(btnWake, INPUT_PULLUP);

  pinMode(led1, OUTPUT); pinMode(led2, OUTPUT); pinMode(buzzerPin, OUTPUT);

  u8g2.begin();
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

  // ================= STATE: STUDYING (AKNO CUTE) =================
  if (currentState == STUDYING) {
    updateAknoPhysics();
    // Draw Cute Eyes (False = Not Angry)
    drawAknoEyes(currentHeight, pupilOffsetX, false); 
  }

  // ================= STATE: SLEEP ALERT (AKNO ANGRY) =================
  else if (currentState == SLEEP_ALERT) {
    
    // Angry Eyes! (Full height, Centered, True = Angry)
    drawAknoEyes(eyeHeight, 0, true);

    // Flashing Text
    u8g2.setDrawColor(2); // XOR Mode
    u8g2.setFont(u8g2_font_ncenB10_tr);
    if ((millis() / 200) % 2 == 0) u8g2.drawStr(35, 60, "WAKE UP!");

    // LED BLINK
    if (millis() - lastLedBlink > 100) {
      lastLedBlink = millis();
      ledState = !ledState;
      digitalWrite(led1, ledState); digitalWrite(led2, !ledState);
    }

    // BUZZER
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
    // Game Physics
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

    // Draw Game
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(0, 10); u8g2.print("Score: "); u8g2.print(score);
    u8g2.drawBox(blockX, blockY, 4, 4);
    u8g2.drawBox(catcherX, 64 - catcherHeight, catcherWidth, catcherHeight);
  }

  u8g2.sendBuffer();
}