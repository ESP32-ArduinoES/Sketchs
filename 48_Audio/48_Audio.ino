
#include <Arduino.h>
#include "driver_sdmmc.h"
#include "es8311.h"
#include "Audio.h"
#include "Wire.h"
#include "ESP_I2S.h"
#include "demo_music.h"

#define FNK0104AB_2P8_240x320_ILI9341                       // Selección de la placa / pantalla compatible.
//#define FNK0104N_3P5_320x480_ST77922
//#define FNK0104S_4P0_320x480_ST7796

#define AP_ENABLE 1                                         // Habilita la alimentación del amplificador / audio.
#define I2C_SPEED 400000                                    // Velocidad del bus I2C para el codec ES8311.

#ifdef FNK0104N_3P5_320x480_ST77922
 #define SD_SCK 5
 #define SD_CMD 4
 #define SD_D0 6
 #define SD_D1 7
 #define SD_D2 2
 #define SD_D3 3

 #define I2S_MCK 17
 #define I2S_BCK 18
 #define I2S_DINT 16
 #define I2S_DOUT 15
 #define I2S_WS 21
 #define I2C_SCL 39
 #define I2C_SDA 38
#else
 #define SD_SCK 38                                          // Pines tarjeta SD -> FNK0104AB_2P8_240x320_ILI9341
 #define SD_CMD 40
 #define SD_D0 39
 #define SD_D1 41
 #define SD_D2 48
 #define SD_D3 47
 
 #define I2S_MCK 4                                          // Pines comunicacion I2S -> FNK0104AB_2P8_240x320_ILI9341
 #define I2S_BCK 5
 #define I2S_DINT 6
 #define I2S_DOUT 8
 #define I2S_WS 7
 
 #define I2C_SCL 15                                         // Pines comunicación I2C-> FNK0104AB_2P8_240x320_ILI9341
 #define I2C_SDA 16        
#endif


Audio audio;                                                // Objeto principal para controlar la reproducción de audio.

I2SClass es8311_i2s;                                        // Objeto del bus I2S usado por el codec ES8311.

// Inicializa el codec ES8311 y el bus I2S asociado.
void driver_es8311_init(void) {
  
  pinMode(AP_ENABLE, OUTPUT);                               // El pin AP_ENABLE activa la alimentación del amplificador.
  digitalWrite(AP_ENABLE, LOW);                             // Amplificador apagado.
  
  Wire.begin(I2C_SDA, I2C_SCL, I2C_SPEED);                  // Inicia el bus I2C con los pines SDA/SCL definidos.

// Configura el hardware I2S para el codec ES8311.
  es8311_i2s.setPins(I2S_BCK, I2S_WS, I2S_DOUT, I2S_DINT, I2S_MCK);
  if (!es8311_i2s.begin(I2S_MODE_STD, 44100, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_LEFT)) {
    Serial.println("Failed to initialize I2S bus!");
  }
}

void setup() { 
  Serial.begin(115200);                                     // Inicia la comunicación serial para depuración y mensajes.

  sdmmc_init(SD_SCK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3);   // Inicializa la tarjeta SD usando los pines configurados.

  driver_es8311_init();                                     // Inicializa el codec ES8311 antes de usar el audio.
  if (es8311_codec_init() != ESP_OK) {
    Serial.println("ES8311 init failed!");
    return;
  }
  
  audio.setPinout(I2S_BCK, I2S_WS, I2S_DOUT, I2S_MCK);      // Configura la salida de audio del objeto Audio.
  audio.setVolume(2);                                       // Ajusta el volumen.

  // Carga la música de demostración desde el archivo demo_music.h.
  if (!demo_music()) {
    return;
  }
  
  demo_music_play(0);                                       // Reproduce la pista 0 de la demo.
}

void loop() {
  
  audio.loop();                                             // Mantiene el sistema de audio funcionando y procesando eventos de reproducción.
}
