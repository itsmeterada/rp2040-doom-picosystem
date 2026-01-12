// ThumbyColor display configuration
// 128x128 GC9107 LCD

#ifndef THUMBYCOLOR_VIDEO_CONFIG_H
#define THUMBYCOLOR_VIDEO_CONFIG_H

// Internal rendering resolution
// 128x80 maintains DOOM's original 1.6:1 aspect ratio
// Displayed centered on 128x128 with 24 pixel black bars (top/bottom)
#define TC_SCREENWIDTH 128
#define TC_SCREENHEIGHT 80

// Display resolution
#define TC_DISPLAYWIDTH 128
#define TC_DISPLAYHEIGHT 128

// Vertical offset for centering (128-80)/2 = 24
#define TC_Y_OFFSET 24

// GPIO pin definitions for ThumbyColor
#define TC_LCD_SDA_PIN   19
#define TC_LCD_SCK_PIN   18
#define TC_LCD_CS_PIN    17
#define TC_LCD_DC_PIN    16
#define TC_LCD_RST_PIN   4
#define TC_LCD_BL_PIN    7

// Button pins
#define TC_BTN_UP     1
#define TC_BTN_DOWN   3
#define TC_BTN_LEFT   0
#define TC_BTN_RIGHT  2
#define TC_BTN_A      21
#define TC_BTN_B      25
#define TC_BTN_L      6
#define TC_BTN_R      22
#define TC_BTN_MENU   26

// Audio pins
#define TC_AUDIO_PWM  23
#define TC_AUDIO_EN   20

// RGB LED pins
#define TC_LED_R      11
#define TC_LED_G      10
#define TC_LED_B      12

#endif // THUMBYCOLOR_VIDEO_CONFIG_H
