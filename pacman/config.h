/*
  ============================================================================
  GALAGINO — config.h
  ============================================================================
  CONFIGURACIÓN CENTRAL DEL PROYECTO. Aquí se decide:
    - Qué placa se usa (ESP32-S3, CYD, ESP32 clásico).
    - La configuración de Pac-Man en modo de juego único.
    - Pines de vídeo, audio, botones, LEDs.
    - Configuración de audio (ES8311, DAC interno, SND_DIFF, etc.).
  ============================================================================
*/

#ifndef _CONFIG_H_
#define _CONFIG_H_


/* ==========================================================================
   CONFIGURACIÓN PARA ESP32-S3
   El ESP32-S3 no tiene DAC interno. Si se compila para S3, hay que usar
   obligatoriamente el códec externo ES8311.
   ========================================================================== */

#if defined(CONFIG_IDF_TARGET_ESP32S3)

// El S3 no tiene DAC interno -> usar el códec ES8311 externo.
#ifndef ES8311_AUDIO
#define ES8311_AUDIO
#endif

// La pantalla del S3 necesita invertir colores.
#ifndef TFT_INVERT
#define TFT_INVERT
#endif

// Frecuencia del bus I2C del códec (400 kHz = Fast Mode).
#ifndef ES8311_I2C_CLK
  #define ES8311_I2C_CLK 400000
#endif

// Dirección I2C del ES8311 (0x18 por defecto en el datasheet).
#ifndef ES8311_I2C_ADDR
  #define ES8311_I2C_ADDR 0x18
#endif

// Pines I2C del códec.
#ifndef ES8311_I2C_SDA
  #define ES8311_I2C_SDA 16
#endif
#ifndef ES8311_I2C_SCL
  #define ES8311_I2C_SCL 15
#endif

// Pines I2S del códec.
#ifndef ES8311_I2S_MCK
  #define ES8311_I2S_MCK 4    // Master Clock
#endif
#ifndef ES8311_I2S_BCK
  #define ES8311_I2S_BCK 5    // Bit Clock
#endif
#ifndef ES8311_I2S_DIN
  #define ES8311_I2S_DIN 6    // Data In (no usado, solo reproducción)
#endif
#ifndef ES8311_I2S_DOUT
  #define ES8311_I2S_DOUT 8   // Data Out
#endif
#ifndef ES8311_I2S_WS
  #define ES8311_I2S_WS 7     // Word Select (LRCK)
#endif

// Pin de habilitación del amplificador (activo a nivel bajo).
#ifndef AUDIO_ENABLE_PIN
  #define AUDIO_ENABLE_PIN 1
#endif

#endif // CONFIG_IDF_TARGET_ESP32S3


/* ==========================================================================
   BUS SPI DE LA PANTALLA
   Por defecto se usa SPI2_HOST (el segundo bus SPI del ESP32).
   ========================================================================== */

#ifndef TFT_SPI_HOST
  #define TFT_SPI_HOST SPI2_HOST
#endif


/* ==========================================================================
   JUEGO HABILITADO
   Solo Pac-Man: usa una CPU y evita incluir los recursos y la lógica de los
   demás juegos.
   ========================================================================== */

#define ENABLE_PACMAN

/* ==========================================================================
   MODO DE JUEGO ÚNICO
   Este proyecto incluye solo Pac-Man, así que no necesita menú ni selección
   de máquina.
   ========================================================================== */

#define SINGLE_MACHINE

// Incluye la configuración de DIP switches de cada juego (monedas por
// crédito, dificultad, vidas, etc.). Está en otro header.
#include "dip_switches.h"


/* ==========================================================================
   SELECTOR DE CONFIGURACIÓN DE PANTALLA
   Por defecto: configuración "estándar" (ILI9341/ST7789 en placa propia).
   Si se define CHEAP_YELLOW_DISPLAY_CONF: configuración para la placa
   "Cheap Yellow Display" (CYD) con pantalla ST7789 y mando Nunchuck.
   ========================================================================== */

// Descomentar para usar la CYD:
//#define CHEAP_YELLOW_DISPLAY_CONF

#ifndef CHEAP_YELLOW_DISPLAY_CONF


/* ==========================================================================
   CONFIGURACIÓN ESTÁNDAR (pantalla ILI9341 o ST7789 en placa propia)
   ========================================================================== */

// Frecuencia del bus SPI de la pantalla.
// 40 MHz -> refresco a 30 Hz (VIDEO_HALF_RATE).
// 80 MHz -> refresco a 60 Hz completo. Algunas pantallas no lo soportan.
#define TFT_SPICLK  40000000

// Pines de control de la pantalla.
#define TFT_CS  10    // Chip Select
#define TFT_DC  46    // Data/Command
#define TFT_RST -1    // Reset (-1 = no conectado, se usa el reset del sistema)
#define TFT_BL  45    // Backlight. Comentar si el backlight está cableado fijo.

// Modelo de pantalla. Descomentar uno:
#define TFT_ILI9341   // ILI9341 (si no, se asume ST7789)
//#define TFT_ST7789

// Descomentar para girar la pantalla 180º.
#define TFT_VFLIP

// Offset de la zona de 224x288 dentro de la pantalla de 240x320.
// La pantalla tiene 240x320, pero el emulador usa 224x288, así que se
// centra con un margen de 8 píxeles a la izquierda y 16 arriba.
#define TFT_X_OFFSET  8
#define TFT_Y_OFFSET 16

// Pin del LED WS2812 opcional (tira de LEDs).
#ifndef LED_PIN
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
    #define LED_PIN 48
  #else
    #define LED_PIN 16
  #endif
#endif
#define LED_BRIGHTNESS 50   // Brillo, 0..255.

// Volumen maestro (escala la señal antes de enviarla al DAC/I2S).
// 64 = máximo, 1 = mínimo. Valores más bajos reducen el ruido.
#define MASTER_VOLUME  5

// Descomentar para salida de audio diferencial (GPIO25 positivo y GPIO26
// negativo). Mejora la calidad pero necesita amplificador diferencial.
// #define SND_DIFF

// Pines de los botones.
// ATENCIÓN: los 7 pines están a 35 por defecto. Eso NO es funcional:
// todos los botones comparten el mismo GPIO. Hay que cambiarlos a los
// pines reales de tu placa. Se dejan así porque el autor no quería
// imponer un pinout concreto.
#define BTN_START_PIN  3
#define BTN_COIN_PIN   2   // Si se comenta, Start actúa como Coin + Start.
#define BTN_LEFT_PIN   14
#define BTN_RIGHT_PIN  21
#define BTN_DOWN_PIN   35
#define BTN_UP_PIN     35
#define BTN_FIRE_PIN   35


#else // CHEAP_YELLOW_DISPLAY_CONF


/* ==========================================================================
   CONFIGURACIÓN PARA "CHEAP YELLOW DISPLAY" (CYD)
   La CYD es una placa barata con ESP32-WROVER, pantalla ST7789 de 2.8"
   y conector para mando Nunchuck. No tiene botones físicos de juego,
   así que se usa un Nunchuck.
   ========================================================================== */

// Frecuencia del bus SPI de la pantalla.
#define TFT_SPICLK 40000000L

// Pines SPI de la pantalla (en la CYD son fijos por hardware).
#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12

// Algunas CYD necesitan este ajuste para rotar bien y tener colores correctos.
// #define TFT_MAC  0x20

// Offset de la zona de 224x288 dentro de la pantalla de 240x320.
#define TFT_X_OFFSET 8
#define TFT_Y_OFFSET 16

// Usar mando Nunchuck como entrada (en vez de botones físicos).
#define NUNCHUCK_INPUT

// Pines I2C del Nunchuck (en la CYD son fijos).
#define NUNCHUCK_SDA 22
#define NUNCHUCK_SCL 27

// Zona muerta del joystick: movimientos menores a este umbral se ignoran.
// Evita que el personaje se mueva solo por ruido del ADC.
#define NUNCHUCK_MOVE_THRESHOLD 30

// Botón físico disponible: el botón "BOOT" del ESP32 (GPIO0).
#define BTN_START_PIN 0   // Botón BOOT de la placa.

// Botón de moneda opcional. Si se comenta, Start actúa como Coin + Start.
// #define BTN_COIN_PIN 21

#endif // CHEAP_YELLOW_DISPLAY_CONF


#endif // _CONFIG_H_
