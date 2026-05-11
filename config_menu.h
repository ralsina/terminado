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
#include <Preferences.h>

// ── Persistent settings ───────────────────────────────────────────────────────

struct TermConfig {
    int  baudIndex;       // index into BAUD_RATES[]
    int  dataBits;        // 5,6,7,8
    int  stopBits;        // 1,2
    int  parityIndex;     // 0=None, 1=Even, 2=Odd
    bool xonXoff;         // software flow control
    int  fontSizeIndex;   // index into FONT_SIZES[]
};

static const int BAUD_RATES[] = {
    300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400
};
static const int BAUD_COUNT = sizeof(BAUD_RATES) / sizeof(BAUD_RATES[0]);
static const char* PARITY_NAMES[] = { "None", "Even", "Odd" };
static const int FONT_SIZES[] = { 0, 8, 12, 5 };  // 0=Tom Thumb, 8=Spleen5x8, 12=Spleen6x12, 5=Font5x7FixedMono
static const int FONT_SIZE_COUNT = 4;

// Default settings
static TermConfig termConfig = {
    .baudIndex     = 5,    // 19200
    .dataBits      = 8,
    .stopBits      = 1,
    .parityIndex   = 0,    // None
    .xonXoff       = true,
    .fontSizeIndex = 1,    // Spleen (excellent terminal font)
};

// ── Menu state ────────────────────────────────────────────────────────────────

enum MenuLevel { MENU_TOP, MENU_CATEGORY };

static bool menuActive = false;
static enum MenuLevel currentMenuLevel = MENU_TOP;
static int  selectedCategory = 0;   // 0=Display, 1=Serial
static int  selectedSetting = 0;    // Setting within current category

// ── Layout constants ─────────────────────────────────────────────────────────
// Navigation: plain W/A/S/D (no Fn needed)

static const int MENU_W       = 200;  // More compact width
static const int MENU_H       = 180;  // More compact height
static const int MENU_X       = (SCREEN_WIDTH  - MENU_W) / 2;
static const int MENU_Y       = (SCREEN_HEIGHT - MENU_H) / 2;
static const int ROW_H        = 20;  // Smaller row height
static const int LABEL_X      = MENU_X + 8;   // Tighter padding
static const int VALUE_X      = MENU_X + 100; // Move value closer to label
static const int FIRST_ROW_Y  = MENU_Y + 35;  // Compact title area

// Category definitions
static const int CATEGORY_COUNT = 2;
static const char* CATEGORY_NAMES[] = {"Display", "Serial"};
static const int CATEGORY_SETTINGS_COUNT[] = {1, 5}; // Display has 1, Serial has 5

static const uint32_t COL_BG       = 0x1A1A2E;
static const uint32_t COL_TITLE_BG = 0x16213E;
static const uint32_t COL_SEL_BG   = 0x0F3460;
static const uint32_t COL_BORDER   = 0x533483;
static const uint32_t COL_TEXT     = TFT_WHITE;
static const uint32_t COL_VALUE    = 0xADD8E6; // light blue
static const uint32_t COL_HINT     = 0x888888;

// ── Helpers ───────────────────────────────────────────────────────────────────

// Draw a main menu category row
static void menuDrawCategoryRow(int row, bool selected) {
    int ry = FIRST_ROW_Y + row * ROW_H;
    uint32_t bg = selected ? COL_SEL_BG : COL_BG;

    tft.fillRect(MENU_X + 1, ry, MENU_W - 2, ROW_H - 1, bg);

    tft.setTextColor(COL_TEXT, bg);
    tft.setCursor(LABEL_X, ry + 6);
    tft.setFont(nullptr);
    tft.setTextSize(1);

    // Category name
    tft.print(CATEGORY_NAMES[row]);

    // Right arrow indicator
    tft.setTextColor(COL_VALUE, bg);
    tft.setCursor(MENU_X + MENU_W - 25, ry + 6);
    tft.print("→");
}

// Draw a setting row within a category
static void menuDrawSettingRow(int row, bool selected, int category) {
    int ry = FIRST_ROW_Y + row * ROW_H;
    uint32_t bg = selected ? COL_SEL_BG : COL_BG;

    tft.fillRect(MENU_X + 1, ry, MENU_W - 2, ROW_H - 1, bg);

    tft.setTextColor(COL_TEXT, bg);
    tft.setCursor(LABEL_X, ry + 6);
    tft.setFont(nullptr);
    tft.setTextSize(1);

    // Map category and row to original setting indices
    int settingRow;
    if (category == 0) { // Display
        settingRow = 0;  // Font
    } else { // Serial
        settingRow = row + 1;  // Baud, Data, Stop, Parity, XON/XOFF
    }

    // Label
    switch (settingRow) {
        case 0: tft.print("Font");        break;
        case 1: tft.print("Baud Rate");   break;
        case 2: tft.print("Data Bits");   break;
        case 3: tft.print("Stop Bits");   break;
        case 4: tft.print("Parity");      break;
        case 5: tft.print("XON/XOFF");    break;
    }

    // Value
    tft.setTextColor(COL_VALUE, bg);
    tft.setCursor(VALUE_X, ry + 6);
    switch (settingRow) {
        case 0:
            if (FONT_SIZES[termConfig.fontSizeIndex] == 0) {
                tft.print("TomThumb");
            } else if (FONT_SIZES[termConfig.fontSizeIndex] == 8) {
                tft.print("Spleen");
            } else if (FONT_SIZES[termConfig.fontSizeIndex] == 12) {
                tft.print("Spleen6x12");
            } else if (FONT_SIZES[termConfig.fontSizeIndex] == 5) {
                tft.print("5x7 mono");
            } else {
                tft.print(FONT_SIZES[termConfig.fontSizeIndex]); tft.print("pt");
            }
            break;
        case 1: tft.print(BAUD_RATES[termConfig.baudIndex]); break;
        case 2: tft.print(termConfig.dataBits);              break;
        case 3: tft.print(termConfig.stopBits);              break;
        case 4: tft.print(PARITY_NAMES[termConfig.parityIndex]); break;
        case 5: tft.print(termConfig.xonXoff ? "On" : "Off"); break;
    }

    // Arrow hints on selected row
    if (selected) {
        tft.setTextColor(COL_HINT, bg);
        tft.setCursor(MENU_X + MENU_W - 25, ry + 6);
        tft.print("< >");
    }
}

// Draw main category menu
static void drawMainMenu() {
    // Outer border
    tft.drawRect(MENU_X - 1, MENU_Y - 1, MENU_W + 2, MENU_H + 2, COL_BORDER);
    tft.fillRect(MENU_X, MENU_Y, MENU_W, MENU_H, COL_BG);

    // Title bar
    tft.fillRect(MENU_X, MENU_Y, MENU_W, 28, COL_TITLE_BG);
    tft.setFont(nullptr);
    tft.setTextSize(1);
    tft.setTextColor(COL_TEXT, COL_TITLE_BG);
    tft.setCursor(MENU_X + 8, MENU_Y + 10);
    tft.print("Settings");

    // Divider
    tft.drawFastHLine(MENU_X, MENU_Y + 28, MENU_W, COL_BORDER);

    // Category rows
    for (int i = 0; i < CATEGORY_COUNT; i++) {
        menuDrawCategoryRow(i, i == selectedCategory);
    }

    // Bottom hint
    int hintY = MENU_Y + MENU_H - 16;
    tft.fillRect(MENU_X, hintY, MENU_W, 16, COL_TITLE_BG);
    tft.setTextColor(COL_HINT, COL_TITLE_BG);
    tft.setCursor(MENU_X + 4, hintY + 4);
    tft.print("W/S:nav ENT:select ESC:exit");
}

// Draw category settings menu
static void drawCategoryMenu(int category) {
    // Outer border
    tft.drawRect(MENU_X - 1, MENU_Y - 1, MENU_W + 2, MENU_H + 2, COL_BORDER);
    tft.fillRect(MENU_X, MENU_Y, MENU_W, MENU_H, COL_BG);

    // Title bar with back arrow
    tft.fillRect(MENU_X, MENU_Y, MENU_W, 28, COL_TITLE_BG);
    tft.setFont(nullptr);
    tft.setTextSize(1);
    tft.setTextColor(COL_TEXT, COL_TITLE_BG);
    tft.setCursor(MENU_X + 8, MENU_Y + 10);
    tft.print("← ");
    tft.print(CATEGORY_NAMES[category]);

    // Divider
    tft.drawFastHLine(MENU_X, MENU_Y + 28, MENU_W, COL_BORDER);

    // Setting rows for this category
    int settingCount = CATEGORY_SETTINGS_COUNT[category];
    for (int i = 0; i < settingCount; i++) {
        menuDrawSettingRow(i, i == selectedSetting, category);
    }

    // Bottom hint
    int hintY = MENU_Y + MENU_H - 16;
    tft.fillRect(MENU_X, hintY, MENU_W, 16, COL_TITLE_BG);
    tft.setTextColor(COL_HINT, COL_TITLE_BG);
    tft.setCursor(MENU_X + 4, hintY + 4);
    tft.print("W/S:nav A/D:chg ESC:back ENT:save");
}

static void menuDraw() {
    if (currentMenuLevel == MENU_TOP) {
        drawMainMenu();
    } else {
        drawCategoryMenu(selectedCategory);
    }
}

// Apply the current settings to the serial port and persist to NVS
static void menuSaveConfig() {
    Preferences prefs;
    prefs.begin("termcfg", false);
    prefs.putInt("baudIdx",   termConfig.baudIndex);
    prefs.putInt("dataBits",  termConfig.dataBits);
    prefs.putInt("stopBits",  termConfig.stopBits);
    prefs.putInt("parityIdx", termConfig.parityIndex);
    prefs.putBool("xonXoff",  termConfig.xonXoff);
    prefs.putInt("fontIdx",   termConfig.fontSizeIndex);
    prefs.end();
}

static void menuLoadConfig() {
    Preferences prefs;
    prefs.begin("termcfg", true); // read-only
    termConfig.baudIndex     = prefs.getInt("baudIdx",   termConfig.baudIndex);
    termConfig.dataBits      = prefs.getInt("dataBits",  termConfig.dataBits);
    termConfig.stopBits      = prefs.getInt("stopBits",  termConfig.stopBits);
    termConfig.parityIndex   = prefs.getInt("parityIdx", termConfig.parityIndex);
    termConfig.xonXoff       = prefs.getBool("xonXoff",  termConfig.xonXoff);
    termConfig.fontSizeIndex = prefs.getInt("fontIdx",   termConfig.fontSizeIndex);
    prefs.end();
}

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
    menuSaveConfig();
}

// ── Public API ────────────────────────────────────────────────────────────────

// Call from the key handler when Fn+ESC is pressed
static void configMenuOpen() {
    menuActive = true;
    currentMenuLevel = MENU_TOP;
    selectedCategory = 0;
    selectedSetting = 0;
    menuDraw();
}

// Call from the key handler to route keys while menu is active.
// Returns true if the key was consumed by the menu.
static bool configMenuHandleKey(char c, bool fnKey) {
    if (!menuActive) return false;

    bool changed = false;

    if (currentMenuLevel == MENU_TOP) {
        // Top level navigation
        if (c == 5 /* ESC */) {
            // ESC: apply and close
            menuApplySettings();
            menuActive = false;
            return true;
        } else if (c == '\n' /* ENTER */) {
            // ENTER: dive into selected category
            currentMenuLevel = MENU_CATEGORY;
            selectedSetting = 0;
            changed = true;
        } else {
            switch (c) {
                case 'w': case 'W':  // up
                    selectedCategory = (selectedCategory + CATEGORY_COUNT - 1) % CATEGORY_COUNT;
                    changed = true;
                    break;
                case 's': case 'S':  // down
                    selectedCategory = (selectedCategory + 1) % CATEGORY_COUNT;
                    changed = true;
                    break;
            }
        }
    } else {
        // Category level navigation
        if (c == 5 /* ESC */) {
            // ESC: return to top level
            currentMenuLevel = MENU_TOP;
            changed = true;
        } else if (c == '\n' /* ENTER */) {
            // ENTER: apply and close
            menuApplySettings();
            menuActive = false;
            return true;
        } else {
            int settingCount = CATEGORY_SETTINGS_COUNT[selectedCategory];
            switch (c) {
                case 'w': case 'W':  // up
                    selectedSetting = (selectedSetting + settingCount - 1) % settingCount;
                    changed = true;
                    break;
                case 's': case 'S':  // down
                    selectedSetting = (selectedSetting + 1) % settingCount;
                    changed = true;
                    break;
                case 'a': case 'A':  // left (decrement)
                    // Map category and setting to original setting index
                    if (selectedCategory == 0) { // Display
                        termConfig.fontSizeIndex = (termConfig.fontSizeIndex + FONT_SIZE_COUNT - 1) % FONT_SIZE_COUNT;
                    } else { // Serial
                        switch (selectedSetting) {
                            case 0: termConfig.baudIndex     = (termConfig.baudIndex + BAUD_COUNT - 1) % BAUD_COUNT; break;
                            case 1: termConfig.dataBits      = max(5, termConfig.dataBits - 1); break;
                            case 2: termConfig.stopBits      = max(1, termConfig.stopBits - 1); break;
                            case 3: termConfig.parityIndex   = (termConfig.parityIndex + 2) % 3; break;
                            case 4: termConfig.xonXoff       = !termConfig.xonXoff; break;
                        }
                    }
                    changed = true;
                    break;
                case 'd': case 'D':  // right (increment)
                    if (selectedCategory == 0) { // Display
                        termConfig.fontSizeIndex = (termConfig.fontSizeIndex + 1) % FONT_SIZE_COUNT;
                    } else { // Serial
                        switch (selectedSetting) {
                            case 0: termConfig.baudIndex     = (termConfig.baudIndex + 1) % BAUD_COUNT; break;
                            case 1: termConfig.dataBits      = min(8, termConfig.dataBits + 1); break;
                            case 2: termConfig.stopBits      = min(2, termConfig.stopBits + 1); break;
                            case 3: termConfig.parityIndex   = (termConfig.parityIndex + 1) % 3; break;
                            case 4: termConfig.xonXoff       = !termConfig.xonXoff; break;
                        }
                    }
                    changed = true;
                    break;
            }
        }
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
