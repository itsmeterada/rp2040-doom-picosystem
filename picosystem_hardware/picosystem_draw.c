/*
 *  PicoSystem drawing functions
 */

#include "picosystem_hardware.h"

extern struct picosystem_hw pshw;

color_t picosystem_rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  // color_t will contain pixel data in the format aaaarrrrggggbbbb
  return (r & 0xf) | ((a & 0xf) << 4) | ((b & 0xf) << 8) | ((g & 0xf) << 12);
}

// clear screen
void picosystem_clear(color_t c) {
  color_t *dst = pshw.screen->data;
  for (int32_t y = 0; y < PICOSYSTEM_SCREEN_HEIGHT; y++) {
    for (int32_t x = 0; x < PICOSYSTEM_SCREEN_WIDTH; x++) {
      *dst++ = c;
    }
  }
}


inline void picosystem_write_pixel(int32_t x, int32_t y, color_t c) {
  color_t *dst = pshw.screen->data;
  dst[x + y * PICOSYSTEM_SCREEN_WIDTH] = c;
}

void picosystem_draw_line(color_t *fb, int32_t x1, int32_t y1, int32_t x2, int32_t y2, color_t color)
{
  bool yLonger = false;
  int shortLen = y2 - y1;
  int longLen = x2 - x1;
  int decInc;
  color_t *dst;

  if (fb == NULL) {
    dst = pshw.screen->data;
  } else {
    dst = fb;
  }

  if ((shortLen ^ (shortLen >> 31)) - (shortLen >> 31) > (longLen ^ (longLen >> 31)) - (longLen >> 31)) {
    shortLen ^= longLen;
    longLen ^= shortLen;
    shortLen ^= longLen;
    yLonger = true;
  }

  if (longLen == 0) decInc = 0;
  else decInc = (shortLen << 16) / longLen;

  if (yLonger) {
    if (longLen > 0) {
      longLen += y1;
      for (int j = 0x8000 + (x1 << 16); y1 <= longLen; ++y1) {
        picosystem_write_pixel((j >> 16), y1, color);
        j+= decInc;
      }
      return;
    }
    longLen += y1;
    for (int j = 0x8000 + (x1 << 16); y1 >= longLen; --y1) {
      picosystem_write_pixel((j >> 16), y1, color);
      j -= decInc;
    }
    return;
  }

  if (longLen > 0) {
    longLen += x1;
    for (int j = 0x8000 + (y1 << 16); x1 <= longLen; ++x1) {
      picosystem_write_pixel(x1, (j >> 16), color);
      j += decInc;
    }
    return;
  }
  longLen += x1;
  for (int j = 0x8000 + (y1 << 16); x1 >= longLen; --x1) {
    picosystem_write_pixel(x1, (j >> 16), color);
    j -= decInc;
  }
}
