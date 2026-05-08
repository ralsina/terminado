/*
 * VT100 Terminal Emulator for ESP32-S3
 * Simplified implementation focused on essential VT100 escape sequences
 */

#include "vt100.h"
#include <string.h>
#include <stdio.h>
#include <cstddef>
#include <cstdint>
#include <algorithm>

    VT100::VT100() :
        _cursorX(0),
        _cursorY(0),
        _cols(TERM_DEFAULT_COLS),
        _rows(TERM_DEFAULT_ROWS),
        _bufferSize(TERM_DEFAULT_COLS * TERM_DEFAULT_ROWS),
        _savedCursorX(0),
        _savedCursorY(0),
        _savedGraphicsMode(false),
        _savedOriginMode(false),
        _scrollTop(0),
        _scrollBottom(_rows - 1),
        _originMode(false),
        _lineFeedMode(false),
        _autoWrap(true),
        _screenReverse(false),
        _appCursorKeys(false),
        _cursorVisible(true),
        _insertMode(false),
        _tabStops{},
        _state(STATE_GROUND),
        _escapePos(0),
        _needsRedraw(false),
        _flag('\0'),
        _graphicsMode(false),
        _utf8Remaining(0),
        _utf8Codepoint(0),
        _writeCallback(nullptr),
        _titleCallback(nullptr)
    {
        // Initialize screen buffer with spaces
        memset(_screen, ' ', MAX_TERM_BUFFER_SIZE);
        
        // Initialize all attributes
        for (int i = 0; i < _bufferSize; i++) {
            _attrs[i] = VT100Attr();
        }

        // Initialize tab stops to every 8 columns
        initTabStops();
    }

// Maps Unicode box-drawing codepoints (U+2500..U+257F) to VT100 ACS letters.
// Zero means no mapping (skip/space).
static const char boxDrawingACS[128] = {
 // 2500  2501  2502  2503  2504  2505  2506  2507
    'q',  'q',  'x',  'x',  'q',  'q',  'x',  'x',
 // 2508  2509  250A  250B  250C  250D  250E  250F
    'q',  'q',  'x',  'x',  'l',  'l',  'l',  'l',
 // 2510  2511  2512  2513  2514  2515  2516  2517
    'k',  'k',  'k',  'k',  'm',  'm',  'm',  'm',
 // 2518  2519  251A  251B  251C  251D  251E  251F
    'j',  'j',  'j',  'j',  't',  't',  't',  't',
 // 2520  2521  2522  2523  2524  2525  2526  2527
    't',  't',  't',  't',  'u',  'u',  'u',  'u',
 // 2528  2529  252A  252B  252C  252D  252E  252F
    'u',  'u',  'u',  'u',  'w',  'w',  'w',  'w',
 // 2530  2531  2532  2533  2534  2535  2536  2537
    'w',  'w',  'w',  'w',  'v',  'v',  'v',  'v',
 // 2538  2539  253A  253B  253C  253D  253E  253F
    'v',  'v',  'v',  'v',  'n',  'n',  'n',  'n',
 // 2540  2541  2542  2543  2544  2545  2546  2547
    'n',  'n',  'n',  'n',  'n',  'n',  'n',  'n',
 // 2548  2549  254A  254B  254C  254D  254E  254F
    'n',  'n',  'n',  'n',  'q',  'q',  'x',  'x',
 // 2550  2551  2552  2553  2554  2555  2556  2557
    'q',  'x',  'l',  'l',  'l',  'k',  'k',  'k',
 // 2558  2559  255A  255B  255C  255D  255E  255F
    'm',  'm',  'm',  'j',  'j',  'j',  't',  't',
 // 2560  2561  2562  2563  2564  2565  2566  2567
    't',  'u',  'u',  'u',  'w',  'w',  'w',  'v',
 // 2568  2569  256A  256B  256C  256D  256E  256F
    'v',  'v',  'n',  'n',  'n',  'l',  'k',  'j',
 // 2570  2571  2572  2573  2574  2575  2576  2577
    'm',   0,    0,    0,    0,    0,    0,    0,
 // 2578  2579  257A  257B  257C  257D  257E  257F
     0,    0,    0,    0,    0,    0,    0,    0,
};

void VT100::handleCodepoint(uint32_t cp) {
    if (cp < 0x80) {
        handleChar((char)cp);
        return;
    }
    // Map Unicode box-drawing to VT100 ACS
    if (cp >= 0x2500 && cp <= 0x257F) {
        char acsChar = boxDrawingACS[cp - 0x2500];
        if (acsChar) {
            bool savedGraphics = _graphicsMode;
            _graphicsMode = true;
            handleChar(acsChar);
            _graphicsMode = savedGraphics;
            return;
        }
    }
    // Unknown/unmapped codepoint: advance cursor with a space
    handleChar(' ');
}

void VT100::setGeometry(int cols, int rows) {
    _cols = constrain(cols, 1, MAX_TERM_COLS);
    _rows = constrain(rows, 1, MAX_TERM_ROWS);
    _bufferSize = _cols * _rows;
    clearScreen();
    // Notify the host of the new window size so it fires SIGWINCH
    if (_writeCallback) {
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "\033[8;%d;%dt", _rows, _cols);
        _writeCallback(buf, len);
    }
}

void VT100::process(char c) {
    _needsRedraw = true;

    switch (_state) {
        case STATE_GROUND:
            if (c == '\033') {
                _utf8Remaining = 0;
                _state = STATE_ESCAPE;
                _escapePos = 0;
                _escapeBuf[_escapePos++] = c;
            } else if ((uint8_t)c >= 0xF0) {
                // 4-byte UTF-8 lead
                _utf8Codepoint = c & 0x07;
                _utf8Remaining = 3;
            } else if ((uint8_t)c >= 0xE0) {
                // 3-byte UTF-8 lead
                _utf8Codepoint = c & 0x0F;
                _utf8Remaining = 2;
            } else if ((uint8_t)c >= 0xC0) {
                // 2-byte UTF-8 lead
                _utf8Codepoint = c & 0x1F;
                _utf8Remaining = 1;
            } else if ((uint8_t)c >= 0x80 && _utf8Remaining > 0) {
                // UTF-8 continuation byte
                _utf8Codepoint = (_utf8Codepoint << 6) | (c & 0x3F);
                _utf8Remaining--;
                if (_utf8Remaining == 0) {
                    handleCodepoint(_utf8Codepoint);
                }
            } else {
                _utf8Remaining = 0;
                handleChar(c);
            }
            break;

        case STATE_ESCAPE:
            _escapeBuf[_escapePos++] = c;
            if (c == '[') {
                _state = STATE_CSI;
            } else if (c == ']') {
                _state = STATE_OSC;
            } else if (c == '(' || c == ')') {
                _state = STATE_CHARSET;  // ESC ( or ESC ) — wait for charset designator
            } else if (c >= ' ' && c <= '~') {
                // Simple escape sequence
                handleEscape(c);
                _state = STATE_GROUND;
            }
            break;

        case STATE_CHARSET:
            // ESC ( 0  → line-drawing graphics mode
            // ESC ( B  → ASCII normal mode
            if (c == '0') {
                _graphicsMode = true;
            } else {
                _graphicsMode = false;  // B, A, or anything else = ASCII
            }
            _state = STATE_GROUND;
            break;

        case STATE_CSI:
            // C0 controls (except ESC, CAN, SUB) are executed immediately
            // without interrupting the CSI sequence
            if (c == '\030' || c == '\032') {
                // CAN (0x18) or SUB (0x1A): cancel sequence
                _state = STATE_GROUND;
            } else if (c == '\033') {
                // ESC cancels current sequence and starts a new one
                _state = STATE_ESCAPE;
                _escapePos = 0;
                _escapeBuf[_escapePos++] = c;
            } else if (c < 0x20) {
                // Other C0 controls: execute immediately, sequence continues
                handleChar(c);
            } else {
                _escapeBuf[_escapePos++] = c;
                if (_escapePos >= 31) {  // Prevent buffer overflow
                    _state = STATE_GROUND;
                } else if (c >= '@' && c <= '~') {
                    // End of CSI sequence
                    executeCSI(_escapeBuf, _escapePos);
                    _state = STATE_GROUND;
                }
            }
            break;

        case STATE_OSC:
            // Operating System Command - set window title
            // Format: ESC ] 0 ; title BEL or ESC ] 2 ; title BEL
            if (c == '\033' || c == '\007') {
                // OSC sequence terminated
                if (_escapePos > 2 && _escapeBuf[2] == ';') {
                    // Extract title (everything after the semicolon)
                    char titleStart = 3;  // Skip "ESC]2;" or "ESC]0;"
                    // Find the title end (before terminator)
                    for (int i = titleStart; i < _escapePos; i++) {
                        if (_escapeBuf[i] == '\033' || _escapeBuf[i] == '\007') {
                            _escapeBuf[i] = '\0';  // Null terminate
                            break;
                        }
                    }

                    // Set window title (max 64 chars for safety)
                    char title[64];
                    strncpy(title, _escapeBuf + titleStart, sizeof(title) - 1);
                    title[sizeof(title) - 1] = '\0';

                    // Call the title callback if set
                    if (_titleCallback) {
                        _titleCallback(title);
                    }
                }
                _state = STATE_GROUND;
            } else if (_escapePos < 31) {
                _escapeBuf[_escapePos++] = c;
            }
            break;
    }
}

void VT100::handleChar(char c) {
    switch (c) {
        case '\r':  // Carriage Return
            _cursorX = 0;
            if (_lineFeedMode) {
                newline();
            }
            break;

        case '\n':  // Line Feed
            newline();
            break;

        case '\016':  // SO - Shift Out: switch to G1 (graphics) charset
            _graphicsMode = true;
            break;

        case '\017':  // SI - Shift In: switch to G0 (ASCII) charset
            _graphicsMode = false;
            break;

        case '\t':  // Tab - advance to next tab stop
            {
                int next = _cursorX + 1;
                while (next < _cols && !_tabStops[next]) {
                    next++;
                }
                _cursorX = (next < _cols) ? next : _cols - 1;
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
                if (_insertMode) {
                    // Shift line right by one before inserting
                    for (int x = _cols - 1; x > _cursorX; x--) {
                        int src = xyToIndex(x - 1, _cursorY);
                        int dst = xyToIndex(x, _cursorY);
                        _screen[dst] = _screen[src];
                        _attrs[dst] = _attrs[src];
                    }
                }
                setChar(c, _cursorX, _cursorY);
                advanceCursor();
            }
            break;
    }
}

void VT100::initTabStops() {
    for (int i = 0; i < MAX_TERM_COLS; i++) {
        _tabStops[i] = (i % 8 == 0 && i > 0);
    }
}

void VT100::handleEscape(char c) {
    switch (c) {
        case 'M':  // Reverse Index (move up, scroll if needed)
            if (_cursorY > _scrollTop) {
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

        case '7':  // Save Cursor (DECSC)
            _savedCursorX = _cursorX;
            _savedCursorY = _cursorY;
            _savedAttr = _currentAttr;
            _savedGraphicsMode = _graphicsMode;
            _savedOriginMode = _originMode;
            break;

        case '8':  // Restore Cursor (DECRC)
            _cursorX = _savedCursorX;
            _cursorY = _savedCursorY;
            _currentAttr = _savedAttr;
            _graphicsMode = _savedGraphicsMode;
            _originMode = _savedOriginMode;
            break;

        case 'c':  // Reset Device (RIS - full reset)
            _originMode = false;
            _lineFeedMode = false;
            _autoWrap = true;
            _screenReverse = false;
            _appCursorKeys = false;
            _cursorVisible = true;
            _insertMode = false;
            _graphicsMode = false;
            _scrollTop = 0;
            _scrollBottom = _rows - 1;
            _currentAttr = VT100Attr();
            _savedCursorX = 0;
            _savedCursorY = 0;
            _savedAttr = VT100Attr();
            _savedGraphicsMode = false;
            _savedOriginMode = false;
            clearScreen();
            initTabStops();
            break;

        case 'H':  // HTS - Horizontal Tab Set (set tab stop at current column)
            if (_cursorX < MAX_TERM_COLS) {
                _tabStops[_cursorX] = true;
            }
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
    _flag = '\0';

    // Skip "ESC[" prefix
    const char* p = seq + 2;
    const char* end = seq + len;
    char command = end[-1];  // Last character is the command

    // Check for flag character (like '?' or '>')
    if (*p == '?') {
        _flag = '?';
        p++;
    } else if (*p == '>') {
        _flag = '>';
        p++;
    }

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
                int topLimit = _originMode ? _scrollTop : 0;
                _cursorY = max(topLimit, _cursorY - n);
            }
            break;

        case 'B':  // Cursor Down
            {
                int n = (params[0] > 0) ? params[0] : 1;
                int bottomLimit = _originMode ? _scrollBottom : _rows - 1;
                _cursorY = min(bottomLimit, _cursorY + n);
            }
            break;

        case 'C':  // Cursor Forward
            {
                int n = (params[0] > 0) ? params[0] : 1;
                _cursorX = min(_cols - 1, _cursorX + n);
            }
            break;

        case 'D':  // Cursor Back
            {
                int n = (params[0] > 0) ? params[0] : 1;
                _cursorX = max(0, _cursorX - n);
            }
            break;

        case 'E':  // Cursor Next Line
            {
                int n = (params[0] > 0) ? params[0] : 1;
                int bottomLimit = _originMode ? _scrollBottom : _rows - 1;
                _cursorY = min(bottomLimit, _cursorY + n);
                _cursorX = 0;
            }
            break;

        case 'F':  // Cursor Preceding Line
            {
                int n = (params[0] > 0) ? params[0] : 1;
                int topLimit = _originMode ? _scrollTop : 0;
                _cursorY = max(topLimit, _cursorY - n);
                _cursorX = 0;
            }
            break;

        case 'G':  // Cursor Horizontal Absolute
            {
                int col = (params[0] > 0) ? params[0] : 1;
                _cursorX = constrain(col - 1, 0, _cols - 1);
            }
            break;

        case 'd':  // Cursor Vertical Absolute (Line Position Absolute)
            {
                int row = (params[0] > 0) ? params[0] : 1;
                _cursorY = constrain(row - 1, 0, _rows - 1);
            }
            break;

        case 'I':  // CHT - Cursor Forward Tab
            {
                int n = (params[0] > 0) ? params[0] : 1;
                for (int t = 0; t < n; t++) {
                    int next = _cursorX + 1;
                    while (next < _cols && !_tabStops[next]) next++;
                    _cursorX = (next < _cols) ? next : _cols - 1;
                }
            }
            break;

        case 'Z':  // CBT - Cursor Backward Tab
            {
                int n = (params[0] > 0) ? params[0] : 1;
                for (int t = 0; t < n; t++) {
                    int prev = _cursorX - 1;
                    while (prev > 0 && !_tabStops[prev]) prev--;
                    _cursorX = (prev >= 0) ? prev : 0;
                }
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

        case 'r':  // Set Scrolling Region (DECSTBM)
            {
                int top = (params[0] > 0) ? params[0] : 1;
                int bottom = (paramCount > 1 && params[1] > 0) ? params[1] : _rows;
                // Convert to 0-based and clamp
                _scrollTop = constrain(top - 1, 0, _rows - 1);
                _scrollBottom = constrain(bottom - 1, 0, _rows - 1);
                // Ensure valid region (top < bottom)
                if (_scrollTop >= _scrollBottom) {
                    _scrollTop = 0;
                    _scrollBottom = _rows - 1;
                }
                setCursor(0, 0);  // Move cursor to home position
            }
            break;

        case 'J':  // Erase Display
            {
                int mode = (params[0] > 0) ? params[0] : 0;
                if (mode == 0) {
                    // Erase from cursor to end of screen
                    for (int y = _cursorY; y < _rows; y++) {
                        int startX = (y == _cursorY) ? _cursorX : 0;
                        for (int x = startX; x < _cols; x++) {
                            setChar(' ', x, y);
                        }
                    }
                } else if (mode == 1) {
                    // Erase from start of screen to cursor
                    for (int y = 0; y <= _cursorY; y++) {
                        int endX = (y == _cursorY) ? _cursorX + 1 : _cols;
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
                    for (int x = _cursorX; x < _cols; x++) {
                        setChar(' ', x, _cursorY);
                    }
                } else if (mode == 1) {
                    // Erase from start of line to cursor
                    for (int x = 0; x <= _cursorX; x++) {
                        setChar(' ', x, _cursorY);
                    }
                } else if (mode == 2) {
                    // Erase entire line
                    for (int x = 0; x < _cols; x++) {
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
                    } else if (code == 2) {
                        _currentAttr.bold = false;  // faint/dim treated as bold-off
                    } else if (code == 3) {
                        _currentAttr.italic = true;
                    } else if (code == 4) {
                        _currentAttr.underline = true;
                    } else if (code == 5 || code == 6) {
                        _currentAttr.blink = true;
                    } else if (code == 7) {
                        _currentAttr.reverse = true;
                    } else if (code == 22) {
                        _currentAttr.bold = false;
                    } else if (code == 23) {
                        _currentAttr.italic = false;
                    } else if (code == 24) {
                        _currentAttr.underline = false;
                    } else if (code == 25) {
                        _currentAttr.blink = false;
                    } else if (code == 27) {
                        _currentAttr.reverse = false;
                    } else if (code >= 30 && code <= 37) {
                        _currentAttr.fg = (VT100Color)(code - 30);
                    } else if (code >= 40 && code <= 47) {
                        _currentAttr.bg = (VT100Color)(code - 40);
                    }
                }
            }
            break;

        case 'L':  // Insert Lines
            {
                int n = (params[0] > 0) ? params[0] : 1;
                if (_cursorY >= _scrollTop && _cursorY <= _scrollBottom) {
                    int moveLines = _scrollBottom - _cursorY - n + 1;
                    if (moveLines > 0) {
                        memmove(&_screen[(_cursorY + n) * _cols], &_screen[_cursorY * _cols], moveLines * _cols);
                        memmove(&_attrs[(_cursorY + n) * _cols], &_attrs[_cursorY * _cols], moveLines * _cols * sizeof(VT100Attr));
                    }
                    int clearLines = min(n, _scrollBottom - _cursorY + 1);
                    for (int row = _cursorY; row < _cursorY + clearLines; row++) {
                        for (int col = 0; col < _cols; col++) {
                            setChar(' ', col, row);
                        }
                    }
                }
            }
            break;

        case 'M':  // Delete Lines
            {
                int n = (params[0] > 0) ? params[0] : 1;
                if (_cursorY >= _scrollTop && _cursorY <= _scrollBottom) {
                    int moveLines = _scrollBottom - _cursorY - n + 1;
                    if (moveLines > 0) {
                        memmove(&_screen[_cursorY * _cols], &_screen[(_cursorY + n) * _cols], moveLines * _cols);
                        memmove(&_attrs[_cursorY * _cols], &_attrs[(_cursorY + n) * _cols], moveLines * _cols * sizeof(VT100Attr));
                    }
                    int clearStart = max(_cursorY, _scrollBottom - n + 1);
                    for (int row = clearStart; row <= _scrollBottom; row++) {
                        for (int col = 0; col < _cols; col++) {
                            setChar(' ', col, row);
                        }
                    }
                }
            }
            break;

        case '@':  // Insert Characters
            {
                int n = min((params[0] > 0) ? params[0] : 1, _cols - _cursorX);
                for (int x = _cols - 1; x >= _cursorX + n; x--) {
                    int src = xyToIndex(x - n, _cursorY);
                    int dst = xyToIndex(x, _cursorY);
                    _screen[dst] = _screen[src];
                    _attrs[dst] = _attrs[src];
                }
                for (int x = _cursorX; x < _cursorX + n; x++) {
                    setChar(' ', x, _cursorY);
                }
            }
            break;

        case 'P':  // Delete Characters
            {
                int n = min((params[0] > 0) ? params[0] : 1, _cols - _cursorX);
                for (int x = _cursorX; x < _cols - n; x++) {
                    int src = xyToIndex(x + n, _cursorY);
                    int dst = xyToIndex(x, _cursorY);
                    _screen[dst] = _screen[src];
                    _attrs[dst] = _attrs[src];
                }
                for (int x = _cols - n; x < _cols; x++) {
                    setChar(' ', x, _cursorY);
                }
            }
            break;

        case 'X':  // Erase Characters
            {
                int n = (params[0] > 0) ? params[0] : 1;
                for (int i = 0; i < n && _cursorX + i < _cols; i++) {
                    setChar(' ', _cursorX + i, _cursorY);
                }
            }
            break;

        case 'x':  // DECREQTPARM - Request Terminal Parameters
            if (_writeCallback) {
                // params[0]: 0 or 1 (request type); response sol = params[0] + 2
                int sol = ((paramCount > 0 && params[0] == 1) ? 1 : 0) + 2;
                // par=1 (no parity), nbits=1 (8 bits), xspeed=rspeed=128 (9600 baud), clkmul=1, flags=0
                char response[32];
                snprintf(response, sizeof(response), "\033[%d;1;1;128;128;1;0x", sol);
                _writeCallback(response, strlen(response));
            }
            break;

        case 'c':  // Device Attributes
            if (_writeCallback) {
                if (_flag == '>') {
                    // Secondary DA (ESC [ > c) - identify as VT102
                    const char* response = "\033[>6;20;0c";
                    _writeCallback(response, strlen(response));
                } else if (_flag == '?') {
                    // Tertiary DA (ESC [ ? c) - not standard, ignore
                } else {
                    // Primary DA (ESC [ c) - identify as VT102
                    const char* response = "\033[?6c";
                    _writeCallback(response, strlen(response));
                }
            }
            break;
            
        case 'h':  // Set Mode (SM)
        case 'l':  // Reset Mode (RM)
            if (_flag == '?') {
                // DEC Private Mode Set/Reset (ESC [ ? Pn h/l)
                if (paramCount > 0) {
                    for (int i = 0; i < paramCount; i++) {
                        if (params[i] == 1) {
                            // DECCKM - Application Cursor Keys
                            _appCursorKeys = (command == 'h');
                        } else if (params[i] == 4) {
                            // DECSCLM - smooth scroll, ignore
                        } else if (params[i] == 5) {
                            // DECSCNM - Screen Normal/Reverse Mode
                            _screenReverse = (command == 'h');
                        } else if (params[i] == 6) {
                            // DECOM - Origin Mode
                            _originMode = (command == 'h');
                            // Cursor moves to home on mode change (spec requirement)
                            setCursor(0, 0);
                        } else if (params[i] == 7) {
                            // DECAWM - Auto Wrap Mode
                            _autoWrap = (command == 'h');
                        } else if (params[i] == 25) {
                            // DECTCEM - Cursor Visibility
                            _cursorVisible = (command == 'h');
                        }
                    }
                }
            } else {
                // ANSI Mode Set/Reset (ESC [ Pn h/l)
                if (paramCount > 0) {
                    for (int i = 0; i < paramCount; i++) {
                        if (params[i] == 4) {
                            // IRM - Insert/Replace Mode
                            _insertMode = (command == 'h');
                        } else if (params[i] == 20) {
                            // LNM - Line Feed/New Line Mode
                            if (command == 'h') {
                                _lineFeedMode = true;  // Enter sends CR LF
                            } else {
                                _lineFeedMode = false; // Enter sends CR only
                            }
                        }
                    }
                }
            }
            break;

        case 's':  // Save Cursor (ANSI SCP)
            _savedCursorX = _cursorX;
            _savedCursorY = _cursorY;
            _savedAttr = _currentAttr;
            _savedGraphicsMode = _graphicsMode;
            _savedOriginMode = _originMode;
            break;

        case 'u':  // Restore Cursor (ANSI RCP)
            _cursorX = _savedCursorX;
            _cursorY = _savedCursorY;
            _currentAttr = _savedAttr;
            _graphicsMode = _savedGraphicsMode;
            _originMode = _savedOriginMode;
            break;

        case 'g':  // TBC - Tab Clear
            if (params[0] == 0) {
                // Clear tab stop at current column
                if (_cursorX < MAX_TERM_COLS) {
                    _tabStops[_cursorX] = false;
                }
            } else if (params[0] == 3) {
                // Clear all tab stops
                memset(_tabStops, 0, sizeof(_tabStops));
            }
            break;

        case 'n':  // Device Status Report
            if (params[0] == 5) {
                // Respond with "OK": ESC [ 0 n
                if (_writeCallback) {
                    const char* response = "\033[0n";
                    _writeCallback(response, strlen(response));
                }
            } else if (params[0] == 6) {
                // Report cursor position: ESC [ row ; col R
                // Apply origin mode if set
                int reportY = _cursorY;
                int reportX = _cursorX;
                if (_originMode) {
                    // Convert to origin-relative coordinates
                    reportY = _cursorY - _scrollTop;
                    reportX = _cursorX;
                }
                if (_writeCallback) {
                    char response[32];
                    snprintf(response, sizeof(response), "\033[%d;%dR", reportY + 1, reportX + 1);
                    _writeCallback(response, strlen(response));
                }
            }
            break;

        case 't':  // Window manipulation
            if (params[0] == 18) {
                if (_writeCallback) {
                    char response[32];
                    snprintf(response, sizeof(response), "\033[8;%d;%dt", _rows, _cols);
                    _writeCallback(response, strlen(response));
                }
            } else {
                if (_writeCallback) {
                    const char* response = "\033[0t";
                    _writeCallback(response, strlen(response));
                }
            }
            break;

        default:
            // Ignore unimplemented CSI sequences
            break;
    }
}

void VT100::setCursor(int x, int y) {
    applyOriginMode(x, y);
    _cursorX = constrain(x, 0, _cols - 1);
    _cursorY = constrain(y, 0, _rows - 1);
}

void VT100::advanceCursor() {
    _cursorX++;
    if (_cursorX >= _cols) {
        if (_autoWrap) {
            _cursorX = 0;
            newline();
        } else {
            _cursorX = _cols - 1;
        }
    }
}

void VT100::newline() {
    if (_cursorY == _scrollBottom) {
        // At the bottom scroll margin: scroll the region, cursor stays
        scrollUp();
    } else {
        // Anywhere else: just move down, clamped to screen bottom
        _cursorY = min(_cursorY + 1, _rows - 1);
    }
}

void VT100::scrollUp() {
    // Scroll within the scrolling region
    if (_scrollTop < _scrollBottom) {
        // Move lines up within the scrolling region
        int lines = _scrollBottom - _scrollTop;
        memmove(&_screen[_scrollTop * _cols], &_screen[(_scrollTop + 1) * _cols], lines * _cols);
        memmove(&_attrs[_scrollTop * _cols], &_attrs[(_scrollTop + 1) * _cols], lines * _cols * sizeof(VT100Attr));
        
        // Clear the bottom line of the scrolling region
        for (int x = 0; x < _cols; x++) {
            setChar(' ', x, _scrollBottom);
        }
    }
}

void VT100::scrollDown() {
    // Scroll within the scrolling region
    if (_scrollTop < _scrollBottom) {
        // Move lines down within the scrolling region
        int lines = _scrollBottom - _scrollTop;
        memmove(&_screen[(_scrollTop + 1) * _cols], &_screen[_scrollTop * _cols], lines * _cols);
        memmove(&_attrs[(_scrollTop + 1) * _cols], &_attrs[_scrollTop * _cols], lines * _cols * sizeof(VT100Attr));
        
        // Clear the top line of the scrolling region
        for (int x = 0; x < _cols; x++) {
            setChar(' ', x, _scrollTop);
        }
    }
}

void VT100::setChar(char c, int x, int y) {
    int idx = xyToIndex(x, y);
    if (idx >= 0 && idx < _bufferSize) {
        _screen[idx] = c;
        _attrs[idx] = _currentAttr;
        _attrs[idx].graphics = _graphicsMode;
    }
}

void VT100::applyOriginMode(int& x, int& y) const {
    if (_originMode) {
        // Coordinates are relative to scrolling region
        x = constrain(x, 0, _cols - 1);
        y = constrain(y + _scrollTop, _scrollTop, _scrollBottom);
    } else {
        // Coordinates are relative to full screen
        x = constrain(x, 0, _cols - 1);
        y = constrain(y, 0, _rows - 1);
    }
}

char VT100::getChar(int x, int y) const {
    int idx = xyToIndex(x, y);
    if (idx >= 0 && idx < _bufferSize) {
        return _screen[idx];
    }
    return ' ';
}

char VT100::getCharWithOrigin(int x, int y) const {
    applyOriginMode(x, y);
    return getChar(x, y);
}

VT100Attr VT100::getAttrWithOrigin(int x, int y) const {
    applyOriginMode(x, y);
    return getAttr(x, y);
}

VT100Attr VT100::getAttr(int x, int y) const {
    int idx = xyToIndex(x, y);
    if (idx >= 0 && idx < _bufferSize) {
        return _attrs[idx];
    }
    return VT100Attr();
}

void VT100::clearScreen() {
    memset(_screen, ' ', _bufferSize);
    for (int i = 0; i < _bufferSize; i++) {
        _attrs[i] = _currentAttr; // Preserve current attributes including background color
    }
    setCursor(0, 0);
}
