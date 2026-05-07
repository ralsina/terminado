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

// Modifier key states with timeout
bool fnKeyPressed = false;
bool ctrlKeyPressed = false;
unsigned long fnKeyTime = 0;
unsigned long ctrlKeyTime = 0;
const unsigned long MODIFIER_TIMEOUT = 1000; // 1 second timeout

// Callback for sending VT100 responses back to host
void vt100WriteCallback(const char* data, size_t len) {
  Serial.write(data, len);
}

// Display configuration - set multiplier and everything is calculated
#define FONT_MULTIPLIER 1  // 1x scaling for smaller text (more columns)
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// Calculate cell size from font multiplier (Font0 base is 8x8 pixels)
#define TERM_CELL_WIDTH (8 * FONT_MULTIPLIER + 1)  // +1 for spacing
#define TERM_CELL_HEIGHT (8 * FONT_MULTIPLIER + 2) // +2 for line spacing
#define TERM_OFFSET_X 0
#define TERM_OFFSET_Y 0

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

// Software flow control
bool flowControlPaused = false;
unsigned long lastFlowControlCheck = 0;
const unsigned long FLOW_CONTROL_INTERVAL = 100; // Check every 100ms

void setup()
{
  Serial.begin(19200); // Middle ground baud rate

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
  tft.setTextSize(FONT_MULTIPLIER); // Use configured multiplier
  tft.fillScreen(TFT_BLACK);

  // Draw title
  tft.setCursor(200, 240);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("VT100 Terminal Ready");
  delay(500);
  tft.fillScreen(TFT_BLACK);

  // Initialize terminal
  vt100.setWriteCallback(vt100WriteCallback);
  vt100.clearScreen();

  // Report terminal size after connection is stable
  delay(500);
  char sizeReport[32];
  snprintf(sizeReport, sizeof(sizeReport), "\033[8;%d;%dt", vt100.rows(), vt100.cols());
  Serial.write(sizeReport);
}

void loop()
{
  // Check modifier key timeouts
  if (fnKeyPressed && millis() - fnKeyTime > MODIFIER_TIMEOUT) {
    fnKeyPressed = false;
  }
  if (ctrlKeyPressed && millis() - ctrlKeyTime > MODIFIER_TIMEOUT) {
    ctrlKeyPressed = false;
  }

  // Check serial buffer level for flow control
  if (millis() - lastFlowControlCheck > FLOW_CONTROL_INTERVAL) {
    int bufferAvailable = Serial.available();
    if (bufferAvailable > 500 && !flowControlPaused) {
      // Buffer getting full, pause transmission
      Serial.write(0x13); // XOFF (DC3)
      flowControlPaused = true;
    } else if (bufferAvailable < 100 && flowControlPaused) {
      // Buffer has space, resume transmission
      Serial.write(0x11); // XON (DC1)
      flowControlPaused = false;
    }
    lastFlowControlCheck = millis();
  }

  // Handle incoming serial data (from host to terminal)
  if (Serial.available()) {
    // Process all available data as fast as possible
    while (Serial.available()) {
      char c = Serial.read();
      vt100.process(c);
    }
    // Only render once per batch of data
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

  // Handle Fn key state (ASCII 7)
  if (c == 7) {
    if (key.state == BBQ10Keyboard::StatePress) {
      fnKeyPressed = true;
      fnKeyTime = millis();
    }
    return;
  }

  // Handle Control key state (ASCII 18)
  if (c == 18) {
    if (key.state == BBQ10Keyboard::StatePress) {
      ctrlKeyPressed = true;
      ctrlKeyTime = millis();
    }
    return;
  }

  // Special key mappings for BBQ20 keyboard
  // 5 = escape, 6 = left, 17 = down, 18 = control, 7 = Fn
  switch (c) {
    case 5:    // Escape key
      Serial.write('\e');
      return;

    case 'w':
    case 'W':
      if (fnKeyPressed) {
        Serial.write("\033[A");  // Up arrow
        return;
      }
      break;

    case 'a':
    case 'A':
      if (fnKeyPressed) {
        Serial.write("\033[D");  // Left arrow
        return;
      }
      break;

    case 's':
    case 'S':
      if (fnKeyPressed) {
        Serial.write("\033[B");  // Down arrow
        return;
      }
      break;

    case 'd':
    case 'D':
      if (fnKeyPressed) {
        Serial.write("\033[C");  // Right arrow
        return;
      }
      break;

    case '\n':  // Enter key
      Serial.write('\r');  // Just send CR, let terminal handle newline
      break;

    case '\b':  // Backspace
      Serial.write('\b');
      break;

    case '\t':  // Tab
      Serial.write('\t');
      break;

    default:
      // Handle Control key combinations
      if (ctrlKeyPressed && c >= 32 && c <= 126) {
        // Send control character (subtract 64 from ASCII value)
        Serial.write(c & 0x1F);
        return;
      }

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
    // Position character in cell (1px down for better centering)
    int charOffset = FONT_MULTIPLIER > 1 ? 1 : 0;
    tft.setCursor(px + charOffset, py + 1); // Always move 1px down
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
