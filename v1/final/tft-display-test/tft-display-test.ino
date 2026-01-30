#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ===== Backlight =====
#define TFT_BL 3   // BL connected to GPIO 3

// ===== TFT pins =====
#define TFT_CS   7
#define TFT_DC   6   // C/S on display
#define TFT_RST  8

#define TFT_SCK  5
#define TFT_MOSI 4

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Brightness function (0–255)
void setBrightness(uint8_t level) {
  ledcWrite(TFT_BL, level);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // ---- Backlight PWM (NEW API) ----
  ledcAttach(TFT_BL, 5000, 8);   // pin, freq, resolution
  setBrightness(180);

  // ---- SPI ----
  SPI.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);

  // ---- TFT ----
  tft.initR(INITR_BLACKTAB);   // try REDTAB/GREENTAB if needed
  tft.setRotation(1);

  tft.fillScreen(ST77XX_RED);
  delay(300);
  tft.fillScreen(ST77XX_GREEN);
  delay(300);
  tft.fillScreen(ST77XX_BLUE);
  delay(300);
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 40);
  tft.println("ESP32-S3");
  tft.setCursor(10, 70);
  tft.println("ST7735 OK");

  Serial.println("TFT + PWM brightness OK");
}

void loop() {
  for (int i = 0; i <= 255; i++) {
    setBrightness(i);
    delay(5);
  }
  for (int i = 255; i >= 0; i--) {
    setBrightness(i);
    delay(5);
  }
  
}
