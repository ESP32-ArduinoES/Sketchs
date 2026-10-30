#include "leds.h"

#ifdef NUM_LEDS

CRGB leds[NUM_LEDS];

void leds_init(void) {
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(LED_BRIGHTNESS);
}

void leds_update(void) {
  static unsigned char frame_count = 0;
  static unsigned char position = 0;

  if(frame_count++ == 4) {
    frame_count = 0;
    const unsigned char light = position < NUM_LEDS
      ? position
      : (2 * NUM_LEDS - 2) - position;

    for(unsigned char i = 0; i < NUM_LEDS; i++)
      leds[i] = (i == light) ? LED_YELLOW : LED_BLUE;

    position = (position + 1) % (2 * NUM_LEDS - 2);
  }

  FastLED.show();
}

#endif
