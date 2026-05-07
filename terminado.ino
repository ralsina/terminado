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
#include "term_config.h"
#include "vt100.h"
#include "iosevka.h"
#include "iosevka_bold.h"
#include "iosevka_italic.h"
#include "iosevka_bolditalic.h"

BBQ10Keyboard keyboard;
VT100 vt100;

// Modifier key states
bool fnKeyPressed = false;
bool ctrlKeyPressed = false;

// Status bar state
String terminalTitle = "VT100 Terminal";
const int serialBaud = 19200;
String debugMessage = "";
unsigned long debugMessageTime = 0;
const unsigned long DEBUG_MESSAGE_DURATION = 3000; // Show debug for 3 seconds
String lastStatusContent = "";  // Track last rendered content to avoid redraws
bool statusNeedsUpdate = true;   // Flag to force update when needed

// Callback for sending VT100 responses back to host
void vt100WriteCallback(const char* data, size_t len) {
  Serial.write(data, len);
}

// Callback for handling window title changes
void vt100TitleCallback(const char* title) {
  terminalTitle = title;
  // Trim title to reasonable length for display
  if (terminalTitle.length() > 30) {
    terminalTitle = terminalTitle.substring(0, 30) + "...";
  }
}

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

// Key autorepeat state
char lastRepeatedKey = '\0';
unsigned long lastRepeatTime = 0;
const unsigned long AUTOREPEAT_DELAY = 500;    // Initial delay before repeat (ms)
const unsigned long AUTOREPEAT_RATE = 100;     // Repeat rate (ms)
bool keyIsHeld = false;  // Track if key is currently held down

int termCellWidth = TERM_DEFAULT_CELL_WIDTH;
int termCellHeight = TERM_DEFAULT_CELL_HEIGHT;
uint16_t termFontFirst = 0x20;
uint16_t termFontLast = 0x7E;
int termCharOffsetX = 0;
int termCharOffsetY = 1;

void configureTerminalGeometryFromFont() {
  uint8_t baseWidth = TERM_DEFAULT_BASE_WIDTH;
  uint8_t baseHeight = TERM_DEFAULT_BASE_HEIGHT;
  int glyphMinX = 0;
  int glyphMinY = 0;
  int glyphVisualWidth = baseWidth;
  int glyphVisualHeight = baseHeight;

  #if USE_CUSTOM_FONT
  const GFXfont* selectedFont = &IosevkaNerdFontMono_Regular7pt8b;
  tft.setFont(selectedFont);

  uint16_t metricFirst = max(static_cast<uint16_t>(selectedFont->first), static_cast<uint16_t>(0x20));
  uint16_t metricLast = min(static_cast<uint16_t>(selectedFont->last), static_cast<uint16_t>(0x7E));
  if (metricLast < metricFirst) {
    metricFirst = selectedFont->first;
    metricLast = selectedFont->last;
  }

  termFontFirst = metricFirst;
  termFontLast = metricLast;
  baseHeight = selectedFont->yAdvance;

  int minX = 0;
  int maxX = 0;
  int maxAscent = 0;
  int maxDescent = 0;
  uint8_t maxAdvance = 0;
  bool haveGlyphBounds = false;

  for (uint16_t codepoint = metricFirst; codepoint <= metricLast; codepoint++) {
    const GFXglyph* glyph = &selectedFont->glyph[codepoint - selectedFont->first];

    if (glyph->xAdvance > maxAdvance) {
      maxAdvance = glyph->xAdvance;
    }

    if (glyph->width == 0 || glyph->height == 0) {
      continue;
    }

    int glyphLeft = glyph->xOffset;
    int glyphRight = glyph->xOffset + glyph->width;
    int glyphTop = glyph->yOffset;
    int glyphBottom = glyph->yOffset + glyph->height;

    if (!haveGlyphBounds) {
      minX = glyphLeft;
      maxX = glyphRight;
      maxAscent = max(0, -glyphTop);
      maxDescent = max(0, glyphBottom);
      haveGlyphBounds = true;
      continue;
    }

    minX = min(minX, glyphLeft);
    maxX = max(maxX, glyphRight);
    maxAscent = max(maxAscent, max(0, -glyphTop));
    maxDescent = max(maxDescent, max(0, glyphBottom));
  }

  if (maxAdvance > 0) {
    baseWidth = maxAdvance;
  }

  if (haveGlyphBounds) {
    glyphMinX = minX;
    glyphVisualWidth = max(static_cast<int>(baseWidth), maxX - minX);
    glyphVisualHeight = max(static_cast<int>(baseHeight), maxAscent + maxDescent);
    termCharOffsetX = max(0, -minX) * FONT_MULTIPLIER;
    termCharOffsetY = TERM_CELL_VPAD / 2;
  }
  #else
  tft.setFont(&fonts::Font0);
  termFontFirst = 0x20;
  termFontLast = 0x7E;
  #endif

  tft.setTextSize(FONT_MULTIPLIER);

  int scaledGlyphWidth = glyphVisualWidth * FONT_MULTIPLIER;
  int scaledGlyphHeight = glyphVisualHeight * FONT_MULTIPLIER;
  int scaledAdvanceWidth = baseWidth * FONT_MULTIPLIER;
  int scaledAdvanceHeight = baseHeight * FONT_MULTIPLIER;

  termCellWidth = max(scaledAdvanceWidth, scaledGlyphWidth) + TERM_CELL_HPAD;
  termCellHeight = max(scaledAdvanceHeight, scaledGlyphHeight) + TERM_CELL_VPAD;

  #if USE_CUSTOM_FONT
  termCharOffsetX += TERM_CELL_HPAD / 2;
  #else
  termCharOffsetX = TERM_CELL_HPAD / 2;
  termCharOffsetY = 1;
  #endif

  int runtimeCols = SCREEN_WIDTH / termCellWidth;
  int runtimeRows = (SCREEN_HEIGHT / termCellHeight) - 1;
  runtimeCols = constrain(runtimeCols, 1, MAX_TERM_COLS);
  runtimeRows = constrain(runtimeRows, 1, MAX_TERM_ROWS);
  vt100.setGeometry(runtimeCols, runtimeRows);
}

static inline uint8_t clampAnsiIndex(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 7) {
    return 7;
  }
  return static_cast<uint8_t>(value);
}

static inline char sanitizeGlyph(char c) {
  uint16_t glyph = static_cast<uint8_t>(c);
  if (glyph < termFontFirst || glyph > termFontLast) {
    return ' ';
  }
  return static_cast<char>(glyph);
}

// Status bar functions
void setStatusDebug(const char* msg) {
  debugMessage = msg;
  debugMessageTime = millis();
  statusNeedsUpdate = true;
}

void renderStatusBar() {
  // Determine what content to show
  String currentContent;
  if (millis() - debugMessageTime < DEBUG_MESSAGE_DURATION && debugMessage.length() > 0) {
    currentContent = "DEBUG: " + debugMessage;
  } else {
    currentContent = terminalTitle + " | " + String(serialBaud) + " baud";
  }

  // Only redraw if content changed
  if (currentContent != lastStatusContent || statusNeedsUpdate) {
    lastStatusContent = currentContent;
    statusNeedsUpdate = false;

    int py = TERM_OFFSET_Y + vt100.rows() * termCellHeight;

    // Clear the status bar
    tft.fillRect(0, py, SCREEN_WIDTH, termCellHeight, TFT_BLUE);

    tft.setCursor(2, py + 1);
    tft.setTextColor(TFT_WHITE, TFT_BLUE);
    tft.print(currentContent);
  }
}

void setup()
{
  Serial.begin(serialBaud); // Middle ground baud rate

  // Explicitly initialize modifier states
  fnKeyPressed = false;
  ctrlKeyPressed = false;

  // Initialize I2C with slower speed for BBQ20 keyboard compatibility
  Wire.begin(19, 20);  // I2C for Elecrow ESP32-S3 HMI: SDA=IO19, SCL=IO20
  Wire.setClock(100000); // Lower I2C speed to 100kHz for BBQ20 keyboard compatibility

  // Initialize keyboard after I2C is set up
  keyboard.begin();
  keyboard.setBacklight(0.5f); // 50% keyboard backlight

  // Display Prepare
  tft.begin();
  tft.setRotation(2); // Flip screen vertically (180 degree rotation)
  tft.fillScreen(TFT_BLACK);

  // Draw title
  tft.setCursor(200, 240);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("VT100 Terminal Ready");
  delay(500);
  tft.fillScreen(TFT_BLACK);

  // Set font and runtime geometry AFTER library is initialized.
  configureTerminalGeometryFromFont();

  // Initialize terminal
  vt100.setWriteCallback(vt100WriteCallback);
  vt100.setTitleCallback(vt100TitleCallback);
  vt100.clearScreen();

  // Report terminal size after connection is stable
  delay(500);
  char sizeReport[32];
  snprintf(sizeReport, sizeof(sizeReport), "\033[8;%d;%dt", vt100.rows(), vt100.cols());
  Serial.write(sizeReport);
}

void loop()
{
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
    handleKeyPress(key);  // Process ALL key events
  }

  // Handle cursor blinking
  if (millis() - lastCursorBlink > CURSOR_BLINK_INTERVAL) {
    cursorVisible = !cursorVisible;
    lastCursorBlink = millis();
    renderCursor();
  }

  // Handle autorepeat for held keys
  if (keyIsHeld && lastRepeatedKey != '\0') {
    unsigned long now = millis();
    if (now - lastRepeatTime > AUTOREPEAT_RATE) {
      // Time to repeat the key
      setStatusDebug("REPEAT");
      processKeyCharacter(lastRepeatedKey);
      lastRepeatTime = now;
    }
  }

  // Update status bar
  renderStatusBar();
}

void handleKeyPress(const BBQ10Keyboard::KeyEvent &key) {
  char c = key.key;

  // Handle ALL key events (press and release) in state machine fashion

  // Handle Fn key state (ASCII 7)
  if (c == 7) {
    if (key.state == BBQ10Keyboard::StatePress) {
      fnKeyPressed = true;
    } else if (key.state == BBQ10Keyboard::StateRelease) {
      fnKeyPressed = false;
    }
    return;
  }

  // Handle Control key state (ASCII 18)
  if (c == 18) {
    if (key.state == BBQ10Keyboard::StatePress) {
      ctrlKeyPressed = true;
    } else if (key.state == BBQ10Keyboard::StateRelease) {
      ctrlKeyPressed = false;
    }
    return;
  }

  // Handle key release - stop autorepeat
  if (key.state == BBQ10Keyboard::StateRelease) {
    if (c == lastRepeatedKey) {
      lastRepeatedKey = '\0';  // Stop repeating this key
      keyIsHeld = false;
    }
    return;
  }

  // Handle LongPress events - mark key as held
  if (key.state == BBQ10Keyboard::StateLongPress) {
    if ((c >= 32 && c <= 126) && c != 7 && c != 18 && c != 5) {
      if (c == lastRepeatedKey) {
        keyIsHeld = true;
        lastRepeatTime = millis();  // Reset timing for continuous repeat
      }
    }
    return;
  }

  // For regular Press events, always process
  if (key.state == BBQ10Keyboard::StatePress) {
    // Track this key for potential autorepeat
    if ((c >= 32 && c <= 126) && c != 7 && c != 18 && c != 5) {
      lastRepeatedKey = c;
      keyIsHeld = false;  // Not yet held, just pressed
      lastRepeatTime = millis();
    }
    // Process the key normally
    processKeyCharacter(c);
    return;
  }

  // Ignore any other states
  return;
}

// Helper function to process character input
void processKeyCharacter(char c) {

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
      // Check for Control key before sending normal character
      if (ctrlKeyPressed) {
        Serial.write(c & 0x1F);  // Ctrl+W
        return;
      }
      Serial.write('w');  // Send normal 'w' when Fn not pressed
      break;

    case 'a':
    case 'A':
      if (fnKeyPressed) {
        Serial.write("\033[D");  // Left arrow
        return;
      }
      // Check for Control key before sending normal character
      if (ctrlKeyPressed) {
        Serial.write(c & 0x1F);  // Ctrl+A
        return;
      }
      Serial.write('a');  // Send normal 'a' when Fn not pressed
      break;

    case 's':
    case 'S':
      if (fnKeyPressed) {
        Serial.write("\033[B");  // Down arrow
        return;
      }
      // Check for Control key before sending normal character
      if (ctrlKeyPressed) {
        Serial.write(c & 0x1F);  // Ctrl+S
        return;
      }
      Serial.write('s');  // Send normal 's' when Fn not pressed
      break;

    case 'd':
    case 'D':
      if (fnKeyPressed) {
        Serial.write("\033[C");  // Right arrow
        return;
      }
      // Check for Control key before sending normal character
      if (ctrlKeyPressed) {
        Serial.write(c & 0x1F);  // Ctrl+D
        return;
      }
      Serial.write('d');  // Send normal 'd' when Fn not pressed
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
        char ctrlChar = c & 0x1F;

        // Debug: show what we're sending
        String debugMsg = "CTRL+";
        debugMsg += c;
        debugMsg += " = ";
        debugMsg += (int)ctrlChar;
        setStatusDebug(debugMsg.c_str());

        Serial.write(ctrlChar);
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
  static char lastScreen[MAX_TERM_BUFFER_SIZE];
  static VT100Attr lastAttrs[MAX_TERM_BUFFER_SIZE];
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

// Draw a VT100 line-drawing character using primitives.
// c is the ASCII letter used in graphics mode (e.g. 'q' = horizontal line).
void drawLineDrawingChar(int px, int py, char c, uint32_t fg) {
  int cx = px + termCellWidth / 2;   // horizontal center of cell
  int cy = py + termCellHeight / 2;  // vertical center of cell
  int r = px + termCellWidth - 1;    // right edge
  int b = py + termCellHeight - 1;   // bottom edge

  switch (c) {
    case 'q':  // ─ horizontal line
      tft.drawFastHLine(px, cy, termCellWidth, fg);
      break;
    case 'x':  // │ vertical line
      tft.drawFastVLine(cx, py, termCellHeight, fg);
      break;
    case 'j':  // ┘ lower-right corner
      tft.drawFastHLine(px, cy, cx - px + 1, fg);
      tft.drawFastVLine(cx, py, cy - py + 1, fg);
      break;
    case 'k':  // ┐ upper-right corner
      tft.drawFastHLine(px, cy, cx - px + 1, fg);
      tft.drawFastVLine(cx, cy, b - cy + 1, fg);
      break;
    case 'l':  // ┌ upper-left corner
      tft.drawFastHLine(cx, cy, r - cx + 1, fg);
      tft.drawFastVLine(cx, cy, b - cy + 1, fg);
      break;
    case 'm':  // └ lower-left corner
      tft.drawFastHLine(cx, cy, r - cx + 1, fg);
      tft.drawFastVLine(cx, py, cy - py + 1, fg);
      break;
    case 'n':  // ┼ cross
      tft.drawFastHLine(px, cy, termCellWidth, fg);
      tft.drawFastVLine(cx, py, termCellHeight, fg);
      break;
    case 't':  // ├ T right
      tft.drawFastHLine(cx, cy, r - cx + 1, fg);
      tft.drawFastVLine(cx, py, termCellHeight, fg);
      break;
    case 'u':  // ┤ T left
      tft.drawFastHLine(px, cy, cx - px + 1, fg);
      tft.drawFastVLine(cx, py, termCellHeight, fg);
      break;
    case 'v':  // ┴ T up
      tft.drawFastHLine(px, cy, termCellWidth, fg);
      tft.drawFastVLine(cx, py, cy - py + 1, fg);
      break;
    case 'w':  // ┬ T down
      tft.drawFastHLine(px, cy, termCellWidth, fg);
      tft.drawFastVLine(cx, cy, b - cy + 1, fg);
      break;
    case 'a':  // ▒ checkerboard (stipple)
      for (int row = py; row <= b; row += 2) {
        for (int col = (row % 4 == 0) ? px : px + 1; col <= r; col += 2) {
          tft.drawPixel(col, row, fg);
        }
      }
      break;
    case '`':  // ◆ diamond — draw as small diamond
      tft.drawLine(cx, py + 2, r - 2, cy, fg);
      tft.drawLine(r - 2, cy, cx, b - 2, fg);
      tft.drawLine(cx, b - 2, px + 2, cy, fg);
      tft.drawLine(px + 2, cy, cx, py + 2, fg);
      break;
    default:
      break;  // unknown graphics char — draw nothing
  }
}

void renderChar(int x, int y, char c) {
  // Get character attributes
  VT100Attr attr = vt100.getAttr(x, y);
  char printable = sanitizeGlyph(c);

  // Calculate position
  int px = TERM_OFFSET_X + x * termCellWidth;
  int py = TERM_OFFSET_Y + y * termCellHeight;

  // Get colors
  uint32_t fg = ansi_colors[clampAnsiIndex(attr.fg)];
  uint32_t bg = ansi_colors[clampAnsiIndex(attr.bg)];

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
  tft.fillRect(px, py, termCellWidth, termCellHeight, bg);

  // Only draw character if it's not a space
  if (printable != ' ') {
    if (attr.graphics) {
      drawLineDrawingChar(px, py, printable, fg);
    } else {
      #if USE_CUSTOM_FONT
      if (attr.bold && attr.italic)
        tft.setFont(&IosevkaNerdFontMono_BoldItalic7pt8b);
      else if (attr.bold)
        tft.setFont(&IosevkaNerdFontMono_Bold7pt8b);
      else if (attr.italic)
        tft.setFont(&IosevkaNerdFontMono_Italic7pt8b);
      else
        tft.setFont(&IosevkaNerdFontMono_Regular7pt8b);
      #endif
      tft.setCursor(px + termCharOffsetX, py + termCharOffsetY);
      tft.setTextColor(fg, bg);
      tft.print(printable);
      #if USE_CUSTOM_FONT
      tft.setFont(&IosevkaNerdFontMono_Regular7pt8b);  // restore default
      #endif
    }
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

  int px = TERM_OFFSET_X + cx * termCellWidth;
  int py = TERM_OFFSET_Y + cy * termCellHeight;

  if (cursorVisible) {
    // Draw cursor as inverted block
    char c = sanitizeGlyph(vt100.getChar(cx, cy));
    VT100Attr attr = vt100.getAttr(cx, cy);

    uint32_t fg = ansi_colors[clampAnsiIndex(attr.fg)];
    uint32_t bg = ansi_colors[clampAnsiIndex(attr.bg)];

    if (attr.reverse) {
      uint32_t temp = fg;
      fg = bg;
      bg = temp;
    }

    tft.fillRect(px, py, termCellWidth, termCellHeight, fg);
    tft.setCursor(px + termCharOffsetX, py + termCharOffsetY);
    tft.setTextColor(bg, fg);
    if (c != ' ') {
      tft.print(c);
    }
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
