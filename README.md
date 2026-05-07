# Terminado - VT100 Terminal Emulator for ESP32-S3

A fully functional VT100/VT102 terminal emulator built on ESP32-S3 hardware, providing a classic terminal experience with modern components.

## Overview

Terminado transforms an ESP32-S3 development board with an 800x480 display and BBQ20 keyboard into a authentic VT100 terminal. It can connect to any Linux system via USB serial and provides a complete terminal computing experience.

## Hardware Components

- **Display**: CrowPanel ESP32 HMI Display 5.0 inch (800x480 pixels)
- **Microcontroller**: ESP32-S3 
- **Keyboard**: BBQ20 mechanical keyboard (RP2040-based)
- **Connection**: USB serial to host computer
- **Internal**: I2C between ESP32-S3 and BBQ20 keyboard (SDA=IO19, SCL=IO20)

## Features

### Terminal Emulation
- **VT100/VT102 Compatibility**: Full escape sequence support
- **Character Display**: 88×48 characters (configurable 47×26 or 32×18)
- **ANSI Colors**: 8 standard colors + bright variants
- **Text Attributes**: Bold, underline, reverse video, blink
- **Cursor Control**: Proper positioning, movement, and blinking
- **Screen Management**: Clearing, scrolling, and region operations

### Host Communication
- **Serial Protocol**: 19200 baud, 8N1
- **Device Identification**: Responds to VT100 device attribute queries
- **Size Reporting**: Reports terminal dimensions to applications
- **Bidirectional**: Keyboard input to host, display output from host

### Integration
- **Auto-login**: Passwordless login via systemd getty
- **Auto-reconnect**: Automatically reconnects after USB reset
- **Shell Compatible**: Works with bash, fish, zsh, and other shells
- **Application Support**: nano, vim, htop, tmux, screen, etc.

## Technical Implementation

### Architecture
```
Host Computer ← USB Serial → ESP32-S3 ← I2C → BBQ20 Keyboard
                                        ↓
                                  800x480 Display
```

### Software Components

**VT100 Core** (`vt100.h`, `vt100.cpp`)
- State machine for escape sequence parsing
- Screen buffer management (88×48 × 2 bytes)
- Attribute tracking per character
- Cursor position and scroll region handling
- CSI, ESC, and OSC sequence processing

**Display Layer** (`terminado.ino`)
- LovyanGFX rendering engine
- Efficient incremental updates (only changed cells)
- Character centering and positioning
- Color mapping and attribute rendering
- Blinking cursor implementation

**Input Handling** (`terminado.ino`)
- BBQ10Keyboard library integration
- Keycode to VT100 sequence translation
- Special keys (Enter, Backspace, Tab, Escape)
- Serial output to host

## Configuration

Terminal size and appearance can be adjusted in `terminado.ino`:

```cpp
#define FONT_MULTIPLIER 1  // 1x = 88×48, 2x = 47×26, 3x = 32×18
```

Cell dimensions are calculated automatically:
```cpp
#define TERM_CELL_WIDTH (8 * FONT_MULTIPLIER + 1)   // +1 for spacing
#define TERM_CELL_HEIGHT (8 * FONT_MULTIPLIER + 2)  // +2 for line spacing
```

## Installation

### Arduino Setup

1. Install Arduino ESP32-S3 support
2. Install required libraries:
   - LovyanGFX
   - BBQ10Keyboard
3. Upload `terminado.ino` to ESP32-S3

### Host Configuration

Install the systemd service for automatic login:

```bash
# Create getty override directory
sudo mkdir -p /etc/systemd/system/getty@ttyUSB0.service.d

# Copy service configuration
sudo cp getty-terminado.service /etc/systemd/system/getty@ttyUSB0.service.d/override.conf

# Reload and enable service
sudo systemctl daemon-reload
sudo systemctl enable --now getty@ttyUSB0.service
```

**Note**: The service configuration assumes your username is `ralsina`. Edit `getty-terminado.service` to change the username.

## Usage

1. **Connect** ESP32-S3 to your Linux computer via USB
2. **Power on** the ESP32-S3
3. **Automatic login** - getty will start and log you in
4. **Use terminal** - keyboard input goes to host, display shows output
5. **Applications** - run nano, htop, vim, or any terminal application

### After ESP32 Reset

When the ESP32 restarts (reset button or power cycle):
- Wait 1-2 seconds for the USB to reconnect
- Getty will automatically detect reconnection and restart
- You'll get a fresh login prompt

## Troubleshooting

### No Login After ESP32 Reset

If getty doesn't restart automatically:
```bash
sudo systemctl reset-failed getty@ttyUSB0.service
sudo systemctl restart getty@ttyUSB0.service
```

### Check Service Status
```bash
sudo systemctl status getty@ttyUSB0.service
```

### View Serial Logs
```bash
sudo journalctl -u getty@ttyUSB0.service -f
```

## Development Notes

### Memory Usage
- **Screen Buffer**: ~130KB for 88×48 character + attribute storage
- **Program Space**: 347KB (26% of available)
- **RAM**: 130KB (39% of available)

### Performance
- **Rendering**: Only updates changed cells
- **Parsing**: Efficient state machine for escape sequences
- **Scrolling**: Optimized memory operations with attribute preservation

### Tested Applications
- **Editors**: nano, vim
- **Monitors**: htop, btop, glances
- **Shells**: fish, bash, zsh
- **Tools**: ls --color=auto, grep --color=auto, git diff
- **Multiplexers**: tmux, screen (basic support)

## Limitations

- Text attributes like bold/underline are basic (font limitations)
- No UTF-8 support beyond ASCII
- VT100/VT102 subset only (not full VT220)
- Some advanced TUI applications may have display issues

## Future Enhancements

- UTF-8 and international character support
- Better font support for text attributes
- VT220 extended mode support
- Scrollback buffer implementation
- Configurable color schemes
- Save/restore terminal state

## License

This project builds upon:
- **hl-vt100**: MIT License - VT100 parser reference
- **LovyanGFX**: Licensed under appropriate terms
- **BBQ10Keyboard**: Licensed under appropriate terms

## Credits

Created as a modern hardware VT100 terminal emulator using:
- ESP32-S3 microcontroller
- LovyanGFX graphics library  
- BBQ10Keyboard library
- hl-vt100 VT100 parser reference
- systemd getty for login management
