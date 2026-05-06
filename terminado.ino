/*
 * BBQ20 Keyboard Test for ESP32-S3 Elecrow HMI
 *
 * This sketch reads keyboard input from a BBQ20 keyboard connected via I2C
 * and displays the key information on the serial monitor.
 *
 * Hardware connections for Elecrow ESP32-S3 HMI:
 * - BBQ20 SDA -> ESP32-S3 IO19 (GPIO 19)
 * - BBQ20 SCL -> ESP32-S3 IO20 (GPIO 20)
 * - BBQ20 GND -> ESP32-S3 GND
 * - BBQ20 VCC -> ESP32-S3 3.3V
 */

#include <BBQ10Keyboard.h>

BBQ10Keyboard keyboard;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("BBQ20 Keyboard Test Starting...");
    Serial.println("Initializing I2C and keyboard...");

    // Initialize I2C with Elecrow ESP32-S3 HMI pinout
    Wire.begin(19, 20);  // SDA=IO19, SCL=IO20

    // Initialize keyboard with default I2C address (0x1F)
    keyboard.begin();

    // Set keyboard backlight to 50% brightness
    keyboard.setBacklight(0.5f);

    Serial.println("Keyboard initialized!");
    Serial.println("Start typing on the BBQ20 keyboard...");
    Serial.println("----------------------------------------");
}

void loop() {
    // Check if there are any key events waiting
    int keyCount = keyboard.keyCount();

    if (keyCount > 0) {
        // Read the key event
        const BBQ10Keyboard::KeyEvent key = keyboard.keyEvent();

        // Display key information
        Serial.print("Key: '");
        Serial.print(key.key);
        Serial.print("' (");
        Serial.print((uint8_t)key.key);
        Serial.print(") | State: ");

        // Display key state
        switch(key.state) {
            case BBQ10Keyboard::StateIdle:
                Serial.print("Idle");
                break;
            case BBQ10Keyboard::StatePress:
                Serial.print("Press");
                break;
            case BBQ10Keyboard::StateLongPress:
                Serial.print("LongPress");
                break;
            case BBQ10Keyboard::StateRelease:
                Serial.print("Release");
                break;
            default:
                Serial.print("Unknown");
                break;
        }

        Serial.println();

        // Simple buffer to build up a line of text
        static char textBuffer[256] = {0};
        static int bufferPos = 0;

        if (key.state == BBQ10Keyboard::StatePress) {
            if (key.key == '\n') {
                // Enter key - display the complete line
                if (bufferPos > 0) {
                    Serial.print("Text entered: ");
                    Serial.println(textBuffer);
                    bufferPos = 0;
                    textBuffer[0] = '\0';
                }
            } else if (key.key == '\b') {
                // Backspace key
                if (bufferPos > 0) {
                    bufferPos--;
                    textBuffer[bufferPos] = '\0';
                }
            } else if (key.key >= 32 && key.key <= 126) {
                // Printable ASCII character
                if (bufferPos < sizeof(textBuffer) - 1) {
                    textBuffer[bufferPos++] = key.key;
                    textBuffer[bufferPos] = '\0';
                }
            }
        }
    }

    // Small delay to prevent overwhelming the serial output
    delay(10);
}