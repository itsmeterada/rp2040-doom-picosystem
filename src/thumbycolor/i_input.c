//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021-2022 Graham Sanderson
//
// ThumbyColor input handling
//

#include <doom/sounds.h>
#include <doom/s_sound.h>
#include "pico.h"
#include "doomkeys.h"
#include "doomtype.h"
#include "d_event.h"
#include "i_input.h"
#include "i_system.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"
#include "m_controls.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include <stdlib.h>

// ThumbyColor button pins
#define BTN_UP_PIN     1
#define BTN_DOWN_PIN   3
#define BTN_LEFT_PIN   0
#define BTN_RIGHT_PIN  2
#define BTN_A_PIN      21
#define BTN_B_PIN      25
#define BTN_BUMPER_L   6
#define BTN_BUMPER_R   22
#define BTN_MENU_PIN   26

enum {
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_A,
    BTN_B,
    BTN_L,
    BTN_R,
    BTN_MENU,
    BTN_COUNT
};

static const uint8_t button_pins[BTN_COUNT] = {
    BTN_UP_PIN, BTN_DOWN_PIN, BTN_LEFT_PIN, BTN_RIGHT_PIN,
    BTN_A_PIN, BTN_B_PIN, BTN_BUMPER_L, BTN_BUMPER_R, BTN_MENU_PIN
};
static uint8_t button_state[BTN_COUNT] = {0};

void buttons_init() {
    for (int i = 0; i < BTN_COUNT; ++i) {
        gpio_init(button_pins[i]);
        gpio_set_dir(button_pins[i], GPIO_IN);
        gpio_pull_up(button_pins[i]);
    }

    key_right = KEY_RIGHTARROW;
    key_left = KEY_LEFTARROW;
    key_up = KEY_UPARROW;
    key_down = KEY_DOWNARROW;

    key_fire = KEY_RCTRL;
    key_use = KEY_RSHIFT;
    key_strafe = KEY_RSHIFT;
    key_speed = KEY_RSHIFT;

    key_prevweapon = '[';
    key_nextweapon = ']';

    key_menu_up = KEY_UPARROW;
    key_menu_down = KEY_DOWNARROW;
    key_menu_left = KEY_LEFTARROW;
    key_menu_right = KEY_RIGHTARROW;
    key_menu_back = KEY_RSHIFT;
    key_menu_forward = KEY_RCTRL;
    key_menu_confirm = KEY_RCTRL;
    key_menu_abort = KEY_RSHIFT;
}

void button_event(key_type_t key, bool pressed) {
    event_t event;
    if (pressed) {
        event.type = ev_keydown;
        event.data1 = key;
        event.data2 = key;
        event.data3 = key;
    } else {
        event.type = ev_keyup;
        event.data1 = key;
        event.data2 = 0;
        event.data3 = 0;
    }
    D_PostEvent(&event);
}

void buttons_getevent() {
    for (int i = 0; i < BTN_COUNT; ++i) {
        bool pressed = (gpio_get(button_pins[i]) == 0);
        if (pressed != (button_state[i] != 0)) {
            button_state[i] = pressed;

            switch (i) {
                case BTN_UP:
                    button_event(key_up, pressed);
                    break;
                case BTN_DOWN:
                    button_event(key_down, pressed);
                    break;
                case BTN_LEFT:
                    button_event(key_left, pressed);
                    break;
                case BTN_RIGHT:
                    button_event(key_right, pressed);
                    break;
                case BTN_A:
                    button_event(key_fire, pressed);
                    break;
                case BTN_B:
                    button_event(key_use, pressed);
                    break;
                case BTN_L:
                    button_event(key_prevweapon, pressed);
                    break;
                case BTN_R:
                    button_event(key_nextweapon, pressed);
                    break;
                case BTN_MENU:
                    button_event(KEY_ESCAPE, pressed);
                    break;
            }
        }
    }
}

static const int scancode_translate_table[] = SCANCODE_TO_KEYS_ARRAY;

static const char shiftxform[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31, ' ', '!', '"', '#', '$', '%', '&',
    '"', '(', ')', '*', '+', '<', '_', '>', '?',
    ')', '!', '@', '#', '$', '%', '^', '&', '*', '(',
    ':', ':', '<', '+', '>', '?', '@',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '[', '!', ']', '"', '_', '\'',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '{', '|', '}', '~', 127
};

static boolean text_input_enabled = true;
static unsigned int mouse_button_state = 0;
int novert = 0;

#if !USE_VANILLA_KEYBOARD_MAPPING_ONLY
int vanilla_keyboard_mapping = true;
#endif

enum {
    SDL_SCANCODE_SPACE = 44,
    SDL_SCANCODE_LCTRL = 224,
    SDL_SCANCODE_LSHIFT = 225,
    SDL_SCANCODE_LALT = 226,
    SDL_SCANCODE_LGUI = 227,
    SDL_SCANCODE_RCTRL = 228,
    SDL_SCANCODE_RSHIFT = 229,
    SDL_SCANCODE_RALT = 230,
    SDL_SCANCODE_RGUI = 231,
};

int TranslateKey(int scancode) {
    switch (scancode) {
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            return KEY_RCTRL;
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            return KEY_RSHIFT;
        case SDL_SCANCODE_LALT:
            return KEY_LALT;
        case SDL_SCANCODE_RALT:
            return KEY_RALT;
        default:
            if (scancode >= 0 && scancode < arrlen(scancode_translate_table)) {
                return scancode_translate_table[scancode];
            } else {
                return 0;
            }
    }
}

static int GetLocalizedKey(int scancode) {
    if (vanilla_keyboard_mapping) {
        return TranslateKey(scancode);
    } else {
        assert(false);
        return 0;
    }
}

int GetTypedChar(int scancode, boolean shiftdown) {
    if (!text_input_enabled) return 0;
    if (vanilla_keyboard_mapping) {
        int result = TranslateKey(scancode);
        if (shiftdown && result >= 0 && result < arrlen(shiftxform)) {
            result = shiftxform[result];
        }
        return result;
    } else {
        assert(false);
        return 0;
    }
}

void I_StartTextInput(int x1, int y1, int x2, int y2) {
    text_input_enabled = true;
}

void I_StopTextInput(void) {
    text_input_enabled = false;
}

void I_BindInputVariables(void) {
    M_BindIntVariable("novert", &novert);
}

#define WITH_SHIFT 0x8000

static void pico_key_down(int scancode, int keysym, int modifiers) {
    event_t event;
    event.type = ev_keydown;
    event.data1 = TranslateKey(scancode);
    event.data2 = GetLocalizedKey(scancode);
    event.data3 = GetTypedChar(scancode, modifiers & WITH_SHIFT ? 1 : 0);

    if (at_exit_screen) {
        handle_exit_key_down(scancode, modifiers & WITH_SHIFT ? 1 : 0, exit_screen_kb_buffer_80, 80);
        return;
    }
    if (event.data1 != 0) {
        D_PostEvent(&event);
    }
}

static void pico_key_up(int scancode, int keysym, int modifiers) {
    event_t event;
    event.type = ev_keyup;
    event.data1 = TranslateKey(scancode);
    event.data2 = 0;
    event.data3 = 0;
    if (event.data1 != 0) {
        D_PostEvent(&event);
    }
}

void I_InputInit(void) {
    buttons_init();
}

void I_GetEvent() {
    return I_GetEventTimeout(50);
}

void I_GetEventTimeout(int key_timeout) {
    buttons_getevent();

#if PICO_ON_DEVICE && !NO_USE_UART
    if (uart_is_readable(uart_default)) {
        char c = uart_getc(uart_default);
        if (c == 26 && uart_is_readable_within_us(uart_default, key_timeout)) {
            c = uart_getc(uart_default);
            static int modifiers = 0;
            switch (c) {
                case 0:
                    if (uart_is_readable_within_us(uart_default, key_timeout)) {
                        uint scancode = (uint8_t)uart_getc(uart_default);
                        if (scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT) {
                            modifiers |= WITH_SHIFT;
                        }
                        pico_key_down(scancode, 0, modifiers);
                    }
                    return;
                case 1:
                    if (uart_is_readable_within_us(uart_default, key_timeout)) {
                        uint scancode = (uint8_t)uart_getc(uart_default);
                        if (scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT) {
                            modifiers &= ~WITH_SHIFT;
                        }
                        pico_key_up(scancode, 0, modifiers);
                    }
                    return;
                case 2:
                case 3:
                case 5:
                    if (uart_is_readable_within_us(uart_default, key_timeout)) {
                        uart_getc(uart_default);
                    }
                    return;
                case 4:
                    if (uart_is_readable_within_us(uart_default, key_timeout)) {
                        uart_getc(uart_default);
                    }
                    if (uart_is_readable_within_us(uart_default, key_timeout)) {
                        uart_getc(uart_default);
                    }
                    return;
            }
        }
    }
#endif
}
