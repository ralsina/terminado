/*
 * VT100 Terminal Emulator for ESP32-S3
 * Simplified implementation focused on essential VT100 escape sequences
 */

#ifndef VT100_H
#define VT100_H

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include "term_config.h"

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
    bool italic;
    bool reverse;
    bool blink;
    bool graphics;  // true = VT100 line-drawing charset (ESC ( 0)
    VT100Color fg;
    VT100Color bg;

    VT100Attr() : bold(false), underline(false), italic(false), reverse(false), blink(false),
                  graphics(false), fg(VT100_COLOR_WHITE), bg(VT100_COLOR_BLACK) {}
};

// Terminal state
class VT100 {
public:
    // Callback type for writing responses back to host
    typedef void (*WriteCallback)(const char* data, size_t len);
    // Callback type for window title changes
    typedef void (*TitleCallback)(const char* title);

    VT100();
    VT100(WriteCallback callback);

    // Set write callback for responses
    void setWriteCallback(WriteCallback callback) { _writeCallback = callback; }

    // Set title callback for window title changes
    void setTitleCallback(TitleCallback callback) { _titleCallback = callback; }

    // Process incoming character
    void process(char c);

    // Get screen contents
    char getChar(int x, int y) const;
    VT100Attr getAttr(int x, int y) const;
    
    // Get screen contents with origin mode applied
    char getCharWithOrigin(int x, int y) const;
    VT100Attr getAttrWithOrigin(int x, int y) const;

    // Cursor position
    int cursorX() const { return _cursorX; }
    int cursorY() const { return _cursorY; }

    // Check if screen needs redraw
    bool needsRedraw() const { return _needsRedraw; }
    void clearRedrawFlag() { _needsRedraw = false; }

    // Clear screen
    void clearScreen();

    // Update terminal geometry at runtime (derived from active font metrics).
    void setGeometry(int cols, int rows);

    // Get terminal size
    int cols() const { return _cols; }
    int rows() const { return _rows; }

    // Get line feed mode (LNM): true = Enter sends CR LF, false = CR only
    bool lineFeedMode() const { return _lineFeedMode; }

    // Get screen reverse mode (DECSCNM): true = light background
    bool screenReverse() const { return _screenReverse; }

private:
    // Screen buffer
    char _screen[MAX_TERM_BUFFER_SIZE];
    VT100Attr _attrs[MAX_TERM_BUFFER_SIZE];

    // Active geometry
    int _cols;
    int _rows;
    int _bufferSize;

    // Cursor position
    int _cursorX;
    int _cursorY;

    // Saved cursor position
    int _savedCursorX;
    int _savedCursorY;
    VT100Attr _savedAttr;
    bool _savedGraphicsMode;
    bool _savedOriginMode;
    
    // Scroll region (inclusive, 0-based)
    int _scrollTop;
    int _scrollBottom;
    
    // Origin mode (DECOM): if set, cursor coordinates are relative to scrolling region
    bool _originMode;
    
    // Line Feed/New Line Mode (LNM): if set, Enter sends CR LF, else CR only
    bool _lineFeedMode;

    // Auto Wrap Mode (DECAWM): if set, cursor wraps at end of line
    bool _autoWrap;

    // Screen Reverse Mode (DECSCNM): if set, entire screen is reversed (light background)
    bool _screenReverse;

    // Tab stops (one bool per column)
    bool _tabStops[MAX_TERM_COLS];

    // Escape sequence parsing
    enum State {
        STATE_GROUND,
        STATE_ESCAPE,
        STATE_CSI,
        STATE_OSC,
        STATE_CHARSET
    } _state;

    char _escapeBuf[32];  // Buffer for escape sequences
    int _escapePos;
    char _flag;  // CSI flag character (like '?')

    // Current attributes
    VT100Attr _currentAttr;

    // Current charset: false = ASCII (G0), true = graphics (G1 line drawing)
    bool _graphicsMode;

    // UTF-8 decoder state
    int _utf8Remaining;       // continuation bytes still expected
    uint32_t _utf8Codepoint;  // codepoint being assembled

    // Redraw flag
    bool _needsRedraw;

    // Write callback for sending responses back to host
    WriteCallback _writeCallback;

    // Title callback for window title changes
    TitleCallback _titleCallback;

    // Internal methods
    void handleChar(char c);
    void handleCodepoint(uint32_t cp);
    void handleEscape(char c);
    void handleCSI(char c);
    void executeCSI(const char* seq, int len);

    void setCursor(int x, int y);
    void advanceCursor();
    void newline();
    void scrollUp();
    void scrollDown();

    void setChar(char c, int x, int y);

    // Initialize tab stops to every 8 columns
    void initTabStops();
    
    // Convert coordinates based on origin mode
    void applyOriginMode(int& x, int& y) const;
    int xyToIndex(int x, int y) const { return y * _cols + x; }
};

#endif // VT100_H
