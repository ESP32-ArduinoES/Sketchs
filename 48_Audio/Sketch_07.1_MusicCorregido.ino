
#include <Arduino.h>
#include "FS.h"
#include "SD_MMC.h"
#include "SPI.h"
#include "es8311.h"
#include "Audio.h"
#include "Wire.h"
#include "demo_music.h"


#define FNK0104AB_2P8_240x320_ILI9341   // Selecciona aqui el modelo de placa para usar su mapa de pines.

#define SD_SCK 38                       // Pines de microSD para FNK0104AB_2P8_240x320_ILI9341.
#define SD_CMD 40
#define SD_D0 39
#define SD_D1 41
#define SD_D2 48
#define SD_D3 47

#define I2S_MCK 4                       // Pines de audio I2S.
#define I2S_BCK 5
#define I2S_DINT 6
#define I2S_DOUT 8
#define I2S_WS 7
#define AP_ENABLE 1
#define I2C_SCL 15                      // Bus de control I2C.
#define I2C_SDA 16
#define I2C_SPEED 400000

Audio audio;

 /* =======================================================
    0. Configura AP_ENABLE como salida (LOW). Amplificador.
    Prepara I2C para configurar el ES8311.
    =======================================================
 */
 
bool driver_es8311_init(void) {
  pinMode(AP_ENABLE, OUTPUT);                       // Pin de salida
  digitalWrite(AP_ENABLE, LOW);                     // Estado del Pin en LOW  (HIGH/LOW)

  if (!Wire.begin(I2C_SDA, I2C_SCL, I2C_SPEED)) {
    Serial.println("I2C initialization failed!");   // Inicio comunicación I2C
    return false;
  }

  return true;
}

void setup() {
  Serial.begin(115200);                   // Baudios del "Monitor Serie"
  
 /* ==============================================================
    1. Configura los pines e inicializa la microSD en modo SD_MMC.
    ==============================================================
 */
 
  if (!SD_MMC.setPins(SD_SCK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3)) {
    Serial.println("Pin change failed!");
    return;
  }
  while (!SD_MMC.begin()) {
    Serial.println("SD card does not exist,please insert SD card.");
    delay(100);
  }
 /* ==================================================
    2. Inicia el bus I2C 
    Se usara para escribir la configuracion del codec.
    ==================================================
 */
 
  if (!driver_es8311_init()) {
    return;
  }

 /* ===================================================================
   3. Inicializa el ES8311. es8311_codec_init() configura el reloj,
   el audio a 44,1 kHz/16 bits y el volumen del codec (ver es8311.cpp).
   Configura parámetros como los relojes, el formato de audio y el DAC.
    ===================================================================
 */
  
  if (es8311_codec_init() != ESP_OK) {
    Serial.println("ES8311 init failed!");
    return;
  }

 /* =======================================================================
    4. Indica a la libreria Audio los pines I2S que llevan el audio al DAC.
    =======================================================================
 */
 
  audio.setPinout(I2S_BCK, I2S_WS, I2S_DOUT, I2S_MCK);
  audio.setVolume(10);                                      // Volumen de reproduccion configurado en la libreria Audio.
 
 /* ========================================================================
    5. Busca archivos MP3 en la microSD (raiz y un nivel de subdirectorios).
    ========================================================================
 */

  if (!demo_music()) {
    return;
  }
 /* ====================================================
    6. Reproduce la primera pista encontrada (indice 0).
    ====================================================
 */

  if (!demo_music_play(0)) {
    Serial.println("Music playback could not be started.");
  }
}

void loop() {  
  audio.loop();                                              // Atiende continuamente al decodificador y mantiene la reproduccion.
}
