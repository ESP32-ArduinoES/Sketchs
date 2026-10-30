#ifndef LEDS_H
#define LEDS_H

#include "config.h"

#ifdef LED_PIN 

#include <FastLED.h>

// seven LEDs of a 144 leds/m stripe are slightly less than 5cm
#define NUM_LEDS  7

#define LED_BLACK    CRGB::Black
#define LED_BLUE     CRGB::Blue
#define LED_YELLOW   CRGB::Yellow

extern void leds_init(void);
extern void leds_update(void);
  
#else

static inline void leds_init(void) {}
static inline void leds_update(void) {}

#endif // LED_PIN

struct sprite_S {
  unsigned char code, color, flags;
  short x, y; 
};

#endif // LEDS_H
