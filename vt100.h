/*
 * VT100 Terminal Emulator for ESP32-S3
 * Simplified implementation focused on essential VT100 escape sequences
 */

#ifndef VT100_H
#define VT100_H

#include <Arduino.h>

// Screen dimensions (must match terminado.ino)
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

// Font multiplier (must match terminado.ino)
#define FONT_MULTIPLIER 1

// Calculate cell size from font multiplier (Font0 base is 8x8 pixels)
#define TERM_CELL_WIDTH (8 * FONT_MULTIPLIER + 1)  // +1 for spacing
#define TERM_CELL_HEIGHT (8 * FONT_MULTIPLIER + 2) // +2 for line spacing

// Calculate terminal size based on screen and cell size
// Reserve one row at bottom for debug messages
#define TERM_COLS (SCREEN_WIDTH / TERM_CELL_WIDTH)
#define TERM_ROWS ((SCREEN_HEIGHT / TERM_CELL_HEIGHT) - 1)  // One row shorter
#define TERM_BUFFER_SIZE (TERM_COLS * TERM_ROWS)
#define DEBUG_ROW (TERM_ROWS)  // The row below the terminal

// VT100 colors
enum VT100Color {
    VT100_COLOR_BLACK = 0,
    VT100_COLOR_RED = 1,
    VT100_COLOR_GREEN = 2,
    VT100_COLOR_YELLOW = 3,
    VT100_COLOR_BLUE = 4,
    VT100_COLOR_MAGENTA = 5,
    VT100_COLOR_CYAN = 6,
    VT100_COLOR_WHITE = 7
};

// Character attributes
struct VT100Attr {
    bool bold;
    bool underline;
    bool reverse;
    bool blink;
    VT100Color fg;
    VT100Color bg;

    VT100Attr() : bold(false), underline(false), reverse(false), blink(false),
                  fg(VT100_COLOR_WHITE), bg(VT100_COLOR_BLACK) {}
};

// Terminal state
class VT100 {
public:
    // Callback type for writing responses back to host
    typedef void (*WriteCallback)(const char* data, size_t len);

    VT100();
    VT100(WriteCallback callback);

    // Set write callback for responses
    void setWriteCallback(WriteCallback callback) { _writeCallback = callback; }

    // Process incoming character
    void process(char c);

    // Get screen contents
    char getChar(int x, int y) const;
    VT100Attr getAttr(int x, int y) const;

    // Cursor position
    int cursorX() const { return _cursorX; }
    int cursorY() const { return _cursorY; }

    // Check if screen needs redraw
    bool needsRedraw() const { return _needsRedraw; }
    void clearRedrawFlag() { _needsRedraw = false; }

    // Clear screen
    void clearScreen();

    // Get terminal size
    int cols() const { return TERM_COLS; }
    int rows() const { return TERM_ROWS; }

private:
    // Screen buffer
    char _screen[TERM_BUFFER_SIZE];
    VT100Attr _attrs[TERM_BUFFER_SIZE];

    // Cursor position
    int _cursorX;
    int _cursorY;

    // Saved cursor position
    int _savedCursorX;
    int _savedCursorY;

    // Escape sequence parsing
    enum State {
        STATE_GROUND,
        STATE_ESCAPE,
        STATE_CSI,
        STATE_OSC
    } _state;

    char _escapeBuf[32];  // Buffer for escape sequences
    int _escapePos;
    char _flag;  // CSI flag character (like '?')

    // Current attributes
    VT100Attr _currentAttr;

    // Redraw flag
    bool _needsRedraw;

    // Write callback for sending responses back to host
    WriteCallback _writeCallback;

    // Internal methods
    void handleChar(char c);
    void handleEscape(char c);
    void handleCSI(char c);
    void executeCSI(const char* seq, int len);

    void setCursor(int x, int y);
    void advanceCursor();
    void newline();
    void scrollUp();
    void scrollDown();

    void setChar(char c, int x, int y);
    int xyToIndex(int x, int y) const { return y * TERM_COLS + x; }
};

#endif // VT100_H
