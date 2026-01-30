#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// --- MUSIC NOTES (Hz) ---
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_B5  988
#define NOTE_C6  1047

// A fast, urgent "Chase" melody
int melody[] = { NOTE_E5, NOTE_G5, NOTE_A5, NOTE_G5, NOTE_B5, NOTE_C6 };
int melodyLen = 6;

// Timing variables for music
int noteIndex = 0;
unsigned long nextNoteTime = 0;
unsigned long silenceUntil = 0; // To pause music when catching a block

// ---------------- OLED (ESP32-S3 Specific) ----------------
// Ensure your OLED is wired to GPIO 17 (SDA) and GPIO 18 (SCL)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, 17, 18
);

// ---------------- PINS (ESP32-S3 SAFE MAPPING) ----------------
const int btnReset = 37; // If pin 37 doesn't exist on your board, use Pin 5
const int btnTimer = 4;  // Action / Timer Start

// SAFE PINS for S3 (Unlike old ESP32, 6 and 7 are safe here)
const int btnLeft  = 6;  
const int btnRight = 7;

// OUTPUTS
const int led1 = 13;      // Verify this pin exists on your specific S3 board
const int led2 = 16;
const int buzzerPin = 15;

// ---------------- SYSTEM STATE ----------------
enum SystemState {
  STUDYING,       // Idle: Eyes Only
  TIMER_RUNNING,  // Timer: Countdown
  GAME_RUNNING    // Alarm: Catcher Game
};

SystemState currentState = STUDYING;

// ---------------- VARIABLES ----------------
// Eyes
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

// Timer
const unsigned long TIMER_DURATION = 40UL * 60UL * 1000UL; 
unsigned long timerStartTime = 0;
bool timerButtonArmed = false;

// --- CATCHER GAME VARS ---
int catcherX = 54;
const int catcherWidth = 24;
const int catcherHeight = 4;

float blockX = 0;
float blockY = 0;
float blockSpeed = 1.5; 
int score = 0;
const int WIN_SCORE = 5; // Score needed to wake up

// ---------------- FUNCTIONS ----------------

void drawAknoEyes(int height, int lookX) {
  int currentY = eyeY + (eyeHeight - height) / 2;
  int radius = (height < 2 * eyeRadius) ? height / 2 : eyeRadius;

  u8g2.setDrawColor(1);
  u8g2.drawRBox(leftEyeX + lookX, currentY, eyeWidth, height, radius);
  u8g2.drawRBox(rightEyeX + lookX, currentY, eyeWidth, height, radius);
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

// void resetToStudying() {
//   currentState = STUDYING;
//   digitalWrite(led1, LOW);
//   digitalWrite(led2, LOW);
//   digitalWrite(buzzerPin, LOW);
//   currentHeight = eyeHeight;
//   timerButtonArmed = false;
// }
void resetToStudying() {
  currentState = STUDYING;
  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);
  
  // STOP MUSIC
  noTone(buzzerPin);      // Stop PWM sound
  digitalWrite(buzzerPin, LOW);
  
  currentHeight = eyeHeight;
  timerButtonArmed = false;
}

void initGame() {
  currentState = GAME_RUNNING;
  score = 0;
  blockSpeed = 1.5;
  catcherX = 52;
  blockY = 0;
  blockX = random(2, 122);
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  
  // ESP32-S3 allows any pins for I2C, but we stick to 17/18 as defined above
  Wire.begin(17, 18); 
  Wire.setClock(400000);

  // INPUTS (S3 has internal pullups on all these)
  pinMode(btnReset, INPUT_PULLUP);
  pinMode(btnTimer, INPUT_PULLUP);
  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);

  // OUTPUTS
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  u8g2.begin();
  randomSeed(analogRead(1)); // Use Pin 1 for random seed on S3 (0 is boot)

  resetToStudying();
}

// ---------------- LOOP ----------------
void loop() {

  // 1. GLOBAL RESET
  if (digitalRead(btnReset) == LOW) {
    resetToStudying();
    delay(300);
    return;
  }

  // 2. CHECK SERIAL (Python)
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    // '1' = Sleep Detected -> Trigger Game
    if (cmd == '1' && currentState != GAME_RUNNING) {
      initGame();
    }
  }

  // 3. STATE MACHINE
  switch (currentState) {

    // --- IDLE / STUDYING ---
    case STUDYING:
      {
        int btnState = digitalRead(btnTimer);

        if (btnState == HIGH) timerButtonArmed = true; 

        if (timerButtonArmed && btnState == LOW) {
           currentState = TIMER_RUNNING;
           timerStartTime = millis();
           delay(300); 
        }

        u8g2.clearBuffer();
        updateAknoPhysics();
        drawAknoEyes(currentHeight, pupilOffsetX);
        u8g2.sendBuffer();
      }
      break;

    // --- TIMER ---
    case TIMER_RUNNING:
      {
        unsigned long elapsed = millis() - timerStartTime;
        u8g2.clearBuffer();

        if (elapsed >= TIMER_DURATION) {
          digitalWrite(buzzerPin, HIGH);
          digitalWrite(led1, HIGH);
          digitalWrite(led2, HIGH);
          u8g2.setFont(u8g2_font_ncenB14_tr);
          u8g2.drawStr(20, 42, "DONE!");
        } else {
          unsigned long remain = TIMER_DURATION - elapsed;
          int m = remain / 60000;
          int s = (remain % 60000) / 1000;
          char buf[10];
          sprintf(buf, "%02d:%02d", m, s);
          u8g2.setFont(u8g2_font_ncenB24_tr);
          int w = u8g2.getStrWidth(buf);
          u8g2.drawStr((128 - w) / 2, 45, buf);
        }
        u8g2.sendBuffer();
      }
      break;

      // ------------------------------------
    // STATE: GAME RUNNING (THE CATCHER)
    // ------------------------------------
    case GAME_RUNNING:
      {
        unsigned long now = millis();

        // --- 1. MELODY LOGIC (Non-Blocking) ---
        // Only play if we are NOT in the "silence" period (just caught a block)
        if (now > silenceUntil) {
           if (now > nextNoteTime) {
             // Play the current note
             tone(buzzerPin, melody[noteIndex]);
             
             // Move to next note
             noteIndex++;
             if (noteIndex >= melodyLen) noteIndex = 0;
             
             // Set time for next note (Speed of song)
             nextNoteTime = now + 120; // 120ms per note (Fast!)
           }
        } else {
           // Ensure silence during the "caught" pause
           noTone(buzzerPin);
        }

        // --- 2. FLASHING LEDS ---
        if ((now / 100) % 2 == 0) {
           digitalWrite(led1, HIGH); digitalWrite(led2, LOW);
        } else {
           digitalWrite(led1, LOW); digitalWrite(led2, HIGH);
        }

        // --- 3. INPUT ---
        if (digitalRead(btnLeft) == LOW && catcherX > 0) {
          catcherX -= 8; 
        }
        if (digitalRead(btnRight) == LOW && catcherX < (128 - catcherWidth)) {
          catcherX += 8;
        }

        // --- 4. PHYSICS ---
        blockY += blockSpeed;

        // Collision Check
        if (blockY + 6 >= 64 - catcherHeight) {
          
          if (blockX + 6 > catcherX && blockX < catcherX + catcherWidth) {
            // CAUGHT!
            score++;
            blockY = 0; 
            blockX = random(2, 120);
            
            // --- SOUND FEEDBACK: SILENCE ---
            noTone(buzzerPin);
            silenceUntil = now + 200; // Silence for 200ms to indicate success
            
            // Harder!
            if (score % 5 == 0) blockSpeed += 0.8;

            // WIN?
            if (score >= WIN_SCORE) {
              resetToStudying();
              return;
            }

          } else if (blockY >= 64) {
            // MISSED!
            score = 0;        // Reset Score
            blockSpeed = 1.5; // Reset Speed
            blockY = 0;
            blockX = random(2, 120);
            
            // Optional: Play a "Fail" low tone immediately?
            // For now, the melody just keeps looping, which is stressful enough!
          }
        }

        // --- 5. DRAW ---
        u8g2.clearBuffer();
        u8g2.drawBox(catcherX, 64 - catcherHeight, catcherWidth, catcherHeight);
        u8g2.drawBox((int)blockX, (int)blockY, 6, 6);
        
        u8g2.setFont(u8g2_font_ncenB10_tr);
        u8g2.setCursor(0, 15);
        u8g2.print("Catch: ");
        u8g2.print(score);
        u8g2.print("/");
        u8g2.print(WIN_SCORE);
        
        u8g2.sendBuffer();
        
        delay(20); 
      }
      break;

    // --- GAME ---
    // case GAME_RUNNING:
    //   {
    //     // Alert Sound/Lights
    //     digitalWrite(buzzerPin, HIGH); 
    //     if ((millis() / 100) % 2 == 0) {
    //        digitalWrite(led1, HIGH); digitalWrite(led2, LOW);
    //     } else {
    //        digitalWrite(led1, LOW); digitalWrite(led2, HIGH);
    //     }

    //     // Input
    //     if (digitalRead(btnLeft) == LOW && catcherX > 0) {
    //       catcherX -= 8; // Increased speed slightly
    //     }
    //     if (digitalRead(btnRight) == LOW && catcherX < (128 - catcherWidth)) {
    //       catcherX += 8;
    //     }

    //     // Physics
    //     blockY += blockSpeed;

    //     // Collision Check
    //     if (blockY + 6 >= 64 - catcherHeight) {
          
    //       if (blockX + 6 > catcherX && blockX < catcherX + catcherWidth) {
    //         // CAUGHT!
    //         score++;
    //         blockY = 0; 
    //         blockX = random(2, 120);
            
    //         digitalWrite(buzzerPin, LOW); // Momentary silence
            
    //         // Harder!
    //         if (score % 5 == 0) blockSpeed += 0.5;

    //         // WIN?
    //         if (score >= WIN_SCORE) {
    //           resetToStudying();
    //           return;
    //         }

    //       } else if (blockY >= 64) {
    //         // MISSED!
    //         score = 0;       // Reset Score
    //         blockSpeed = 1.5; // Reset Speed
    //         blockY = 0;
    //         blockX = random(2, 120);
    //       }
    //     }

    //     // Draw
    //     u8g2.clearBuffer();
    //     u8g2.drawBox(catcherX, 64 - catcherHeight, catcherWidth, catcherHeight);
    //     u8g2.drawBox((int)blockX, (int)blockY, 6, 6);
        
    //     u8g2.setFont(u8g2_font_ncenB10_tr);
    //     u8g2.setCursor(0, 15);
    //     u8g2.print("Catch: ");
    //     u8g2.print(score);
    //     u8g2.print("/");
    //     u8g2.print(WIN_SCORE);
        
    //     u8g2.sendBuffer();
    //     delay(20); 
    //   }
    //   break;
  }
}