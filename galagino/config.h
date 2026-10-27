#ifndef _CONFIG_H_
#define _CONFIG_H_

#if defined(CONFIG_IDF_TARGET_ESP32S3)
// The ESP32-S3 has no built-in DAC, so use the external ES8311 codec.
#ifndef TFT_INVERT
#define TFT_INVERT
#endif
#ifndef ES8311_AUDIO
#define ES8311_AUDIO
#endif
#ifndef ES8311_I2C_CLK
  #define ES8311_I2C_CLK 400000
#endif
#ifndef ES8311_I2C_ADDR
  #define ES8311_I2C_ADDR 0x18
#endif
#ifndef ES8311_I2C_SDA
  #define ES8311_I2C_SDA 16
#endif
#ifndef ES8311_I2C_SCL
  #define ES8311_I2C_SCL 15
#endif
#ifndef ES8311_I2S_MCK
  #define ES8311_I2S_MCK 4
#endif
#ifndef ES8311_I2S_BCK
  #define ES8311_I2S_BCK 5
#endif
#ifndef ES8311_I2S_DIN
  #define ES8311_I2S_DIN 6
#endif
#ifndef ES8311_I2S_DOUT
  #define ES8311_I2S_DOUT 8
#endif
#ifndef ES8311_I2S_WS
  #define ES8311_I2S_WS 7
#endif
#ifndef AUDIO_ENABLE_PIN
  #define AUDIO_ENABLE_PIN 1
#endif
#endif

#ifndef TFT_SPI_HOST
  #define TFT_SPI_HOST SPI2_HOST
#endif

// disable e.g. if roms are missing
#define ENABLE_PACMAN
#define ENABLE_GALAGA
#define ENABLE_DKONG
#define ENABLE_FROGGER
#define ENABLE_DIGDUG
#define ENABLE_1942

#if !defined(ENABLE_PACMAN) && !defined(ENABLE_GALAGA) && !defined(ENABLE_DKONG) && !defined(ENABLE_FROGGER) && !defined(ENABLE_DIGDUG) && !defined(ENABLE_1942)
#error "At least one machine has to be enabled!"
#endif

// check if only one machine is enabled
#if (( defined(ENABLE_PACMAN) && !defined(ENABLE_GALAGA) && !defined(ENABLE_DKONG) && !defined(ENABLE_FROGGER) && !defined(ENABLE_DIGDUG) && !defined(ENABLE_1942)) || \
     (!defined(ENABLE_PACMAN) &&  defined(ENABLE_GALAGA) && !defined(ENABLE_DKONG) && !defined(ENABLE_FROGGER) && !defined(ENABLE_DIGDUG) && !defined(ENABLE_1942)) || \
     (!defined(ENABLE_PACMAN) && !defined(ENABLE_GALAGA) &&  defined(ENABLE_DKONG) && !defined(ENABLE_FROGGER) && !defined(ENABLE_DIGDUG) && !defined(ENABLE_1942)) || \
     (!defined(ENABLE_PACMAN) && !defined(ENABLE_GALAGA) && !defined(ENABLE_DKONG) &&  defined(ENABLE_FROGGER) && !defined(ENABLE_DIGDUG) && !defined(ENABLE_1942)) || \
     (!defined(ENABLE_PACMAN) && !defined(ENABLE_GALAGA) && !defined(ENABLE_DKONG) && !defined(ENABLE_FROGGER) &&  defined(ENABLE_DIGDUG) && !defined(ENABLE_1942)) || \
     (!defined(ENABLE_PACMAN) && !defined(ENABLE_GALAGA) && !defined(ENABLE_DKONG) && !defined(ENABLE_FROGGER) && !defined(ENABLE_DIGDUG) &&  defined(ENABLE_1942)))
  #define SINGLE_MACHINE
#endif

// game config

#define MASTER_ATTRACT_MENU_TIMEOUT  20000   // start games randomly while sitting idle in menu for 20 seconds, undefine to disable

#include "dip_switches.h"

//#define CHEAP_YELLOW_DISPLAY_CONF

#ifndef CHEAP_YELLOW_DISPLAY_CONF // Config as it was before

// video config
#define TFT_SPICLK  40000000    // 40 Mhz. Some displays cope with 80 Mhz

#define TFT_CS  10
#define TFT_DC  46
#define TFT_RST -1
#define TFT_BL  45    // don't set if backlight is hard wired
#define TFT_ILI9341   // define for ili9341, otherwise st7789
#define TFT_VFLIP     // define for upside down

// x and y offset of 224x288 pixels inside the 240x320 screen
#define TFT_X_OFFSET  8
#define TFT_Y_OFFSET 16

#define LED_PIN        16   // pin used for optional WS2812 stripe
#define LED_BRIGHTNESS 50   // range 0..255

// audio config
#define MASTER_VOLUME  64   // master volume scaler, default 64 (max), range 1-64
// #define SND_DIFF   // set to output differential audio on GPIO25 _and_ inverted on GPIO26

// Pins used for buttons
#define BTN_START_PIN  35
#define BTN_COIN_PIN   35   // if this is not defined, then start will act as coin & start
#define BTN_LEFT_PIN   35
#define BTN_RIGHT_PIN  35
#define BTN_DOWN_PIN   35
#define BTN_UP_PIN     35
#define BTN_FIRE_PIN   35

#else                       // Cheap Yellow Display Conf

// video config
#define TFT_SPICLK 40000000L // 40 Mhz. Some displays cope with 80 Mhz

#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12

// #define TFT_MAC  0x20  // some CYD need this to rotate properly and have correct colors

// x and y offset of 224x288 pixels inside the 240x320 screen
#define TFT_X_OFFSET 8
#define TFT_Y_OFFSET 16

#define NUNCHUCK_INPUT

#define NUNCHUCK_SDA 22
#define NUNCHUCK_SCL 27

#define NUNCHUCK_MOVE_THRESHOLD 30 // This is the dead-zone for where minor movements on the stick will not be considered valid movements

// Pins used for buttons
#define BTN_START_PIN 0 //This is the "boot" button
// #define BTN_COIN_PIN 21 // if this is not defined, then start will act as coin & start

#endif // #ifndef CHEAP_YELLOW_DISPLAY_CONF

#endif // _CONFIG_H_
