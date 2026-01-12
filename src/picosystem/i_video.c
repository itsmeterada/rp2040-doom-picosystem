//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021-2022 Graham Sanderson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DOOM graphics stuff for Pico.
//

#if PICODOOM_RENDER_NEWHOPE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <doom/r_data.h>
#include "doom/f_wipe.h"
#include "pico.h"

#include "config.h"
#include "d_loop.h"
#include "deh_str.h"
#include "doomtype.h"
#include "i_input.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_misc.h"
#include "tables.h"
#include "v_diskicon.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "pico/multicore.h"
#include "pico/sync.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "picodoom.h"
#include "video_doom.pio.h"
#include "image_decoder.h"
#if PICO_ON_DEVICE
#include "hardware/dma.h"
#include "hardware/structs/xip_ctrl.h"
#include "hardware/spi.h"
#else
#include "SDL_image.h"
#include "SDL_mutex.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#endif

int debugline = 39;

#if PICO_ON_DEVICE
#define DISPLAYWIDTH 240
#define DISPLAYHEIGHT 240
#else

#if FSAA
#define DISPLAYWIDTH (SCREENWIDTH>>FSAA)
#define DISPLAYHEIGHT (SCREENHEIGHT>>FSAA)
#else
#define DISPLAYWIDTH SCREENWIDTH
#define DISPLAYHEIGHT SCREENHEIGHT
#endif

#endif

#define YELLOW_SUBMARINE 0
#define SUPPORT_TEXT 1
#if SUPPORT_TEXT
typedef struct __packed {
    const char * const name;
    const uint8_t * const data;
    const uint8_t w;
    const uint8_t h;
} txt_font_t;
#define TXT_SCREEN_W 80
#include "fonts/normal.h"

#endif

// todo temproarly turned this off because it causes a seeming bug in scanvideo (perhaps only with the new callback stuff) where the last repeated scanline of a pixel line is freed while shown
//  note it may just be that this happens anyway, but usually we are writing slower than the beam?
#define USE_INTERP PICO_ON_DEVICE
#if USE_INTERP
#include "hardware/interp.h"
#endif


CU_REGISTER_DEBUG_PINS(scanline_copy)
//CU_SELECT_DEBUG_PINS(scanline_copy)

static const patch_t *stbar;

volatile uint8_t interp_in_use;


// display has been set up?

static boolean initialized = false;

boolean screenvisible = true;

//int vga_porch_flash = false;

//static int startup_delay = 1000;

// The screen buffer; this is modified to draw things to the screen
//pixel_t *I_VideoBuffer = NULL;
// Gamma correction level to use

boolean screensaver_mode = false;

isb_int8_t usegamma = 0;

// Joystick/gamepad hysteresis
unsigned int joywait = 0;

pixel_t *I_VideoBuffer; // todo can't have this

uint8_t __aligned(4) frame_buffer[2][SCREENWIDTH * SCREENHEIGHT];
static uint16_t palette_rgb565[256];  // RGB565 color palette for PicoSystem LCD
static uint8_t __scratch_x("shared_pal") shared_pal[NUM_SHARED_PALETTES][16];
static int8_t next_pal=-1;

semaphore_t vsync;

uint8_t *text_screen_data;
static uint32_t *text_scanline_buffer_start;
static uint8_t *text_screen_cpy;
static uint8_t *text_font_cpy;


#if USE_INTERP
static interp_hw_save_t interp0_save, interp1_save;
static boolean interp_updated;
static boolean need_save;

static inline void interp_save_static(interp_hw_t *interp, interp_hw_save_t *saver) {
    saver->accum[0] = interp->accum[0];
    saver->accum[1] = interp->accum[1];
    saver->base[0] = interp->base[0];
    saver->base[1] = interp->base[1];
    saver->base[2] = interp->base[2];
    saver->ctrl[0] = interp->ctrl[0];
    saver->ctrl[1] = interp->ctrl[1];
}

static inline void interp_restore_static(interp_hw_t *interp, interp_hw_save_t *saver) {
    interp->accum[0] = saver->accum[0];
    interp->accum[1] = saver->accum[1];
    interp->base[0] = saver->base[0];
    interp->base[1] = saver->base[1];
    interp->base[2] = saver->base[2];
    interp->ctrl[0] = saver->ctrl[0];
    interp->ctrl[1] = saver->ctrl[1];
}
#endif

void I_ShutdownGraphics(void)
{
}

//
// I_StartFrame
//
void I_StartFrame (void)
{
    // er?
}

//
// Set the window title
//

void I_SetWindowTitle(const char *title)
{
//    window_title = title;
}

//
// I_SetPalette
//
void I_SetPaletteNum(int doompalette)
{
    next_pal = doompalette;
}


uint8_t display_frame_index;
uint8_t display_overlay_index;
uint8_t display_video_type;


uint8_t *wipe_yoffsets; // position of start of y in each column
int16_t *wipe_yoffsets_raw;
uint32_t *wipe_linelookup; // offset of each line from start of screenbuffer (can be negative for FB 1 to FB 0)
uint8_t next_video_type;
uint8_t next_frame_index; // todo combine with video type?
uint8_t next_overlay_index;
#if !DEMO1_ONLY
uint8_t *next_video_scroll;
uint8_t *video_scroll;
#endif
volatile uint8_t wipe_min;

#pragma GCC push_options
#if PICO_ON_DEVICE
#pragma GCC optimize("O3")
#endif

// Forward declaration
static inline uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b);

static inline uint16_t crapify_rgb(uint8_t r, uint8_t g, uint8_t b) {
    // Convert RGB888 to RGB565 for PicoSystem color LCD
    return rgb888_to_rgb565(r, g, b);
}

// this is not in flash as quite large and only once per frame
void __noinline new_frame_init_overlays_palette_and_wipe() {
    // re-initialize our overlay drawing
    if (display_video_type >= FIRST_VIDEO_TYPE_WITH_OVERLAYS) {
        memset(vpatchlists->vpatch_next, 0, sizeof(vpatchlists->vpatch_next));
        memset(vpatchlists->vpatch_starters, 0, sizeof(vpatchlists->vpatch_starters));
        memset(vpatchlists->vpatch_doff, 0, sizeof(vpatchlists->vpatch_doff));
        vpatchlist_t *overlays = vpatchlists->overlays[display_overlay_index];
        // do it in reverse so our linked lists are in ascending order
        for (int i = overlays->header.size - 1; i > 0; i--) {
            assert(overlays[i].entry.y < count_of(vpatchlists->vpatch_starters));
            vpatchlists->vpatch_next[i] = vpatchlists->vpatch_starters[overlays[i].entry.y];
            vpatchlists->vpatch_starters[overlays[i].entry.y] = i;
        }
        if (next_pal != -1) {
            static const uint8_t *playpal;
            static bool calculate_palettes;
            if (!playpal) {
                lumpindex_t l = W_GetNumForName("PLAYPAL");
                playpal = W_CacheLumpNum(l, PU_STATIC);
                calculate_palettes = W_LumpLength(l) == 768;
            }
            if (!calculate_palettes || !next_pal) {
                const uint8_t *doompalette = playpal + next_pal * 768;
                for (int i = 0; i < 256; i++) {
                    int r = *doompalette++;
                    int g = *doompalette++;
                    int b = *doompalette++;

                    if (usegamma) {
                        r = gammatable[usegamma-1][r];
                        g = gammatable[usegamma-1][g];
                        b = gammatable[usegamma-1][b];
                    }

                    palette_rgb565[i] = crapify_rgb(r, g, b);
                }
            } else {
                int mul, r0, g0, b0;
                if (next_pal < 9) {
                    mul = next_pal * 65536 / 9;
                    r0 = 255; g0 = b0 = 0;
                } else if (next_pal < 13) {
                    mul = (next_pal - 8) * 65536 / 8;
                    r0 = 215; g0 = 186; b0 = 69;
                } else {
                    mul = 65536 / 8;
                    r0 = b0 = 0; g0 = 256;
                }
                const uint8_t *doompalette = playpal;
                for (int i = 0; i < 256; i++) {
                    int r = *doompalette++;
                    int g = *doompalette++;
                    int b = *doompalette++;

                    r += ((r0 - r) * mul) >> 16;
                    g += ((g0 - g) * mul) >> 16;
                    b += ((b0 - b) * mul) >> 16;

                    palette_rgb565[i] = crapify_rgb(r, g, b);
                }
            }
            next_pal = -1;
            assert(vpatch_type(stbar) == vp4_solid); // no transparent, no runs, 4 bpp
            for (int i = 0; i < NUM_SHARED_PALETTES; i++) {
                patch_t *patch = resolve_vpatch_handle(vpatch_for_shared_palette[i]);
                assert(vpatch_colorcount(patch) <= 16);
                assert(vpatch_has_shared_palette(patch));
                for (int j = 0; j < 16; j++) {
                    // Convert RGB565 back to grayscale for shared_pal
                    uint16_t rgb565 = palette_rgb565[vpatch_palette(patch)[j]];
                    uint8_t r = (rgb565 >> 11) << 3;
                    uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;
                    uint8_t b = (rgb565 & 0x1F) << 3;
                    shared_pal[i][j] = (r * 5 + g * 3 + b * 3) / 8;
                }
            }
        }
        if (display_video_type == VIDEO_TYPE_WIPE) {
            printf("WIPEMIN %d\n", wipe_min);
            // Note: wipe is disabled for now to avoid potential issues
            // Just complete the wipe immediately
            wipe_min = SCREENHEIGHT + 1;
        }
    }
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
    sem_acquire_blocking(&vsync);

    display_video_type = next_video_type;
    display_frame_index = next_frame_index;
    display_overlay_index = next_overlay_index;

    if (display_video_type != VIDEO_TYPE_SAVING) {
        // this stuff is large (so in flash) and not needed in save move
        new_frame_init_overlays_palette_and_wipe();
    }

    sem_release(&vsync);
}

#pragma GCC pop_options

#if PICO_ON_DEVICE
#define LOW_PRIO_IRQ 31
#include "hardware/irq.h"
#include "hardware/pwm.h"

static void __not_in_flash_func(free_buffer_callback)() {
//    irq_set_pending(LOW_PRIO_IRQ);
    // ^ is in flash by default
    *((io_rw_32 *) (PPB_BASE + M0PLUS_NVIC_ISPR_OFFSET)) = 1u << LOW_PRIO_IRQ;
}

#define FRAME_PERIOD J_OLED_FRAME_PERIOD

// PicoSystem LCD pins (explicit definitions)
#define LCD_SCK_PIN  6
#define LCD_MOSI_PIN 7
#define LCD_CS_PIN   5
#define LCD_DC_PIN   9
#define LCD_RESET_PIN 4
#define LCD_BL_PIN   12

// ST7789 command definitions
enum st7789_cmd {
    ST7789_SWRESET   = 0x01,
    ST7789_SLPOUT    = 0x11,
    ST7789_INVON     = 0x21,
    ST7789_DISPON    = 0x29,
    ST7789_CASET     = 0x2A,
    ST7789_RASET     = 0x2B,
    ST7789_RAMWR     = 0x2C,
    ST7789_TEON      = 0x35,
    ST7789_MADCTL    = 0x36,
    ST7789_COLMOD    = 0x3A,
    ST7789_FRMCTR2   = 0xB2,
    ST7789_GCTRL     = 0xB7,
    ST7789_VCOMS     = 0xBB,
    ST7789_LCMCTRL   = 0xC0,
    ST7789_VDVVRHEN  = 0xC2,
    ST7789_VRHS      = 0xC3,
    ST7789_VDVS      = 0xC4,
    ST7789_FRCTRL2   = 0xC6,
    ST7789_PWRCTRL1  = 0xD0,
    ST7789_GMCTRP1   = 0xE0,
    ST7789_GMCTRN1   = 0xE1,
    ST7789_GAMSET    = 0x26,
};

static void st7789_command(uint8_t cmd, size_t len, const char *data) {
    gpio_put(LCD_CS_PIN, 0);
    gpio_put(LCD_DC_PIN, 0); // command mode
    spi_write_blocking(spi0, &cmd, 1);
    if (data) {
        gpio_put(LCD_DC_PIN, 1); // data mode
        spi_write_blocking(spi0, (const uint8_t*)data, len);
    }
    gpio_put(LCD_CS_PIN, 1);
}

static uint16_t gamma_correct(uint8_t v) {
    float gamma = 2.8f;
    return (uint16_t)(powf((float)(v) / 100.0f, gamma) * 65535.0f + 0.5f);
}

static void set_backlight(uint8_t brightness) {
    pwm_set_gpio_level(PICOSYSTEM_LCD_BACKLIGHT, gamma_correct(brightness));
}

// Debug: blink LED to show we're alive
static void debug_led_init() {
    // Initialize RGB LED pins for PWM
    pwm_config cfg = pwm_get_default_config();

    pwm_set_wrap(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_RED), 65535);
    pwm_init(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_RED), &cfg, true);
    gpio_set_function(PICOSYSTEM_PIN_RED, GPIO_FUNC_PWM);

    pwm_set_wrap(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_GREEN), 65535);
    pwm_init(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_GREEN), &cfg, true);
    gpio_set_function(PICOSYSTEM_PIN_GREEN, GPIO_FUNC_PWM);

    pwm_set_wrap(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_BLUE), 65535);
    pwm_init(pwm_gpio_to_slice_num(PICOSYSTEM_PIN_BLUE), &cfg, true);
    gpio_set_function(PICOSYSTEM_PIN_BLUE, GPIO_FUNC_PWM);
}

static void debug_led_set(uint8_t r, uint8_t g, uint8_t b) {
    pwm_set_gpio_level(PICOSYSTEM_PIN_RED, gamma_correct(r));
    pwm_set_gpio_level(PICOSYSTEM_PIN_GREEN, gamma_correct(g));
    pwm_set_gpio_level(PICOSYSTEM_PIN_BLUE, gamma_correct(b));
}

static void display_driver_init() {
    // === CHECKPOINT 1: We entered display_driver_init() ===
    // Blink BLUE rapidly 5 times to show we're in display_driver_init
    gpio_init(15); gpio_set_dir(15, GPIO_OUT);
    for (int i = 0; i < 5; i++) {
        gpio_put(15, 0); sleep_ms(100);
        gpio_put(15, 1); sleep_ms(100);
    }
    sleep_ms(500);  // Pause

    // === BACKLIGHT - Try PWM like reference ===
    pwm_config bl_cfg = pwm_get_default_config();
    pwm_set_wrap(pwm_gpio_to_slice_num(LCD_BL_PIN), 65535);
    pwm_init(pwm_gpio_to_slice_num(LCD_BL_PIN), &bl_cfg, true);
    gpio_set_function(LCD_BL_PIN, GPIO_FUNC_PWM);
    pwm_set_gpio_level(LCD_BL_PIN, 65535);  // Full brightness

    // === SPI INIT - exactly like reference ===
    spi_init(spi0, 8000000);

    // === RESET - exactly like reference ===
    gpio_set_function(LCD_RESET_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(LCD_RESET_PIN, GPIO_OUT);
    gpio_put(LCD_RESET_PIN, 0);
    sleep_ms(100);
    gpio_put(LCD_RESET_PIN, 1);

    // === CONTROL PINS - exactly like reference ===
    gpio_set_function(LCD_DC_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(LCD_DC_PIN, GPIO_OUT);
    gpio_set_function(LCD_CS_PIN, GPIO_FUNC_SIO);
    gpio_set_dir(LCD_CS_PIN, GPIO_OUT);
    gpio_set_function(LCD_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MOSI_PIN, GPIO_FUNC_SPI);

    gpio_put(LCD_CS_PIN, 1);

    // === ST7789 INIT ===
    st7789_command(ST7789_SWRESET, 0, NULL);
    sleep_ms(5);
    st7789_command(ST7789_MADCTL, 1, "\x04");
    st7789_command(ST7789_TEON, 1, "\x00");
    st7789_command(ST7789_FRMCTR2, 5, "\x0C\x0C\x00\x33\x33");
    st7789_command(ST7789_COLMOD, 1, "\x05");  // RGB565
    st7789_command(ST7789_GAMSET, 1, "\x01");
    st7789_command(ST7789_GCTRL, 1, "\x14");
    st7789_command(ST7789_VCOMS, 1, "\x25");
    st7789_command(ST7789_LCMCTRL, 1, "\x2C");
    st7789_command(ST7789_VDVVRHEN, 1, "\x01");
    st7789_command(ST7789_VRHS, 1, "\x12");
    st7789_command(ST7789_VDVS, 1, "\x20");
    st7789_command(ST7789_PWRCTRL1, 2, "\xA4\xA1");
    st7789_command(ST7789_FRCTRL2, 1, "\x1E");
    st7789_command(ST7789_GMCTRP1, 14, "\xD0\x04\x0D\x11\x13\x2B\x3F\x54\x4C\x18\x0D\x0B\x1F\x23");
    st7789_command(ST7789_GMCTRN1, 14, "\xD0\x04\x0C\x11\x13\x2C\x3F\x44\x51\x2F\x1F\x1F\x20\x23");
    st7789_command(ST7789_INVON, 0, NULL);
    sleep_ms(115);
    st7789_command(ST7789_SLPOUT, 0, NULL);
    st7789_command(ST7789_DISPON, 0, NULL);
    st7789_command(ST7789_CASET, 4, "\x00\x00\x00\xef");
    st7789_command(ST7789_RASET, 4, "\x00\x00\x00\xef");
    st7789_command(ST7789_RAMWR, 0, NULL);

    // Switch to data mode (keep CS low, DC high)
    gpio_put(LCD_CS_PIN, 0);
    gpio_put(LCD_DC_PIN, 1);

    // Enable vsync input
    gpio_init(8);
    gpio_set_dir(8, GPIO_IN);

    // Increase SPI speed for game
    spi_set_baudrate(spi0, 62500000);
}
#else

//this is pure laziness - using bits of pico-host-sdl's scanline simulation instead of setting up a clean sdl loop.
extern SDL_Surface *pico_access_surface;
extern SDL_Texture *texture_raw;
extern void send_update_screen();

const struct scanvideo_pio_program bogus_pio = {
    .id = "bogus"
};

static const scanvideo_timing_t bogus_timing = {
    .clock_freq = 1,
    .h_active = DISPLAYWIDTH,
    .v_active = DISPLAYHEIGHT,
    .h_total = 1,
    .v_total = 1,
};

static const scanvideo_mode_t bogus_mode = {
    .default_timing = &bogus_timing,
    .pio_program = &bogus_pio,
    .width = DISPLAYWIDTH,
    .height = DISPLAYHEIGHT,
    .xscale = 1,
    .yscale = 1,
};

#define FRAME_PERIOD 5556

extern SDL_Window *window;
static void display_driver_init() {

    scanvideo_setup(&bogus_mode);
}

static void simulate_display(uint dither) {
    static bool first = true;
    if (texture_raw && first) {
        SDL_SetWindowSize(window, 640, 360);
        first = false;
    }

    if (pico_access_surface) {
        uint8_t* data = (uint8_t*)pico_access_surface->pixels;

        uint w = MIN(MIN(DISPLAYWIDTH, SCREENWIDTH), pico_access_surface->w);
        uint h = MIN(MIN(DISPLAYHEIGHT, SCREENHEIGHT), pico_access_surface->h);
        
        for (int y = 0; y < h; ++y) {
            uint16_t* row = (uint16_t*)((uint8_t*)pico_access_surface->pixels + pico_access_surface->pitch * y);
            dither ^= 1;
            for (int x = 0; x < w; ++x) {
                dither ^= 1;

#if FSAA
                uint8_t *pframe = &frame_buffer[display_frame_index][y*(SCREENWIDTH<<FSAA) + (x<<FSAA)];
                uint lum = 0;
                for (int aay=0; aay<(1<<FSAA); ++aay) {
                    for (int aax=0; aax<(1<<FSAA); ++aax) {
                        lum += palette[pframe[aay*SCREENWIDTH + aax]];
                    }
                }
                lum >>= (FSAA*2);
                pframe += (1<<FSAA);
#else
                uint8_t *pframe = &frame_buffer[display_frame_index][y*SCREENWIDTH + x];
                uint lum = palette[*pframe];
#endif

                lum = (lum >> 5) + ((lum >> 4) & dither);
                lum = MIN(lum, 7);

#if DEBUGLINE
                if (x < 6)
                    lum = (-(((y>>(5-x))&1)==0))&7;
                if (y == debugline)
                    lum = 7;
#endif
                lum *= 36;
                row[x] = PICO_SCANVIDEO_PIXEL_FROM_RGB8(lum, lum, lum);
            }
        }
    }

    send_update_screen();
    SDL_Delay(1);
}


#endif

//#define TESTCARD_BAR 1

// Convert RGB888 to RGB565
static inline uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static void core1() {
    absolute_time_t frame_time = get_absolute_time();

    while (true) {
        sem_acquire_blocking(&vsync);

#if PICO_ON_DEVICE
        // Convert frame buffer to RGB565 and send to LCD
        // The frame buffer is 320x200, the display is 240x240
        // We need to scale/center the image

        // DOOM is 320x200, display is 240x240
        // Scale 320->240 horizontally (0.75x) and 200->240 vertically (1.2x)
        // This will show the full width with some vertical stretch

        // Alternatively, maintain aspect ratio: 200*240/320 = 150 pixels high, centered
        // Let's use full width scaling for now

        // Display SCREENWIDTH x SCREENHEIGHT framebuffer scaled and centered on 240x240 display
        // Scale up to fill display width, maintain aspect ratio
        // 160x100 -> 240x150 (1.5x scale), centered with 45 pixel black bars top/bottom
        // 240x150 -> 240x150 (1x scale), centered with 45 pixel black bars top/bottom

        int scaled_height = SCREENHEIGHT * DISPLAYWIDTH / SCREENWIDTH;
        int y_offset = (DISPLAYHEIGHT - scaled_height) / 2;

        for (int y = 0; y < DISPLAYHEIGHT; ++y) {
            int src_y = (y - y_offset) * SCREENHEIGHT / scaled_height;

            for (int x = 0; x < DISPLAYWIDTH; ++x) {
                uint16_t pixel;

                if (y < y_offset || y >= y_offset + scaled_height) {
                    // Black bar (top/bottom)
                    pixel = 0x0000;
                } else {
                    int src_x = x * SCREENWIDTH / DISPLAYWIDTH;
                    if (src_x >= SCREENWIDTH) src_x = SCREENWIDTH - 1;
                    if (src_y >= SCREENHEIGHT) src_y = SCREENHEIGHT - 1;
                    if (src_y < 0) src_y = 0;
                    uint8_t *pframe = &frame_buffer[display_frame_index][src_y * SCREENWIDTH + src_x];
                    pixel = palette_rgb565[*pframe];
                }

                // Send RGB565 pixel (big-endian for ST7789)
                uint8_t data[2] = { pixel >> 8, pixel & 0xFF };
                spi_write_blocking(spi0, data, 2);
            }
        }
        // After sending exactly 240*240 pixels, the ST7789 resets to position 0,0
        // so we're ready for the next frame without any commands
#else
        simulate_display(0);
#endif

        sem_release(&vsync);

        frame_time = delayed_by_us(frame_time, FRAME_PERIOD);
        sleep_until(frame_time);
    }
}

void I_InitGraphics(void)
{
    stbar = resolve_vpatch_handle(VPATCH_STBAR);
    sem_init(&vsync, 1, 1);
    pd_init();
    display_driver_init();

    multicore_launch_core1(core1);

#if USE_ZONE_FOR_MALLOC
    disallow_core1_malloc = true;
#endif
    initialized = true;
}

// Bind all variables controlling video options into the configuration
// file system.
void I_BindVideoVariables(void)
{
//    M_BindIntVariable("use_mouse",                 &usemouse);
//    M_BindIntVariable("fullscreen",                &fullscreen);
//    M_BindIntVariable("video_display",             &video_display);
//    M_BindIntVariable("aspect_ratio_correct",      &aspect_ratio_correct);
//    M_BindIntVariable("integer_scaling",           &integer_scaling);
//    M_BindIntVariable("vga_porch_flash",           &vga_porch_flash);
//    M_BindIntVariable("startup_delay",             &startup_delay);
//    M_BindIntVariable("fullscreen_width",          &fullscreen_width);
//    M_BindIntVariable("fullscreen_height",         &fullscreen_height);
//    M_BindIntVariable("force_software_renderer",   &force_software_renderer);
//    M_BindIntVariable("max_scaling_buffer_pixels", &max_scaling_buffer_pixels);
//    M_BindIntVariable("window_width",              &window_width);
//    M_BindIntVariable("window_height",             &window_height);
//    M_BindIntVariable("grabmouse",                 &grabmouse);
//    M_BindStringVariable("video_driver",           &video_driver);
//    M_BindStringVariable("window_position",        &window_position);
//    M_BindIntVariable("usegamma",                  &usegamma);
//    M_BindIntVariable("png_screenshots",           &png_screenshots);
}

//
// I_StartTic
//
void I_StartTic (void)
{
    if (!initialized)
    {
        return;
    }

    I_GetEvent();
//
//    if (usemouse && !nomouse && window_focused)
//    {
//        I_ReadMouse();
//    }
//
//    if (joywait < I_GetTime())
//    {
//        I_UpdateJoystick();
//    }
}


//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
    // what is this?
}

int I_GetPaletteIndex(int r, int g, int b)
{
    return 0;
}

#if !NO_USE_ENDDOOM
void I_Endoom(byte *endoom_data) {
    uint32_t size;
    uint8_t *wa = pd_get_work_area(&size);
    assert(size >=TEXT_SCANLINE_BUFFER_TOTAL_WORDS * 4 + 80*25*2 + 4096);
    text_screen_cpy = wa;
    text_font_cpy = text_screen_cpy + 80 * 25 * 2;
    text_scanline_buffer_start = (uint32_t *) (text_font_cpy + 4096);
#if 0
    static_assert(sizeof(normal_font_data) == 4096, "");
    memcpy(text_font_cpy, normal_font_data, sizeof(normal_font_data));
    memcpy(text_screen_cpy, endoom_data, 80 * 25 * 2);
#else
    static_assert(TEXT_SCANLINE_BUFFER_TOTAL_WORDS * 4 > 1024 + 512, "");
    uint8_t *tmp_buf = (uint8_t *)text_scanline_buffer_start;
    uint16_t *decoder = (uint16_t *)(tmp_buf + 512);
    th_bit_input bi;
    th_bit_input_init(&bi, normal_font_data_z);
    decode_data(text_font_cpy, 4096, &bi, decoder, 512, tmp_buf, 512);
    th_bit_input_init(&bi, endoom_data);
    // text
    decode_data(text_screen_cpy, 80*25, &bi, decoder, 512, tmp_buf, 512);
    // attr
    decode_data(text_screen_cpy+80*25, 80*25, &bi, decoder, 512, tmp_buf, 512);
    static_assert(TEXT_SCANLINE_BUFFER_TOTAL_WORDS * 4 > 80*25*2, "");
    // re-interlace the text & attr
    memcpy(tmp_buf, text_screen_cpy, 80*25*2);
    for(int i=0;i<80*25;i++) {
        text_screen_cpy[i*2] = tmp_buf[i];
        text_screen_cpy[i*2+1] = tmp_buf[80*25 + i];
    }
#endif
    text_screen_data = text_screen_cpy;
}
#endif

void I_GraphicsCheckCommandLine(void)
{

}

// Check if we have been invoked as a screensaver by xscreensaver.

void I_CheckIsScreensaver(void)
{
}

void I_DisplayFPSDots(boolean dots_on)
{
}


#endif

