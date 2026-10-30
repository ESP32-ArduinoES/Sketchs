/*
  Galagino - Pac-Man only
  Minimal ESP32 build for a single-game Pac-Man cabinet.
*/

#include "config.h"
#ifdef ES8311_AUDIO
#include "driver/i2s_std.h"
#else
#include "driver/i2s.h"
#endif
#include "video.h"
#include "leds.h"
#include "emulation.h"
#include "tileaddr.h"

#ifdef ES8311_AUDIO
#include "es8311.h"

static constexpr int es8311_sample_rate_hz = 24000;
static constexpr int es8311_mclk_multiple = 256;
static bool audio_ready = false;
static i2s_chan_handle_t audio_tx_channel = NULL;
static es8311_handle_t es8311_codec = NULL;
static bool es8311_i2c_ready = false;

static void es8311_release_codec(void) {
  if (es8311_codec) {
    es8311_delete(es8311_codec);
    es8311_codec = NULL;
  }

  if (es8311_i2c_ready) {
    const esp_err_t error = i2c_driver_delete(I2C_NUM_0);
    if (error != ESP_OK)
      Serial.printf("ES8311: I2C cleanup failed (%d)\n", error);
    es8311_i2c_ready = false;
  }
}

static void es8311_cleanup_i2s(void) {
  if (audio_tx_channel) {
    const esp_err_t error = i2s_del_channel(audio_tx_channel);
    if (error != ESP_OK)
      Serial.printf("ES8311: I2S channel cleanup failed (%d)\n", error);
    audio_tx_channel = NULL;
  }
  es8311_release_codec();
}

static bool es8311_init_codec(void) {
  i2c_config_t i2c_config = {};
  i2c_config.mode = I2C_MODE_MASTER;
  i2c_config.sda_io_num = (gpio_num_t)ES8311_I2C_SDA;
  i2c_config.scl_io_num = (gpio_num_t)ES8311_I2C_SCL;
  i2c_config.sda_pullup_en = GPIO_PULLUP_ENABLE;
  i2c_config.scl_pullup_en = GPIO_PULLUP_ENABLE;
  i2c_config.master.clk_speed = ES8311_I2C_CLK;

  esp_err_t error = i2c_param_config(I2C_NUM_0, &i2c_config);
  if (error != ESP_OK) {
    Serial.printf("ES8311: I2C configuration failed (%d)\n", error);
    return false;
  }

  error = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
  if (error != ESP_OK) {
    Serial.printf("ES8311: I2C driver install failed (%d)\n", error);
    return false;
  }
  es8311_i2c_ready = true;

  es8311_codec = es8311_create(I2C_NUM_0, ES8311_I2C_ADDR);
  if (!es8311_codec) {
    Serial.println("ES8311: codec handle allocation failed");
    es8311_release_codec();
    return false;
  }

  const es8311_clock_config_t clock_config = {
    .mclk_inverted = false,
    .sclk_inverted = false,
    .mclk_from_mclk_pin = true,
    .mclk_frequency = es8311_sample_rate_hz * es8311_mclk_multiple,
    .sample_frequency = es8311_sample_rate_hz
  };

  error = es8311_init(es8311_codec, &clock_config,
                      ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
  if (error != ESP_OK) {
    Serial.printf("ES8311: codec initialization failed (%d)\n", error);
    es8311_release_codec();
    return false;
  }

  error = es8311_voice_volume_set(es8311_codec, 80, NULL);
  if (error != ESP_OK) {
    Serial.printf("ES8311: volume configuration failed (%d)\n", error);
    es8311_release_codec();
    return false;
  }

  Serial.println("ES8311: codec initialized");
  return true;
}
#endif

#define IO_EMULATION

unsigned char active_sprites = 0;
struct sprite_S *sprite;
unsigned short *frame_buffer;

#ifdef NUNCHUCK_INPUT
#include "Nunchuck.h"
#endif

#include "pacman.h"

Video tft = Video();
TaskHandle_t emulationtask;

static unsigned long snd_cnt[3] = {0, 0, 0};
static unsigned long snd_freq[3];
static const signed char *snd_wave[3];
static unsigned char snd_volume[3];

#ifdef SND_DIFF
static unsigned short snd_buffer[128];
#else
static unsigned short snd_buffer[64];
#endif

#if defined(ES8311_AUDIO) && defined(SND_DIFF)
#error "ES8311_AUDIO does not support SND_DIFF"
#endif

void snd_render_buffer(void) {
  for (int i = 0; i < 64; ++i) {
    short v = 0;

    for (int ch = 0; ch < 3; ++ch) {
      if (snd_volume[ch])
        v += snd_volume[ch] * snd_wave[ch][(snd_cnt[ch] >> 13) & 0x1f];
    }

    v *= MASTER_VOLUME;

#ifdef ES8311_AUDIO
    snd_buffer[i] = (unsigned short)v;
#elif defined(SND_DIFF)
    snd_buffer[2 * i] = 0x8000 + v;
    snd_buffer[2 * i + 1] = 0x8000 - v;
#else
    snd_buffer[i ^ 1] = 0x8000 + v;
#endif

    for (int ch = 0; ch < 3; ++ch)
      snd_cnt[ch] += snd_freq[ch];
  }
}

void audio_namco_waveregs_parse(void) {
  for (int ch = 0; ch < 3; ++ch) {
    snd_volume[ch] = soundregs[ch * 5 + 0x15];

    if (snd_volume[ch]) {
      snd_freq[ch] = (ch == 0) ? soundregs[0x10] : 0;
      snd_freq[ch] += soundregs[ch * 5 + 0x11] << 4;
      snd_freq[ch] += soundregs[ch * 5 + 0x12] << 8;
      snd_freq[ch] += soundregs[ch * 5 + 0x13] << 12;
      snd_freq[ch] += soundregs[ch * 5 + 0x14] << 16;
      snd_wave[ch] = pacman_wavetable[soundregs[ch * 5 + 0x05] & 0x0f];
    }
  }
}

static void snd_prepare_buffer(void) {
  audio_namco_waveregs_parse();
  snd_render_buffer();
}

void snd_transmit(void) {
#ifdef ES8311_AUDIO
  if (!audio_ready) return;
#endif

  size_t bytesOut = 0;
  do {
#ifdef ES8311_AUDIO
    const esp_err_t write_error =
        i2s_channel_write(audio_tx_channel, snd_buffer, sizeof(snd_buffer), &bytesOut, 0);
    if (write_error != ESP_OK && write_error != ESP_ERR_TIMEOUT) {
      Serial.printf("ES8311: I2S write failed (%d)\n", write_error);
      audio_ready = false;
      return;
    }
#else
    i2s_write(I2S_NUM_0, snd_buffer, sizeof(snd_buffer), &bytesOut, 0);
#endif

    if (bytesOut) {
      snd_prepare_buffer();
    }
  } while (bytesOut);
}

void audio_init(void) {
  snd_prepare_buffer();

#ifndef ES8311_AUDIO
  static const i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
    .sample_rate = 24000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
#ifdef SND_DIFF
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
#elif defined(SND_LEFT_CHANNEL)
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
#else
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
#endif
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = 64,
#ifndef WORKAROUND_I2S_APLL_PROBLEM
    .use_apll = true
#else
    .use_apll = false
#endif
  };
#endif

#ifdef ES8311_AUDIO
  if (!es8311_init_codec()) return;

  i2s_chan_config_t channel_config =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  channel_config.dma_desc_num = 4;
  channel_config.dma_frame_num = 64;

  esp_err_t i2s_error = i2s_new_channel(&channel_config, &audio_tx_channel, NULL);
  if (i2s_error != ESP_OK) {
    Serial.printf("ES8311: I2S channel allocation failed (%d)\n", i2s_error);
    es8311_cleanup_i2s();
    return;
  }

  const i2s_std_config_t i2s_config = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(es8311_sample_rate_hz),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
        I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = (gpio_num_t)ES8311_I2S_MCK,
      .bclk = (gpio_num_t)ES8311_I2S_BCK,
      .ws = (gpio_num_t)ES8311_I2S_WS,
      .dout = (gpio_num_t)ES8311_I2S_DOUT,
      .din = I2S_GPIO_UNUSED,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false
      }
    }
  };

  i2s_error = i2s_channel_init_std_mode(audio_tx_channel, &i2s_config);
  if (i2s_error != ESP_OK) {
    Serial.printf("ES8311: I2S channel configuration failed (%d)\n", i2s_error);
    es8311_cleanup_i2s();
    return;
  }

  i2s_error = i2s_channel_enable(audio_tx_channel);
  if (i2s_error != ESP_OK) {
    Serial.printf("ES8311: I2S channel enable failed (%d)\n", i2s_error);
    es8311_cleanup_i2s();
    return;
  }
  audio_ready = true;
  Serial.println("ES8311: I2S audio ready");
#ifdef AUDIO_ENABLE_PIN
  pinMode(AUDIO_ENABLE_PIN, OUTPUT);
  digitalWrite(AUDIO_ENABLE_PIN, LOW);
#endif
#else
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
#ifdef SND_DIFF
  i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
#elif defined(SND_LEFT_CHANNEL)
  i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
#else
  i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN);
#endif
#endif
}

static void render_audio_chunk(int first_row, int row_count) {
  for (int row = first_row; row < first_row + row_count; ++row) {
    pacman_render_row(row);
    tft.write(frame_buffer, 224 * 8);
  }
  snd_transmit();
}

void update_screen(void) {
  uint32_t t0 = micros();
  pacman_prepare_frame();

#if TFT_SPICLK < 80000000
  for (int half = 0; half < 2; ++half) {
    for (int c = 18 * half; c < 18 * (half + 1); c += 3) {
      render_audio_chunk(c, 3);
    }

    uint32_t elapsed = (micros() - t0) / 1000;
    if (elapsed < (half ? 33 : 16)) vTaskDelay((half ? 33 : 16) - elapsed);
    else if (half) vTaskDelay(1);

    xTaskNotifyGive(emulationtask);
  }
#else
  for (int c = 0; c < 36; c += 6) {
    render_audio_chunk(c, 6);
  }

  uint32_t elapsed = (micros() - t0) / 1000;
  if (elapsed < 16) vTaskDelay(16 - elapsed);
  else vTaskDelay(1);

  xTaskNotifyGive(emulationtask);
#endif
}

void emulation_task(void *p) {
  prepare_emulation();
  while (1) emulate_frame();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Galagino");
  setCpuFrequencyMhz(240);

  frame_buffer = (unsigned short *)malloc(224 * 8 * 2);
  sprite = (struct sprite_S *)malloc(128 * sizeof(struct sprite_S));

  pinMode(BTN_START_PIN, INPUT_PULLUP);
#ifdef BTN_COIN_PIN
  pinMode(BTN_COIN_PIN, INPUT_PULLUP);
#endif

#ifdef NUNCHUCK_INPUT
  nunchuckSetup();
#else
  pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
  pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_FIRE_PIN, INPUT_PULLUP);
#endif

  audio_init();

#ifdef LED_PIN
  leds_init();
#endif

  xTaskCreatePinnedToCore(emulation_task, "emulation task", 4096, NULL, 2, &emulationtask, 0);
  tft.begin();
}

unsigned char buttons_get(void) {
#ifdef NUNCHUCK_INPUT
  const unsigned char nunchuck_buttons = getNunchuckInput();
  const unsigned char inputs = (nunchuck_buttons & ~BUTTON_EXTRA) |
                               ((nunchuck_buttons & BUTTON_EXTRA) ? BUTTON_COIN : 0);
#endif

  unsigned char startAndCoinState =
#ifdef BTN_COIN_PIN
      (digitalRead(BTN_START_PIN) ? 0 : BUTTON_START) |
      (digitalRead(BTN_COIN_PIN) ? 0 : BUTTON_COIN);
#else
      (digitalRead(BTN_START_PIN) ? 0 : (BUTTON_COIN | BUTTON_START));
#endif

#ifdef NUNCHUCK_INPUT
  return startAndCoinState | inputs;
#else
  return startAndCoinState |
         (digitalRead(BTN_LEFT_PIN) ? 0 : BUTTON_LEFT) |
         (digitalRead(BTN_RIGHT_PIN) ? 0 : BUTTON_RIGHT) |
         (digitalRead(BTN_UP_PIN) ? 0 : BUTTON_UP) |
         (digitalRead(BTN_DOWN_PIN) ? 0 : BUTTON_DOWN) |
         (digitalRead(BTN_FIRE_PIN) ? 0 : BUTTON_FIRE);
#endif
}

void loop(void) {
  update_screen();

#ifdef LED_PIN
  leds_update();
#endif
}
