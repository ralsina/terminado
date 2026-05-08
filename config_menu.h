/*
 * config_menu.h — Overlay configuration menu for Terminado
 *
 * Trigger: Fn + ESC
 * Navigate: Fn+W (up) / Fn+S (down)
 * Change value: Fn+A (left) / Fn+D (right)
 * Exit: ESC or Fn+ESC
 */

#ifndef CONFIG_MENU_H
#define CONFIG_MENU_H

#include "gfx_conf.h"
#include "term_config.h"

// ── Persistent settings ───────────────────────────────────────────────────────

struct TermConfig {
    int  baudIndex;       // index into BAUD_RATES[]
    int  dataBits;        // 5,6,7,8
    int  stopBits;        // 1,2
    int  parityIndex;     // 0=None, 1=Even, 2=Odd
    bool xonXoff;         // software flow control
};

static const int BAUD_RATES[] = {
    300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400
};
static const int BAUD_COUNT = sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);
static const char* PARITY_NAMES[] = { "None", "Even", "Odd" };

// Default settings
static TermConfig termConfig = {
    .baudIndex   = 5,    // 19200
    .dataBits    = 8,
    .stopBits    = 1,
    .parityIndex = 0,    // None
    .xonXoff     = true
};

// ── Menu state ────────────────────────────────────────────────────────────────

static bool menuActive = false;
static int  menuSelectedRow = 0;

// ── Layout constants ──────────────────────────────────────────────────────────

static const int MENU_W       = 360;
static const int MENU_H       = 230;
static const int MENU_X       = (SCREEN_WIDTH  - MENU_W) / 2;
static const int MENU_Y       = (SCREEN_HEIGHT - MENU_H) / 2;
static const int MENU_ROWS    = 5;   // number of setting rows
static const int ROW_H        = 30;
static const int LABEL_X      = MENU_X + 12;
static const int VALUE_X      = MENU_X + 200;
static const int FIRST_ROW_Y  = MENU_Y + 52;

static const uint32_t COL_BG       = 0x1A1A2E;
static const uint32_t COL_TITLE_BG = 0x16213E;
static const uint32_t COL_SEL_BG   = 0x0F3460;
static const uint32_t COL_BORDER   = 0x533483;
static const uint32_t COL_TEXT     = TFT_WHITE;
static const uint32_t COL_VALUE    = 0xADD8E6; // light blue
static const uint32_t COL_HINT     = 0x888888;

// ── Helpers ───────────────────────────────────────────────────────────────────

static void menuDrawRow(int row, bool selected) {
    int ry = FIRST_ROW_Y + row * ROW_H;
    uint32_t bg = selected ? COL_SEL_BG : COL_BG;

    tft.fillRect(MENU_X + 2, ry, MENU_W - 4, ROW_H - 2, bg);

    tft.setTextColor(COL_TEXT, bg);
    tft.setCursor(LABEL_X, ry + 8);
    tft.setFont(nullptr);
    tft.setTextSize(1);

    // Label
    switch (row) {
        case 0: tft.print("Baud Rate");   break;
        case 1: tft.print("Data Bits");   break;
        case 2: tft.print("Stop Bits");   break;
        case 3: tft.print("Parity");      break;
        case 4: tft.print("XON/XOFF");    break;
    }

    // Value
    tft.setTextColor(COL_VALUE, bg);
    tft.setCursor(VALUE_X, ry + 8);
    switch (row) {
        case 0: tft.print(BAUD_RATES[termConfig.baudIndex]); break;
        case 1: tft.print(termConfig.dataBits);              break;
        case 2: tft.print(termConfig.stopBits);              break;
        case 3: tft.print(PARITY_NAMES[termConfig.parityIndex]); break;
        case 4: tft.print(termConfig.xonXoff ? "On" : "Off"); break;
    }

    // Arrow hints on selected row
    if (selected) {
        tft.setTextColor(COL_HINT, bg);
        tft.setCursor(MENU_X + MENU_W - 30, ry + 8);
        tft.print("< >");
    }
}

static void menuDraw() {
    // Outer border
    tft.drawRect(MENU_X - 1, MENU_Y - 1, MENU_W + 2, MENU_H + 2, COL_BORDER);
    tft.fillRect(MENU_X, MENU_Y, MENU_W, MENU_H, COL_BG);

    // Title bar
    tft.fillRect(MENU_X, MENU_Y, MENU_W, 40, COL_TITLE_BG);
    tft.setFont(nullptr);
    tft.setTextSize(1);
    tft.setTextColor(COL_TEXT, COL_TITLE_BG);
    tft.setCursor(MENU_X + 12, MENU_Y + 14);
    tft.print("Terminal Settings");

    // Divider
    tft.drawFastHLine(MENU_X, MENU_Y + 40, MENU_W, COL_BORDER);

    // Rows
    for (int i = 0; i < MENU_ROWS; i++) {
        menuDrawRow(i, i == menuSelectedRow);
    }

    // Bottom hint
    int hintY = MENU_Y + MENU_H - 18;
    tft.fillRect(MENU_X, hintY, MENU_W, 18, COL_TITLE_BG);
    tft.setTextColor(COL_HINT, COL_TITLE_BG);
    tft.setCursor(MENU_X + 8, hintY + 4);
    tft.print("Fn+W/S: select  Fn+A/D: change  ESC: apply & close");
}

// Apply the current settings to the serial port
static void menuApplySettings() {
    int baud = BAUD_RATES[termConfig.baudIndex];
    uint32_t config = SERIAL_8N1;

    // Build config word from data bits, parity, stop bits
    if (termConfig.dataBits == 5) {
        if      (termConfig.parityIndex == 1) config = (termConfig.stopBits == 2) ? SERIAL_5E2 : SERIAL_5E1;
        else if (termConfig.parityIndex == 2) config = (termConfig.stopBits == 2) ? SERIAL_5O2 : SERIAL_5O1;
        else                                  config = (termConfig.stopBits == 2) ? SERIAL_5N2 : SERIAL_5N1;
    } else if (termConfig.dataBits == 6) {
        if      (termConfig.parityIndex == 1) config = (termConfig.stopBits == 2) ? SERIAL_6E2 : SERIAL_6E1;
        else if (termConfig.parityIndex == 2) config = (termConfig.stopBits == 2) ? SERIAL_6O2 : SERIAL_6O1;
        else                                  config = (termConfig.stopBits == 2) ? SERIAL_6N2 : SERIAL_6N1;
    } else if (termConfig.dataBits == 7) {
        if      (termConfig.parityIndex == 1) config = (termConfig.stopBits == 2) ? SERIAL_7E2 : SERIAL_7E1;
        else if (termConfig.parityIndex == 2) config = (termConfig.stopBits == 2) ? SERIAL_7O2 : SERIAL_7O1;
        else                                  config = (termConfig.stopBits == 2) ? SERIAL_7N2 : SERIAL_7N1;
    } else { // 8
        if      (termConfig.parityIndex == 1) config = (termConfig.stopBits == 2) ? SERIAL_8E2 : SERIAL_8E1;
        else if (termConfig.parityIndex == 2) config = (termConfig.stopBits == 2) ? SERIAL_8O2 : SERIAL_8O1;
        else                                  config = (termConfig.stopBits == 2) ? SERIAL_8N2 : SERIAL_8N1;
    }

    Serial.end();
    Serial.begin(baud, config);
}

// ── Public API ────────────────────────────────────────────────────────────────

// Call from the key handler when Fn+ESC is pressed
static void configMenuOpen() {
    menuActive = true;
    menuSelectedRow = 0;
    menuDraw();
}

// Call from the key handler to route keys while menu is active.
// Returns true if the key was consumed by the menu.
static bool configMenuHandleKey(char c, bool fnKey) {
    if (!menuActive) return false;

    if (c == 5 /* ESC */ || (c == 5 && !fnKey)) {
        // ESC: apply and close
        menuApplySettings();
        menuActive = false;
        // Force full terminal redraw
        return true;
    }

    if (!fnKey) return true;  // swallow all keys while menu is open

    bool changed = false;
    switch (c) {
        case 'w': case 'W':  // up
            menuSelectedRow = (menuSelectedRow + MENU_ROWS - 1) % MENU_ROWS;
            changed = true;
            break;
        case 's': case 'S':  // down
            menuSelectedRow = (menuSelectedRow + 1) % MENU_ROWS;
            changed = true;
            break;
        case 'a': case 'A':  // left (decrement)
            switch (menuSelectedRow) {
                case 0: termConfig.baudIndex   = (termConfig.baudIndex + BAUD_COUNT - 1) % BAUD_COUNT; break;
                case 1: termConfig.dataBits    = max(5, termConfig.dataBits - 1); break;
                case 2: termConfig.stopBits    = max(1, termConfig.stopBits - 1); break;
                case 3: termConfig.parityIndex = (termConfig.parityIndex + 2) % 3; break;
                case 4: termConfig.xonXoff     = !termConfig.xonXoff; break;
            }
            changed = true;
            break;
        case 'd': case 'D':  // right (increment)
            switch (menuSelectedRow) {
                case 0: termConfig.baudIndex   = (termConfig.baudIndex + 1) % BAUD_COUNT; break;
                case 1: termConfig.dataBits    = min(8, termConfig.dataBits + 1); break;
                case 2: termConfig.stopBits    = min(2, termConfig.stopBits + 1); break;
                case 3: termConfig.parityIndex = (termConfig.parityIndex + 1) % 3; break;
                case 4: termConfig.xonXoff     = !termConfig.xonXoff; break;
            }
            changed = true;
            break;
    }

    if (changed) {
        menuDraw();
    }
    return true;
}

// Returns true if the menu is currently active (for use in main render loop)
static bool configMenuIsActive() { return menuActive; }

// Returns current baud rate
static int configMenuGetBaud() { return BAUD_RATES[termConfig.baudIndex]; }

#endif // CONFIG_MENU_H
