#include <Arduino.h>
#include <SPIFFS.h>
#include <driver/i2s.h>

#define I2S_BCLK 5
#define I2S_LRC  6
#define I2S_DIN  4

File wavFile;

void setup() {
  Serial.begin(115200);

  // SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
    while (1);
  }

  // I2S config
  i2s_config_t i2s_config;
  memset(&i2s_config, 0, sizeof(i2s_config));

  i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  i2s_config.sample_rate = 44100;
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  i2s_config.communication_format = I2S_COMM_FORMAT_I2S;
  i2s_config.dma_buf_count = 8;
  i2s_config.dma_buf_len = 64;

  i2s_pin_config_t pin_config;
  memset(&pin_config, 0, sizeof(pin_config));
  pin_config.bck_io_num = I2S_BCLK;
  pin_config.ws_io_num = I2S_LRC;
  pin_config.data_out_num = I2S_DIN;
  pin_config.data_in_num = I2S_PIN_NO_CHANGE;

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  playWav("/test.wav");
}

void loop() {
  // play once
}

void playWav(const char *filename) {
  wavFile = SPIFFS.open(filename, "r");
  if (!wavFile) {
    Serial.println("Failed to open WAV");
    return;
  }

  // Skip WAV header (44 bytes)
  wavFile.seek(44);

  uint8_t buffer[512];
  size_t bytesWritten;

  while (wavFile.available()) {
    int bytesRead = wavFile.read(buffer, sizeof(buffer));
    i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  }

  wavFile.close();
}
