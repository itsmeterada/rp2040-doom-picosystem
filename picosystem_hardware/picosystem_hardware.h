//
//  Pimoroni PicoSystem hardware abstraction layer
//

#ifndef PICOSYSTEM_HARDWARE_H
#define PICOSYSTEM_HARDWARE_H

#pragma once

#include <stdlib.h>
#include <string.h>

#include "hardware/adc.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "hardware/vreg.h"

#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#define PIXEL_DOUBLE

#ifdef PIXEL_DOUBLE
  #define PICOSYSTEM_SCREEN_WIDTH   120
  #define PICOSYSTEM_SCREEN_HEIGHT  120
#else // PIXEL_DOUBLE
  #define PICOSYSTEM_SCREEN_WIDTH   240
  #define PICOSYSTEM_SCREEN_HEIGHT  240
#endif // PIXEL_DOUBLE

typedef uint16_t color_t;
typedef struct {
  int32_t w, h;
  color_t *data;
  bool alloc;
} buffer_t;

struct picosystem_hw {
  PIO screen_pio;
  uint screen_sm;
  uint32_t dma_channel;
  volatile int16_t dma_scanline;
  buffer_t *screen;
  int32_t cx, cy, cw, ch;
  uint32_t io, lio; // input, last input
  bool in_flip;
};

enum PICOSYSTEM_PIN {
  PICOSYSTEM_PIN_RED = 14, PICOSYSTEM_PIN_GREEN = 13, PICOSYSTEM_PIN_BLUE = 15,                  // user rgb led
  PICOSYSTEM_PIN_CS = 5, PICOSYSTEM_PIN_SCK = 6, PICOSYSTEM_PIN_MOSI  = 7,                       // spi
  PICOSYSTEM_PIN_VSYNC = 8, PICOSYSTEM_PIN_DC = 9, PICOSYSTEM_PIN_LCD_RESET = 4, PICOSYSTEM_PIN_BACKLIGHT = 12, // screen
  PICOSYSTEM_PIN_AUDIO = 11,                                       // audio
  PICOSYSTEM_PIN_CHARGE_LED = 2, PICOSYSTEM_PIN_CHARGING = 24, PICOSYSTEM_PIN_BATTERY_LEVEL = 26 // battery / charging
};

  // input pins
  enum PICOSYSTEM_INPUT {
    PICOSYSTEM_INPUT_UP    = 23,
    PICOSYSTEM_INPUT_DOWN  = 20,
    PICOSYSTEM_INPUT_LEFT  = 22,
    PICOSYSTEM_INPUT_RIGHT = 21,
    PICOSYSTEM_INPUT_A     = 18,
    PICOSYSTEM_INPUT_B     = 19,
    PICOSYSTEM_INPUT_X     = 17,
    PICOSYSTEM_INPUT_Y     = 16
  };

void picosystem_init();
void picosystem_backlight(uint8_t brightness);
void picosystem_led(uint8_t r, uint8_t g, uint8_t b);

color_t picosystem_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void picosystem_clear(color_t c);
void picosystem_draw_line(color_t *fb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, color_t c);
inline void picosystem_write_pixel(int32_t x, int32_t y, color_t c);

uint32_t picosystem_time();
uint32_t picosystem_time_us();
void picosystem_sleep(uint32_t ms);
void picosystem_wait_vsync();
bool picosystem_is_flipping();
void picosystem_flip();
uint32_t picosystem_gpio_get();

#endif // PICOSYSTEM_HARDWARE_H
