//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021-2022 Graham Sanderson
//
// ThumbyColor port
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
#include "hardware/spi.h"
#include "hardware/pwm.h"
#endif

// ThumbyColor display is 128x128
#define DISPLAYWIDTH 128
#define DISPLAYHEIGHT 128

// ThumbyColor GPIO pins
#define LCD_SDA_PIN   19
#define LCD_SCK_PIN   18
#define LCD_CS_PIN    17
#define LCD_DC_PIN    16
#define LCD_RST_PIN   4
#define LCD_BL_PIN    7

// SPI speeds
#define SPI_BAUDRATE_CMD  (10 * 1000 * 1000)
#define SPI_BAUDRATE_DATA (80 * 1000 * 1000)

// GC9107 commands
#define GC9107_SLPOUT   0x11
#define GC9107_INVON    0x21
#define GC9107_DISPON   0x29
#define GC9107_CASET    0x2A
#define GC9107_RASET    0x2B
#define GC9107_RAMWR    0x2C
#define GC9107_MADCTL   0x36
#define GC9107_COLMOD   0x3A

static const patch_t *stbar;
volatile uint8_t interp_in_use;
static boolean initialized = false;
boolean screenvisible = true;
boolean screensaver_mode = false;
isb_int8_t usegamma = 0;
unsigned int joywait = 0;
pixel_t *I_VideoBuffer;

uint8_t __aligned(4) frame_buffer[2][SCREENWIDTH * SCREENHEIGHT];
static uint16_t palette_rgb565[256];
static uint8_t __scratch_x("shared_pal") shared_pal[NUM_SHARED_PALETTES][16];
static int8_t next_pal = -1;

semaphore_t vsync;

uint8_t *text_screen_data;
static uint32_t *text_scanline_buffer_start;
static uint8_t *text_screen_cpy;
static uint8_t *text_font_cpy;

uint8_t display_frame_index;
uint8_t display_overlay_index;
uint8_t display_video_type;

uint8_t *wipe_yoffsets;
int16_t *wipe_yoffsets_raw;
uint32_t *wipe_linelookup;
uint8_t next_video_type;
uint8_t next_frame_index;
uint8_t next_overlay_index;
#if !DEMO1_ONLY
uint8_t *next_video_scroll;
uint8_t *video_scroll;
#endif
volatile uint8_t wipe_min;

void I_ShutdownGraphics(void) {}
void I_StartFrame(void) {}
void I_SetWindowTitle(const char *title) {}

void I_SetPaletteNum(int doompalette) {
    next_pal = doompalette;
}

static inline uint16_t rgb888_to_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static inline uint16_t crapify_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return rgb888_to_rgb565(r, g, b);
}

#if PICO_ON_DEVICE
static int dma_channel;

static void gc9107_command(uint8_t cmd, size_t len, const uint8_t *data) {
    spi_set_baudrate(spi0, SPI_BAUDRATE_CMD);
    gpio_put(LCD_CS_PIN, 0);
    gpio_put(LCD_DC_PIN, 0);
    spi_write_blocking(spi0, &cmd, 1);
    if (data && len > 0) {
        gpio_put(LCD_DC_PIN, 1);
        spi_write_blocking(spi0, data, len);
    }
    gpio_put(LCD_CS_PIN, 1);
}

static void display_driver_init() {
    // Initialize SPI
    spi_init(spi0, SPI_BAUDRATE_CMD);
    gpio_set_function(LCD_SDA_PIN, GPIO_FUNC_SPI);
    gpio_set_function(LCD_SCK_PIN, GPIO_FUNC_SPI);

    // Control pins
    gpio_init(LCD_CS_PIN);
    gpio_set_dir(LCD_CS_PIN, GPIO_OUT);
    gpio_put(LCD_CS_PIN, 1);

    gpio_init(LCD_DC_PIN);
    gpio_set_dir(LCD_DC_PIN, GPIO_OUT);

    gpio_init(LCD_RST_PIN);
    gpio_set_dir(LCD_RST_PIN, GPIO_OUT);

    // Backlight PWM
    gpio_set_function(LCD_BL_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(LCD_BL_PIN);
    pwm_set_wrap(slice, 65535);
    pwm_set_gpio_level(LCD_BL_PIN, 65535);
    pwm_set_enabled(slice, true);

    // Hardware reset
    gpio_put(LCD_RST_PIN, 0);
    sleep_ms(50);
    gpio_put(LCD_RST_PIN, 1);
    sleep_ms(120);

    // GC9107 initialization sequence
    gc9107_command(0xFE, 0, NULL);  // Inter-register enable 1
    gc9107_command(0xEF, 0, NULL);  // Inter-register enable 2

    // Power control
    uint8_t data;
    data = 0x14; gc9107_command(0xB0, 1, &data);
    data = 0x01; gc9107_command(0xB2, 1, &data);
    data = 0x03; gc9107_command(0xB3, 1, &data);
    data = 0x10; gc9107_command(0xB4, 1, &data);
    data = 0x0A; gc9107_command(0xB5, 1, &data);
    data = 0x21; gc9107_command(0xB6, 1, &data);
    data = 0x04; gc9107_command(0xB7, 1, &data);

    data = 0x08; gc9107_command(0xA8, 1, &data);
    data = 0x10; gc9107_command(0xB8, 1, &data);

    // Voltage
    data = 0x5A; gc9107_command(0xE7, 1, &data);
    data = 0x23; gc9107_command(0xE8, 1, &data);
    data = 0x47; gc9107_command(0xE9, 1, &data);
    data = 0x99; gc9107_command(0xEA, 1, &data);

    // Gamma
    uint8_t gamma_p[] = {0x00, 0x05, 0x0B, 0x09, 0x19, 0x0B, 0x35, 0x99, 0x49, 0x0A, 0x0C, 0x0C, 0x26, 0x2D};
    gc9107_command(0xF0, 14, gamma_p);
    uint8_t gamma_n[] = {0x00, 0x05, 0x0B, 0x09, 0x05, 0x23, 0x33, 0x44, 0x44, 0x1A, 0x15, 0x15, 0x2B, 0x34};
    gc9107_command(0xF1, 14, gamma_n);

    // Color format: RGB565
    data = 0x05; gc9107_command(GC9107_COLMOD, 1, &data);

    // Display orientation
    data = 0x08; gc9107_command(GC9107_MADCTL, 1, &data);

    // Exit sleep
    gc9107_command(GC9107_SLPOUT, 0, NULL);
    sleep_ms(120);

    // Invert colors (typical for IPS)
    gc9107_command(GC9107_INVON, 0, NULL);

    // Display on
    gc9107_command(GC9107_DISPON, 0, NULL);
    sleep_ms(20);

    // Setup DMA
    dma_channel = dma_claim_unused_channel(true);

    // Clear display
    uint8_t caset[] = {0, 0, 0, 127};
    uint8_t raset[] = {0, 0, 0, 127};
    gc9107_command(GC9107_CASET, 4, caset);
    gc9107_command(GC9107_RASET, 4, raset);
    gc9107_command(GC9107_RAMWR, 0, NULL);

    spi_set_baudrate(spi0, SPI_BAUDRATE_DATA);
    gpio_put(LCD_CS_PIN, 0);
    gpio_put(LCD_DC_PIN, 1);
    for (int i = 0; i < 128 * 128; i++) {
        uint8_t zero[2] = {0, 0};
        spi_write_blocking(spi0, zero, 2);
    }
    gpio_put(LCD_CS_PIN, 1);
}
#endif

void __noinline new_frame_init_overlays_palette_and_wipe() {
    if (display_video_type >= FIRST_VIDEO_TYPE_WITH_OVERLAYS) {
        memset(vpatchlists->vpatch_next, 0, sizeof(vpatchlists->vpatch_next));
        memset(vpatchlists->vpatch_starters, 0, sizeof(vpatchlists->vpatch_starters));
        memset(vpatchlists->vpatch_doff, 0, sizeof(vpatchlists->vpatch_doff));
        vpatchlist_t *overlays = vpatchlists->overlays[display_overlay_index];
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
            assert(vpatch_type(stbar) == vp4_solid);
            for (int i = 0; i < NUM_SHARED_PALETTES; i++) {
                patch_t *patch = resolve_vpatch_handle(vpatch_for_shared_palette[i]);
                assert(vpatch_colorcount(patch) <= 16);
                assert(vpatch_has_shared_palette(patch));
                for (int j = 0; j < 16; j++) {
                    uint16_t rgb565 = palette_rgb565[vpatch_palette(patch)[j]];
                    uint8_t r = (rgb565 >> 11) << 3;
                    uint8_t g = ((rgb565 >> 5) & 0x3F) << 2;
                    uint8_t b = (rgb565 & 0x1F) << 3;
                    shared_pal[i][j] = (r * 5 + g * 3 + b * 3) / 8;
                }
            }
        }
        if (display_video_type == VIDEO_TYPE_WIPE) {
            wipe_min = SCREENHEIGHT + 1;
        }
    }
}

void I_FinishUpdate(void) {
    sem_acquire_blocking(&vsync);
    display_video_type = next_video_type;
    display_frame_index = next_frame_index;
    display_overlay_index = next_overlay_index;
    if (display_video_type != VIDEO_TYPE_SAVING) {
        new_frame_init_overlays_palette_and_wipe();
    }
    sem_release(&vsync);
}

#if PICO_ON_DEVICE
#define FRAME_PERIOD 16667  // ~60 FPS

// For 128x80 resolution centered on 128x128 display
// Black bars: 24 pixels top, 24 pixels bottom
#define DISPLAY_Y_OFFSET 24

static void core1() {
    absolute_time_t frame_time = get_absolute_time();

    while (true) {
        sem_acquire_blocking(&vsync);

        // Set window for full display
        uint8_t caset[] = {0, 0, 0, 127};
        uint8_t raset[] = {0, 0, 0, 127};
        gc9107_command(GC9107_CASET, 4, caset);
        gc9107_command(GC9107_RASET, 4, raset);
        gc9107_command(GC9107_RAMWR, 0, NULL);

        spi_set_baudrate(spi0, SPI_BAUDRATE_DATA);
        gpio_put(LCD_CS_PIN, 0);
        gpio_put(LCD_DC_PIN, 1);

        // Display 128x80 framebuffer centered on 128x128 display
        // Top black bar (24 pixels)
        for (int y = 0; y < DISPLAY_Y_OFFSET; y++) {
            for (int x = 0; x < 128; x++) {
                uint8_t data[2] = {0, 0};
                spi_write_blocking(spi0, data, 2);
            }
        }

        // Game area (80 pixels)
        for (int y = 0; y < SCREENHEIGHT; y++) {
            for (int x = 0; x < SCREENWIDTH; x++) {
                uint8_t *pframe = &frame_buffer[display_frame_index][y * SCREENWIDTH + x];
                uint16_t pixel = palette_rgb565[*pframe];
                uint8_t data[2] = { pixel >> 8, pixel & 0xFF };
                spi_write_blocking(spi0, data, 2);
            }
        }

        // Bottom black bar (24 pixels)
        for (int y = 0; y < DISPLAY_Y_OFFSET; y++) {
            for (int x = 0; x < 128; x++) {
                uint8_t data[2] = {0, 0};
                spi_write_blocking(spi0, data, 2);
            }
        }

        gpio_put(LCD_CS_PIN, 1);
        sem_release(&vsync);

        frame_time = delayed_by_us(frame_time, FRAME_PERIOD);
        sleep_until(frame_time);
    }
}
#endif

void I_InitGraphics(void) {
    stbar = resolve_vpatch_handle(VPATCH_STBAR);
    sem_init(&vsync, 1, 1);
    pd_init();
#if PICO_ON_DEVICE
    display_driver_init();
    multicore_launch_core1(core1);
#endif
#if USE_ZONE_FOR_MALLOC
    disallow_core1_malloc = true;
#endif
    initialized = true;
}

void I_BindVideoVariables(void) {}

void I_StartTic(void) {
    if (!initialized) return;
    I_GetEvent();
}

void I_UpdateNoBlit(void) {}

int I_GetPaletteIndex(int r, int g, int b) {
    return 0;
}

#if !NO_USE_ENDDOOM
void I_Endoom(byte *endoom_data) {
    uint32_t size;
    uint8_t *wa = pd_get_work_area(&size);
    assert(size >= TEXT_SCANLINE_BUFFER_TOTAL_WORDS * 4 + 80*25*2 + 4096);
    text_screen_cpy = wa;
    text_font_cpy = text_screen_cpy + 80 * 25 * 2;
    text_scanline_buffer_start = (uint32_t *)(text_font_cpy + 4096);
    static_assert(TEXT_SCANLINE_BUFFER_TOTAL_WORDS * 4 > 1024 + 512, "");
    uint8_t *tmp_buf = (uint8_t *)text_scanline_buffer_start;
    uint16_t *decoder = (uint16_t *)(tmp_buf + 512);
    th_bit_input bi;
    th_bit_input_init(&bi, normal_font_data_z);
    decode_data(text_font_cpy, 4096, &bi, decoder, 512, tmp_buf, 512);
    th_bit_input_init(&bi, endoom_data);
    decode_data(text_screen_cpy, 80*25, &bi, decoder, 512, tmp_buf, 512);
    decode_data(text_screen_cpy+80*25, 80*25, &bi, decoder, 512, tmp_buf, 512);
    static_assert(TEXT_SCANLINE_BUFFER_TOTAL_WORDS * 4 > 80*25*2, "");
    memcpy(tmp_buf, text_screen_cpy, 80*25*2);
    for(int i=0;i<80*25;i++) {
        text_screen_cpy[i*2] = tmp_buf[i];
        text_screen_cpy[i*2+1] = tmp_buf[80*25 + i];
    }
    text_screen_data = text_screen_cpy;
}
#endif

void I_GraphicsCheckCommandLine(void) {}
void I_CheckIsScreensaver(void) {}
void I_DisplayFPSDots(boolean dots_on) {}

#endif
