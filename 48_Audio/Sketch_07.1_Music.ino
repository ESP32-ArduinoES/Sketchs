/*
* @ File:   Sketch_07.1_Music.ino
* @ Author: [Eason Shen]
* @ Date:   [2026-05-15]
*/

#include <Arduino.h>
#include "SD_MMC.h"
#include "es8311.h"
#include "Audio.h"
#include "ESP_I2S.h"
#include "demo_music.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

#define FNK0104AB_2P8_240x320_ILI9341
//#define FNK0104N_3P5_320x480_ST77922
//#define FNK0104S_4P0_320x480_ST7796

#ifdef FNK0104N_3P5_320x480_ST77922
 //ESP32-S3 IO Pin define
 #define SD_SCK 5
 #define SD_CMD 4
 #define SD_D0 6
 #define SD_D1 7
 #define SD_D2 2
 #define SD_D3 3
 
 //I2S IO Pin define
 #define I2S_MCK 17
 #define I2S_BCK 18
 #define I2S_DINT 16
 #define I2S_DOUT 15
 #define I2S_WS 21
 #define AP_ENABLE 1
 #define I2C_SCL 39        /*!< GPIO number used for I2C master clock */
 #define I2C_SDA 38        /*!< GPIO number used for I2C master data  */
 #define I2C_SPEED 400000  /*!< I2C master clock frequency */
#else
 //ESP32-S3 IO Pin define
 #define SD_SCK 38
 #define SD_CMD 40
 #define SD_D0 39
 #define SD_D1 41
 #define SD_D2 48
 #define SD_D3 47
 
 //I2S IO Pin define
 #define I2S_MCK 4
 #define I2S_BCK 5
 #define I2S_DINT 6
 #define I2S_DOUT 8
 #define I2S_WS 7
 #define AP_ENABLE 1
 #define I2C_SCL 15        /*!< GPIO number used for I2C master clock */
 #define I2C_SDA 16        /*!< GPIO number used for I2C master data  */
 #define I2C_SPEED 400000  /*!< I2C master clock frequency */
#endif

Audio audio;
I2SClass es8311_i2s;

bool audio_hardware_init(void) {
  pinMode(AP_ENABLE, OUTPUT);
  digitalWrite(AP_ENABLE, LOW);

  i2c_config_t i2c_config = {};
  i2c_config.mode = I2C_MODE_MASTER;
  i2c_config.sda_io_num = static_cast<gpio_num_t>(I2C_SDA);
  i2c_config.scl_io_num = static_cast<gpio_num_t>(I2C_SCL);
  i2c_config.sda_pullup_en = GPIO_PULLUP_ENABLE;
  i2c_config.scl_pullup_en = GPIO_PULLUP_ENABLE;
  i2c_config.master.clk_speed = I2C_SPEED;

  if (i2c_param_config(I2C_NUM_0, &i2c_config) != ESP_OK ||
      i2c_driver_install(I2C_NUM_0, i2c_config.mode, 0, 0, 0) != ESP_OK) {
    Serial.println("Failed to initialize I2C!");
    return false;
  }

  es8311_i2s.setPins(I2S_BCK, I2S_WS, I2S_DOUT, I2S_DINT, I2S_MCK);
  if (!es8311_i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_LEFT)) {
    Serial.println("Failed to initialize I2S bus!");
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);

  //SD card init
  if (!SD_MMC.setPins(SD_SCK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3)) {
    Serial.println("Pin change failed!");
    return;
  }
  while (!SD_MMC.begin()) {
    Serial.println("SD card does not exist,please insert SD card.");
    delay(100);
  }

  if (!audio_hardware_init() || es8311_codec_init() != ESP_OK) {
    return;
  }

  audio.setPinout(I2S_BCK, I2S_WS, I2S_DOUT, I2S_MCK);
  audio.setVolume(2);
  if (!demo_music()) {
    return;
  }

  demo_music_play(1);
}

void loop() {
  audio.loop();
}
