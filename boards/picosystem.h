// This isn't anything official, and doesn't match the
// actual thumby very well. It's just copied from the
// PIMORONI_TINY2040 header file with a few changes that
// made it work in this one case.

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

#ifndef _BOARDS_PICOSYSTEM_H
#define _BOARDS_PICOSYSTEM_H

// For board detection
#define PICOSYSTEM 1

// --- BOARD SPECIFIC ---
#define PICOSYSTEM_LCD_CS 5
#define PICOSYSTEM_LCD_RESET 4
#define PICOSYSTEM_LCD_DC 9
#define PICOSYSTEM_LCD_VSYNC 8
#define PICOSYSTEM_LCD_BACKLIGHT 12
#define PICOSYSTEM_LCD_FRAME_PERIOD 5556

// Compatibility with J_OLED naming for existing code
#define J_OLED_CS PICOSYSTEM_LCD_CS
#define J_OLED_RESET PICOSYSTEM_LCD_RESET
#define J_OLED_DC PICOSYSTEM_LCD_DC
#define J_OLED_FRAME_PERIOD PICOSYSTEM_LCD_FRAME_PERIOD

// RGB LED pins
#define PICOSYSTEM_PIN_RED 14
#define PICOSYSTEM_PIN_GREEN 13
#define PICOSYSTEM_PIN_BLUE 15

// Audio pin
#define PICOSYSTEM_PIN_AUDIO 11

// Button pins (accent active low)
#define PICOSYSTEM_BTN_UP 23
#define PICOSYSTEM_BTN_DOWN 20
#define PICOSYSTEM_BTN_LEFT 22
#define PICOSYSTEM_BTN_RIGHT 21
#define PICOSYSTEM_BTN_A 18
#define PICOSYSTEM_BTN_B 19
#define PICOSYSTEM_BTN_X 17
#define PICOSYSTEM_BTN_Y 16


// --- UART ---
#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 0
#endif

#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN 0
#endif

#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN 1
#endif

// --- LED ---
// Included so basic examples will work
#ifndef PICO_DEFAULT_LED_PIN
// #define PICO_DEFAULT_LED_PIN 8
#define PICO_DEFAULT_LED_PIN 14
#endif
// no PICO_DEFAULT_WS2812_PIN

#ifndef PICO_DEFAULT_LED_PIN_INVERTED
#define PICO_DEFAULT_LED_PIN_INVERTED 1
#endif

// --- I2C ---
#ifndef PICO_DEFAULT_I2C
#define PICO_DEFAULT_I2C 1
#endif
#ifndef PICO_DEFAULT_I2C_SDA_PIN
#define PICO_DEFAULT_I2C_SDA_PIN 2
#endif
#ifndef PICO_DEFAULT_I2C_SCL_PIN
#define PICO_DEFAULT_I2C_SCL_PIN 3
#endif

// --- SPI ---
#ifndef PICO_DEFAULT_SPI
#define PICO_DEFAULT_SPI 0
#endif
#ifndef PICO_DEFAULT_SPI_SCK_PIN
// #define PICO_DEFAULT_SPI_SCK_PIN 18
#define PICO_DEFAULT_SPI_SCK_PIN 6
#endif
#ifndef PICO_DEFAULT_SPI_TX_PIN
// #define PICO_DEFAULT_SPI_TX_PIN 19
#define PICO_DEFAULT_SPI_TX_PIN 7
#endif
#ifndef PICO_DEFAULT_SPI_RX_PIN
// #define PICO_DEFAULT_SPI_RX_PIN 4
#define PICO_DEFAULT_SPI_RX_PIN 10  // Set to an unused pin
#endif
#ifndef PICO_DEFAULT_SPI_CSN_PIN
#define PICO_DEFAULT_SPI_CSN_PIN 5
#endif

// --- FLASH ---
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
#endif

// All boards have B1 RP2040
#ifndef PICO_RP2040_B0_SUPPORTED
#define PICO_RP2040_B0_SUPPORTED 0
#endif

#endif
