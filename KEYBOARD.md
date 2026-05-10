# Keyboard Mapping for Terminado

Terminado uses the BBQ10Keyboard with special key mappings optimized for VT100 terminal emulation.

## Hardware Layout

The BBQ10Keyboard has the following special keys:
- **BACK key** → Functions as **Fn** (modifier)
- **SYMBOL key** → Functions as **Ctrl** (modifier)

## Key Mappings

### Modifier Keys

| Key | Function |
|-----|----------|
| BACK | Fn (Function modifier) |
| SYMBOL | Ctrl (Control modifier) |

### Special Function Combinations

| Combination | Action |
|-------------|--------|
| Fn + ESC | Open/close configuration menu |
| Fn + W | Arrow Up |
| Fn + A | Arrow Left |
| Fn + S | Arrow Down |
| Fn + D | Arrow Right |
| Ctrl + Letter | Control characters (Ctrl+C, Ctrl+D, etc.) |

### Standard Keys

- **A-Z** → Letters (hold Shift for uppercase)
- **0-9** → Numbers
- **Space** → Space bar
- **Enter** → Enter/Return key
- **Backspace** → Backspace
- **Tab** → Tab key

### Escape Key

- **ESC key** → ESC character (ASCII 27)
- Used alone: sends escape character to host
- With Fn (Fn+ESC): opens configuration menu

## VT100 Compatibility

The keyboard mappings are designed to work seamlessly with VT100/VT102 escape sequences:

- **Arrow keys** → Send proper escape sequences (`ESC[A` through `ESC[D`)
- **Control keys** → Send control characters (ASCII 0-31)
- **Function combinations** → Mapped to terminal features

## Examples

### Navigation in Vim
```
Fn + W, A, S, D  → Arrow keys for navigation
```

### System Control
```
Ctrl + C → Interrupt signal
Ctrl + D → End of file
Ctrl + Z → Background process
```

### Configuration
```
Fn + ESC → Open settings menu
W/S → Navigate options
A/D → Change values
ESC/ENTER → Save and exit
```

## Technical Details

The keyboard uses I2C communication with the ESP32-S3:
- **SDA**: GPIO 22 (CN1 Blue wire)
- **SCL**: GPIO 27 (CN1 Yellow wire) 
- **Address**: BBQ20Keyboard I2C address

The key mappings are handled in `terminado.ino` through the `handleKeyPress()` function, which translates BBQ10Keyboard scancodes to VT100-appropriate key codes and escape sequences.