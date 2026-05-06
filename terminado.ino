/**************************Terminado - VT100 Terminal Emulator for ESP32-S3************************
Version     :	2.0
Suitable for:	CrowPanel ESP32 HMI Display 5.0 inch
Product link:	https://www.elecrow.com/esp32-display-series-hmi-touch-screen.html
Description	:	VT100 terminal emulator with BBQ20 keyboard and serial communication
********************************************************************************/

#include <Wire.h>
#include <SPI.h>
#include "gfx_conf.h"
#include <BBQ10Keyboard.h>
#include "vt100.h"

BBQ10Keyboard keyboard;
VT100 vt100;

// Display configuration
#define TERM_OFFSET_X 0
#define TERM_OFFSET_Y 0
#define TERM_CELL_WIDTH 16
#define TERM_CELL_HEIGHT 16

// Color mapping for ANSI colors
static const uint32_t ansi_colors[8] = {
    TFT_BLACK,   // 0: Black
    0xFF0000,    // 1: Red
    0x00FF00,    // 2: Green
    0xFFFF00,    // 3: Yellow
    0x0000FF,    // 4: Blue
    0xFF00FF,    // 5: Magenta
    0x00FFFF,    // 6: Cyan
    0xFFFFFF     // 7: White
};

// Cursor blink state
bool cursorVisible = true;
unsigned long lastCursorBlink = 0;
const unsigned long CURSOR_BLINK_INTERVAL = 500;

// Track previous cursor position for proper restoration
int prevCursorX = -1;
int prevCursorY = -1;

void setup()
{
  Serial.begin(115200);

  // Initialize I2C with slower speed for BBQ20 keyboard compatibility
  Wire.begin(19, 20);  // I2C for Elecrow ESP32-S3 HMI: SDA=IO19, SCL=IO20
  Wire.setClock(100000); // Lower I2C speed to 100kHz for BBQ20 keyboard compatibility

  // Initialize keyboard after I2C is set up
  keyboard.begin();
  keyboard.setBacklight(0.5f); // 50% keyboard backlight

  // Display Prepare
  tft.begin();
  tft.setRotation(2); // Flip screen vertically (180 degree rotation)
  tft.setFont(&fonts::Font0); // Use built-in Font0 for terminal display
  tft.setTextSize(2); // 2x scaling for better readability
  tft.fillScreen(TFT_BLACK);

  // Draw title
  tft.setCursor(200, 240);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("VT100 Terminal Ready");
  delay(1000);
  tft.fillScreen(TFT_BLACK);

  // Initialize terminal
  vt100.clearScreen();

  Serial.println("VT100 Terminal Emulator Ready");
  Serial.println("Send terminal data via Serial at 115200 baud");
}

void loop()
{
  // Handle incoming serial data (from host to terminal)
  if (Serial.available()) {
    while (Serial.available()) {
      char c = Serial.read();
      vt100.process(c);
    }
    renderTerminal();
  }

  // Handle keyboard input (from terminal to host)
  const int keyCount = keyboard.keyCount();
  if (keyCount > 0) {
    const BBQ10Keyboard::KeyEvent key = keyboard.keyEvent();

    if (key.state == BBQ10Keyboard::StatePress) {
      handleKeyPress(key);
    }
  }

  // Handle cursor blinking
  if (millis() - lastCursorBlink > CURSOR_BLINK_INTERVAL) {
    cursorVisible = !cursorVisible;
    lastCursorBlink = millis();
    renderCursor();
  }
}

void handleKeyPress(const BBQ10Keyboard::KeyEvent &key) {
  char c = key.key;

  // Handle special keys
  switch (c) {
    case '\n':  // Enter key
      Serial.write('\r');
      Serial.write('\n');
      break;

    case '\b':  // Backspace
      Serial.write('\b');
      break;

    case '\t':  // Tab
      Serial.write('\t');
      break;

    case 27:   // Escape
      Serial.write('\e');
      break;

    default:
      // Regular characters
      if (c >= 32 && c <= 126) {
        Serial.write(c);
      }
      break;
  }
}

void renderTerminal() {
  static char lastScreen[TERM_COLS * TERM_ROWS];
  static VT100Attr lastAttrs[TERM_COLS * TERM_ROWS];
  static bool initialized = false;

  if (!initialized) {
    memset(lastScreen, 0, sizeof(lastScreen));
    memset(lastAttrs, 0, sizeof(lastAttrs));
    initialized = true;
  }

  // Clear previous cursor position before rendering
  if (prevCursorX >= 0 && prevCursorY >= 0) {
    renderChar(prevCursorX, prevCursorY, vt100.getChar(prevCursorX, prevCursorY));
    prevCursorX = -1;
    prevCursorY = -1;
  }

  // Only redraw changed characters or attributes for efficiency
  for (int y = 0; y < vt100.rows(); y++) {
    for (int x = 0; x < vt100.cols(); x++) {
      int idx = y * vt100.cols() + x;
      char c = vt100.getChar(x, y);
      VT100Attr attr = vt100.getAttr(x, y);

      // Check if character or attributes changed
      if (c != lastScreen[idx] ||
          attr.fg != lastAttrs[idx].fg ||
          attr.bg != lastAttrs[idx].bg ||
          attr.bold != lastAttrs[idx].bold ||
          attr.underline != lastAttrs[idx].underline ||
          attr.reverse != lastAttrs[idx].reverse) {

        lastScreen[idx] = c;
        lastAttrs[idx] = attr;
        renderChar(x, y, c);
      }
    }
  }

  // Draw cursor at new position
  renderCursor();
}

void renderChar(int x, int y, char c) {
  // Get character attributes
  VT100Attr attr = vt100.getAttr(x, y);

  // Calculate position
  int px = TERM_OFFSET_X + x * TERM_CELL_WIDTH;
  int py = TERM_OFFSET_Y + y * TERM_CELL_HEIGHT;

  // Get colors
  uint32_t fg = ansi_colors[attr.fg];
  uint32_t bg = ansi_colors[attr.bg];

  // Handle reverse video
  if (attr.reverse) {
    uint32_t temp = fg;
    fg = bg;
    bg = temp;
  }

  // Handle bold (brighter colors)
  if (attr.bold && fg != TFT_BLACK) {
    fg = brightenColor(fg);
  }

  // Always fill background first (important for empty cells)
  tft.fillRect(px, py, TERM_CELL_WIDTH, TERM_CELL_HEIGHT, bg);

  // Only draw character if it's not a space
  if (c != ' ') {
    // Center character in the cell (Font0 at 2x is 16x16 pixels)
    tft.setCursor(px, py); // Position at top-left of cell
    tft.setTextColor(fg, bg);
    tft.print(c);
  }
}

void renderCursor() {
  int cx = vt100.cursorX();
  int cy = vt100.cursorY();

  // Restore previous cursor position first
  if (prevCursorX >= 0 && prevCursorY >= 0) {
    if (prevCursorX != cx || prevCursorY != cy) {
      // Cursor moved, restore old position
      renderChar(prevCursorX, prevCursorY, vt100.getChar(prevCursorX, prevCursorY));
    }
  }

  // Update previous cursor position
  prevCursorX = cx;
  prevCursorY = cy;

  int px = TERM_OFFSET_X + cx * TERM_CELL_WIDTH;
  int py = TERM_OFFSET_Y + cy * TERM_CELL_HEIGHT;

  if (cursorVisible) {
    // Draw cursor as inverted block
    char c = vt100.getChar(cx, cy);
    VT100Attr attr = vt100.getAttr(cx, cy);

    uint32_t fg = ansi_colors[attr.fg];
    uint32_t bg = ansi_colors[attr.bg];

    if (attr.reverse) {
      uint32_t temp = fg;
      fg = bg;
      bg = temp;
    }

    tft.fillRect(px, py, TERM_CELL_WIDTH, TERM_CELL_HEIGHT, fg);
    tft.setCursor(px, py);
    tft.setTextColor(bg, fg);
    tft.print(c);
  } else {
    // Restore normal character (cursor invisible)
    renderChar(cx, cy, vt100.getChar(cx, cy));
  }
}

uint32_t brightenColor(uint32_t color) {
  // Brighten color by adding 128 to each RGB component
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;

  r = min(255, r + 128);
  g = min(255, g + 128);
  b = min(255, b + 128);

  return (r << 16) | (g << 8) | b;
}
