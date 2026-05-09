#ifndef TERM_CONFIG_H
#define TERM_CONFIG_H

// Shared terminal display/layout configuration.
// This file is included by both the renderer and VT100 core so dimensions stay in sync.

// I2C Configuration for BBQ20 Keyboard
// ================================================================
// Board-specific I2C pin configurations for different hardware platforms
// ================================================================

#ifdef ESP32_2432S028R
  // ESP32-2432S028R (Cheap Yellow Display)
  // Uses CN1 connector for I2C
  #define I2C_SDA_PIN 22  // CN1 Blue wire
  #define I2C_SCL_PIN 27  // CN1 Yellow wire
  #define I2C_BOARD "CYD (CN1: GPIO_22/GPIO_27)"

  // Screen dimensions - must match gfx_conf.h
  #define SCREEN_WIDTH 320
  #define SCREEN_HEIGHT 240

#else
  // Elecrow 5" HMI Display (original configuration)
  #define I2C_SDA_PIN 19  // SDA=IO19
  #define I2C_SCL_PIN 20  // SCL=IO20
  #define I2C_BOARD "Elecrow 5\" HMI (GPIO_19/GPIO_20)"

  // Screen dimensions - must match gfx_conf.h
  #define SCREEN_WIDTH 800
  #define SCREEN_HEIGHT 480
#endif

// Set to 1 to use the custom GFXfont, 0 to use built-in Font0.
#define USE_CUSTOM_FONT 1

// Text scaling for the selected font.
#define FONT_MULTIPLIER 1

#if USE_CUSTOM_FONT
// Padding used around glyphs in each terminal cell.
// Reduced vertical padding to fit more rows on small 240x320 display
#define TERM_CELL_HPAD 2
#define TERM_CELL_VPAD 1  // Reduced from 2 to 1
#else
#define TERM_CELL_HPAD 1
#define TERM_CELL_VPAD 2
#endif

// Default geometry uses Font0-style metrics before runtime font probing.
#define TERM_DEFAULT_BASE_WIDTH 8
#define TERM_DEFAULT_BASE_HEIGHT 8

#define TERM_DEFAULT_CELL_WIDTH ((TERM_DEFAULT_BASE_WIDTH * FONT_MULTIPLIER) + TERM_CELL_HPAD)
#define TERM_DEFAULT_CELL_HEIGHT ((TERM_DEFAULT_BASE_HEIGHT * FONT_MULTIPLIER) + TERM_CELL_VPAD)

#define TERM_DEFAULT_COLS (SCREEN_WIDTH / TERM_DEFAULT_CELL_WIDTH)
#define TERM_DEFAULT_ROWS ((SCREEN_HEIGHT / TERM_DEFAULT_CELL_HEIGHT) - 1)

// Maximum buffer size is based on the smallest supported cell dimensions.
// Tom Thumb has xAdvance=4, yAdvance=6; with HPAD=2: 6x6 per cell.
// 4pt Iosevka has xAdvance≈4, yAdvance=10; with HPAD=2: 6x10 per cell.
// 6pt Iosevka has xAdvance≈6, yAdvance=14; with HPAD=2: 8x14 per cell.
#define TERM_MIN_CELL_WIDTH  6
#define TERM_MIN_CELL_HEIGHT 6  // Tom Thumb yAdvance=6 (smallest)

// Maximum size for Tom Thumb on 320x240 display:
// Columns: 320/6 ≈ 53 chars
// Rows: (240/6)-1 = 39 chars (minus 1 for status bar)
#define MAX_TERM_COLS 53
#define MAX_TERM_ROWS 39
#define MAX_TERM_BUFFER_SIZE (MAX_TERM_COLS * MAX_TERM_ROWS)  // 2067 chars max

#define TERM_OFFSET_X 0
#define TERM_OFFSET_Y 0

#endif // TERM_CONFIG_H