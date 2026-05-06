/*
 * VT100 Terminal Emulator for ESP32-S3
 * Simplified implementation focused on essential VT100 escape sequences
 */

#include "vt100.h"
#include <string.h>
#include <stdio.h>

VT100::VT100() :
    _cursorX(0),
    _cursorY(0),
    _savedCursorX(0),
    _savedCursorY(0),
    _state(STATE_GROUND),
    _escapePos(0),
    _needsRedraw(false)
{
    // Initialize screen buffer with spaces
    memset(_screen, ' ', TERM_BUFFER_SIZE);

    // Initialize all attributes
    for (int i = 0; i < TERM_BUFFER_SIZE; i++) {
        _attrs[i] = VT100Attr();
    }
}

void VT100::process(char c) {
    _needsRedraw = true;

    switch (_state) {
        case STATE_GROUND:
            if (c == '\033') {
                _state = STATE_ESCAPE;
                _escapePos = 0;
                _escapeBuf[_escapePos++] = c;
            } else {
                handleChar(c);
            }
            break;

        case STATE_ESCAPE:
            _escapeBuf[_escapePos++] = c;
            if (c == '[') {
                _state = STATE_CSI;
            } else if (c == ']') {
                _state = STATE_OSC;
            } else if (c >= ' ' && c <= '~') {
                // Simple escape sequence
                handleEscape(c);
                _state = STATE_GROUND;
            }
            break;

        case STATE_CSI:
            _escapeBuf[_escapePos++] = c;
            if (_escapePos >= 31) {  // Prevent buffer overflow
                _state = STATE_GROUND;
            } else if (c >= '@' && c <= '~') {
                // End of CSI sequence
                executeCSI(_escapeBuf, _escapePos);
                _state = STATE_GROUND;
            }
            break;

        case STATE_OSC:
            // Operating System Command - mostly ignore for now
            if (c == '\033' || c == '\007') {
                _state = STATE_GROUND;
            }
            break;
    }
}

void VT100::handleChar(char c) {
    switch (c) {
        case '\r':  // Carriage Return
            _cursorX = 0;
            break;

        case '\n':  // Line Feed
            newline();
            break;

        case '\t':  // Tab
            _cursorX = (_cursorX + 8) & ~7;
            if (_cursorX >= TERM_COLS) {
                _cursorX = TERM_COLS - 1;
            }
            break;

        case '\b':  // Backspace
            if (_cursorX > 0) {
                _cursorX--;
            }
            break;

        case '\007':  // Bell - ignore
            break;

        default:
            // Regular character
            if (c >= 32 && c <= 126) {
                setChar(c, _cursorX, _cursorY);
                advanceCursor();
            }
            break;
    }
}

void VT100::handleEscape(char c) {
    switch (c) {
        case 'M':  // Reverse Index (move up, scroll if needed)
            if (_cursorY > 0) {
                _cursorY--;
            } else {
                scrollDown();
            }
            break;

        case 'D':  // Index (move down, scroll if needed)
            newline();
            break;

        case 'E':  // Next Line
            _cursorX = 0;
            newline();
            break;

        case '7':  // Save Cursor
            _savedCursorX = _cursorX;
            _savedCursorY = _cursorY;
            break;

        case '8':  // Restore Cursor
            _cursorX = _savedCursorX;
            _cursorY = _savedCursorY;
            break;

        case 'c':  // Reset Device
            clearScreen();
            break;

        default:
            // Ignore other escape sequences
            break;
    }
}

void VT100::handleCSI(char c) {
    // This is handled in executeCSI
}

void VT100::executeCSI(const char* seq, int len) {
    // Parse CSI sequence: ESC [ ... <command>
    // Extract parameters and command

    int params[10] = {0};
    int paramCount = 0;

    // Skip "ESC[" prefix
    const char* p = seq + 2;
    const char* end = seq + len;
    char command = end[-1];  // Last character is the command

    // Parse parameters
    while (p < end - 1 && paramCount < 10) {
        if (*p >= '0' && *p <= '9') {
            params[paramCount] = atoi(p);
            paramCount++;
            while (p < end && *p >= '0' && *p <= '9') p++;
        } else if (*p == ';') {
            p++;
        } else {
            p++;
        }
    }

    // Execute command
    switch (command) {
        case 'A':  // Cursor Up
            {
                int n = (params[0] > 0) ? params[0] : 1;
                _cursorY = max(0, _cursorY - n);
            }
            break;

        case 'B':  // Cursor Down
            {
                int n = (params[0] > 0) ? params[0] : 1;
                _cursorY = min(TERM_ROWS - 1, _cursorY + n);
            }
            break;

        case 'C':  // Cursor Forward
            {
                int n = (params[0] > 0) ? params[0] : 1;
                _cursorX = min(TERM_COLS - 1, _cursorX + n);
            }
            break;

        case 'D':  // Cursor Back
            {
                int n = (params[0] > 0) ? params[0] : 1;
                _cursorX = max(0, _cursorX - n);
            }
            break;

        case 'H':  // Cursor Position
        case 'f':  // Horizontal Vertical Position
            {
                int row = (params[0] > 0) ? params[0] : 1;
                int col = (paramCount > 1 && params[1] > 0) ? params[1] : 1;
                setCursor(col - 1, row - 1);
            }
            break;

        case 'J':  // Erase Display
            {
                int mode = (params[0] > 0) ? params[0] : 0;
                if (mode == 0) {
                    // Erase from cursor to end of screen
                    for (int y = _cursorY; y < TERM_ROWS; y++) {
                        int startX = (y == _cursorY) ? _cursorX : 0;
                        for (int x = startX; x < TERM_COLS; x++) {
                            setChar(' ', x, y);
                        }
                    }
                } else if (mode == 1) {
                    // Erase from start of screen to cursor
                    for (int y = 0; y <= _cursorY; y++) {
                        int endX = (y == _cursorY) ? _cursorX + 1 : TERM_COLS;
                        for (int x = 0; x < endX; x++) {
                            setChar(' ', x, y);
                        }
                    }
                } else if (mode == 2 || mode == 3) {
                    // Erase entire screen
                    clearScreen();
                }
            }
            break;

        case 'K':  // Erase Line
            {
                int mode = (params[0] > 0) ? params[0] : 0;
                if (mode == 0) {
                    // Erase from cursor to end of line
                    for (int x = _cursorX; x < TERM_COLS; x++) {
                        setChar(' ', x, _cursorY);
                    }
                } else if (mode == 1) {
                    // Erase from start of line to cursor
                    for (int x = 0; x <= _cursorX; x++) {
                        setChar(' ', x, _cursorY);
                    }
                } else if (mode == 2) {
                    // Erase entire line
                    for (int x = 0; x < TERM_COLS; x++) {
                        setChar(' ', x, _cursorY);
                    }
                }
            }
            break;

        case 'm':  // Select Graphic Rendition (colors and attributes)
            if (paramCount == 0) {
                // Reset all attributes
                _currentAttr = VT100Attr();
            } else {
                for (int i = 0; i < paramCount; i++) {
                    int code = params[i];
                    if (code == 0) {
                        _currentAttr = VT100Attr();
                    } else if (code == 1) {
                        _currentAttr.bold = true;
                    } else if (code == 4) {
                        _currentAttr.underline = true;
                    } else if (code == 5 || code == 6) {
                        _currentAttr.blink = true;
                    } else if (code == 7) {
                        _currentAttr.reverse = true;
                    } else if (code >= 30 && code <= 37) {
                        _currentAttr.fg = (VT100Color)(code - 30);
                    } else if (code >= 40 && code <= 47) {
                        _currentAttr.bg = (VT100Color)(code - 40);
                    }
                }
            }
            break;

        case 'L':  // Insert Lines
            scrollDown();
            break;

        case 'M':  // Delete Lines
            scrollUp();
            break;

        case '@':  // Insert Characters
            // Shift rest of line to the right
            for (int x = TERM_COLS - 1; x > _cursorX; x--) {
                setChar(getChar(x - 1, _cursorY), x, _cursorY);
            }
            setChar(' ', _cursorX, _cursorY);
            break;

        case 'P':  // Delete Characters
            // Shift rest of line to the left
            for (int x = _cursorX; x < TERM_COLS - 1; x++) {
                setChar(getChar(x + 1, _cursorY), x, _cursorY);
            }
            setChar(' ', TERM_COLS - 1, _cursorY);
            break;

        case 'X':  // Erase Characters
            {
                int n = (params[0] > 0) ? params[0] : 1;
                for (int i = 0; i < n && _cursorX + i < TERM_COLS; i++) {
                    setChar(' ', _cursorX + i, _cursorY);
                }
            }
            break;

        default:
            // Ignore unimplemented CSI sequences
            break;
    }
}

void VT100::setCursor(int x, int y) {
    _cursorX = constrain(x, 0, TERM_COLS - 1);
    _cursorY = constrain(y, 0, TERM_ROWS - 1);
}

void VT100::advanceCursor() {
    _cursorX++;
    if (_cursorX >= TERM_COLS) {
        _cursorX = 0;
        newline();
    }
}

void VT100::newline() {
    _cursorY++;
    if (_cursorY >= TERM_ROWS) {
        _cursorY = TERM_ROWS - 1;
        scrollUp();
    }
}

void VT100::scrollUp() {
    // Move all lines up by one
    memmove(_screen, _screen + TERM_COLS, (TERM_ROWS - 1) * TERM_COLS);
    memmove(_attrs, _attrs + TERM_COLS, (TERM_ROWS - 1) * TERM_COLS);

    // Clear bottom line
    for (int x = 0; x < TERM_COLS; x++) {
        setChar(' ', x, TERM_ROWS - 1);
    }
}

void VT100::scrollDown() {
    // Move all lines down by one
    memmove(_screen + TERM_COLS, _screen, (TERM_ROWS - 1) * TERM_COLS);
    memmove(_attrs + TERM_COLS, _attrs, (TERM_ROWS - 1) * TERM_COLS);

    // Clear top line
    for (int x = 0; x < TERM_COLS; x++) {
        setChar(' ', x, 0);
    }
}

void VT100::setChar(char c, int x, int y) {
    int idx = xyToIndex(x, y);
    if (idx >= 0 && idx < TERM_BUFFER_SIZE) {
        _screen[idx] = c;
        _attrs[idx] = _currentAttr;
    }
}

char VT100::getChar(int x, int y) const {
    int idx = xyToIndex(x, y);
    if (idx >= 0 && idx < TERM_BUFFER_SIZE) {
        return _screen[idx];
    }
    return ' ';
}

VT100Attr VT100::getAttr(int x, int y) const {
    int idx = xyToIndex(x, y);
    if (idx >= 0 && idx < TERM_BUFFER_SIZE) {
        return _attrs[idx];
    }
    return VT100Attr();
}

void VT100::clearScreen() {
    memset(_screen, ' ', TERM_BUFFER_SIZE);
    for (int i = 0; i < TERM_BUFFER_SIZE; i++) {
        _attrs[i] = VT100Attr();
    }
    setCursor(0, 0);
}
