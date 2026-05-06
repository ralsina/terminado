/**************************Terminado - BBQ20 Keyboard Display for ESP32-S3 HMI************************
Version     :	1.0
Suitable for:	CrowPanel ESP32 HMI Display 5.0 inch
Product link:	https://www.elecrow.com/esp32-display-series-hmi-touch-screen.html
Description	:	Displays keyboard input from BBQ20 keyboard on 800x480 screen
********************************************************************************/


#include <Wire.h>
#include <SPI.h>


/*******************************************************************************
   Config the display panel and touch panel in gfx_conf.h
 ******************************************************************************/
#include "gfx_conf.h"

/* Keyboard support */
#include <BBQ10Keyboard.h>

BBQ10Keyboard keyboard;

void setup()
{
  Serial.begin(9600);

  // Initialize I2C with slower speed for BBQ20 keyboard compatibility
  Wire.begin(19, 20);  // I2C for Elecrow ESP32-S3 HMI: SDA=IO19, SCL=IO20
  Wire.setClock(100000); // Lower I2C speed to 100kHz for BBQ20 keyboard compatibility

  // Initialize keyboard after I2C is set up
  keyboard.begin();
  keyboard.setBacklight(0.5f); // 50% keyboard backlight

  //Display Prepare - exactly like working Draw.ino
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(3);
  delay(100);

  // Test sequence from working Draw.ino
  tft.fillScreen(TFT_BLUE);
  delay(1000);
  tft.fillScreen(TFT_YELLOW);
  delay(1000);
  tft.fillScreen(TFT_GREEN);
  delay(1000);
  tft.fillScreen(TFT_WHITE);
  delay(1000);
  tft.fillScreen(TFT_BLACK);

  // Draw title like working example
  tft.setCursor(200, 240);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("Terminado Started");

  // Test basic text display
  tft.setCursor(50, 100);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.print("Type on BBQ20 Keyboard");

  Serial.println("Terminado started - Type on BBQ20 keyboard!");
  Serial.println("Checking keyboard status...");

  // Test keyboard communication
  int status = keyboard.status();
  Serial.printf("Keyboard status: %d\n", status);

  // Try to read basic info from keyboard
  uint8_t version = keyboard.readRegister8(0x01); // Version register
  Serial.printf("Keyboard firmware version: %d\n", version);

  int keyCount = keyboard.keyCount();
  Serial.printf("Initial key count: %d\n", keyCount);
}

void loop()
{
  // Handle keyboard input
  const int keyCount = keyboard.keyCount();

  // Only print loop info occasionally to reduce spam
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    Serial.printf("Loop - keyCount: %d\n", keyCount);
    lastPrint = millis();
  }

  if (keyCount > 0) {
    const BBQ10Keyboard::KeyEvent key = keyboard.keyEvent();
    String state = "pressed";
    if (key.state == BBQ10Keyboard::StateLongPress)
      state = "held down";
    else if (key.state == BBQ10Keyboard::StateRelease)
      state = "released";

    Serial.printf("key: '%c' (dec %d, hex %02x) %s\r\n", key.key, key.key, key.key, state.c_str());

    // Display keys on screen
    static int yPos = 150;
    static int xPos = 50;

    if (key.state == BBQ10Keyboard::StatePress) {
      tft.setCursor(xPos, yPos);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.print(key.key);

      xPos += 20;  // Move cursor right
      if (xPos > 750) {  // Wrap to next line
        xPos = 50;
        yPos += 30;
        if (yPos > 450) {  // Reset if screen full
          yPos = 150;
          tft.fillScreen(TFT_BLACK);  // Clear screen
        }
      }
    }

    // Keyboard backlight control
    if (key.state == BBQ10Keyboard::StatePress) {
      if (key.key == 'b') {
        keyboard.setBacklight(0);
      } else if (key.key == 'B') {
        keyboard.setBacklight(1.0);
      }
    }
  }

  // Handle serial input
  if (Serial.available()) {
    static int serialYPos = 200;
    static int serialXPos = 50;

    char c = Serial.read();

    // Display serial input in different color
    tft.setCursor(serialXPos, serialYPos);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.print(c);

    serialXPos += 20;  // Move cursor right
    if (serialXPos > 750) {  // Wrap to next line
      serialXPos = 50;
      serialYPos += 30;
      if (serialYPos > 450) {  // Reset if screen full
        serialYPos = 200;
        tft.fillRect(0, 180, 800, 300, TFT_BLACK);  // Clear serial area
      }
    }

    // Echo back to serial
    Serial.write(c);
  }
}