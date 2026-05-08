#ifndef TERM_CONFIG_H
#define TERM_CONFIG_H

// Shared terminal display/layout configuration.
// This file is included by both the renderer and VT100 core so dimensions stay in sync.

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// Set to 1 to use the custom GFXfont, 0 to use built-in Font0.
#define USE_CUSTOM_FONT 1

// Text scaling for the selected font.
#define FONT_MULTIPLIER 1

#if USE_CUSTOM_FONT
// Padding used around glyphs in each terminal cell.
#define TERM_CELL_HPAD 2
#define TERM_CELL_VPAD 2
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
// 4pt font has xAdvance=4, yAdvance=10; with padding: 6x12 per cell.
#define TERM_MIN_CELL_WIDTH  6
#define TERM_MIN_CELL_HEIGHT 10

#define MAX_TERM_COLS (SCREEN_WIDTH / TERM_MIN_CELL_WIDTH)
#define MAX_TERM_ROWS ((SCREEN_HEIGHT / TERM_MIN_CELL_HEIGHT) - 1)
#define MAX_TERM_BUFFER_SIZE (MAX_TERM_COLS * MAX_TERM_ROWS)

#define TERM_OFFSET_X 0
#define TERM_OFFSET_Y 0

#endif // TERM_CONFIG_H