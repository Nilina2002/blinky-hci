#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// Constructor for SSD1306 128x64 using I2C with custom pins
// U8G2_SSD1306_128X64_NONAME_F_SW_I2C(rotation, clock, data, reset)
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, 17, 18, U8X8_PIN_NONE);

// Buttons
const int btnLeft = 11;
const int btnRight = 10;
const int btnWake = 37;

// Game Variables
int blockX;
int blockY = 0;
int blockSpeed = 3;
int catcherX = 54;
const int catcherWidth = 20;
const int catcherHeight = 4;
int score = 0;
bool gameRunning = true;

void resetBlock() {
  blockY = 0;
  blockX = random(0, 122); // 128 - block width
}

void resetGame() {
  score = 0;
  blockSpeed = 3;
  gameRunning = true;
  resetBlock();
}

void setup() {
  pinMode(btnLeft, INPUT_PULLUP);
  pinMode(btnRight, INPUT_PULLUP);
  pinMode(btnWake, INPUT_PULLUP);

  u8g2.begin();
  randomSeed(analogRead(0));
  resetGame();
}

void loop() {
  if (gameRunning) {
    // 1. Handle Input
    if (digitalRead(btnLeft) == LOW && catcherX > 0) catcherX -= 5;
    if (digitalRead(btnRight) == LOW && catcherX < (128 - catcherWidth)) catcherX += 5;

    // 2. Update Physics
    blockY += blockSpeed;

    // Collision Check
    if (blockY + 4 >= 64 - catcherHeight) {
      if (blockX + 4 > catcherX && blockX < catcherX + catcherWidth) {
        score++;
        resetBlock();
        if (score % 5 == 0) blockSpeed++; 
      } else if (blockY >= 64) {
        gameRunning = false;
      }
    }

    // 3. Draw Game
    u8g2.clearBuffer();					
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(0, 10);
    u8g2.print("Score: ");
    u8g2.print(score);

    u8g2.drawBox(blockX, blockY, 4, 4); // Falling block
    u8g2.drawBox(catcherX, 64 - catcherHeight, catcherWidth, catcherHeight); // Paddle
    u8g2.sendBuffer();					

  } else {
    // Game Over Screen
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(15, 30, "GAME OVER");
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(40, 50);
    u8g2.print("Score: ");
    u8g2.print(score);
    u8g2.sendBuffer();

    if (digitalRead(btnWake) == LOW) {
      resetGame();
      delay(200);
    }
  }
  delay(20); 
}