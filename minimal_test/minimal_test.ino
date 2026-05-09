// Minimal terminado test - just display and basic functionality
// This will help us isolate the crash

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32/Bus_SPI.hpp>
#include <lgfx/v1/panel/Panel_ST7789.hpp>

class LGFX : public lgfx::LGFX_Device
{
public:
    lgfx::Bus_SPI _bus_instance;
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Light_PWM _light_instance;

    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 80000000;
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = 1;
            cfg.pin_sclk = 14;
            cfg.pin_mosi = 13;
            cfg.pin_miso = 12;
            cfg.pin_dc = 2;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = 15;
            cfg.pin_rst = -1;
            cfg.pin_busy = -1;
            cfg.memory_width = 240;
            cfg.memory_height = 320;
            cfg.panel_width = 240;
            cfg.panel_height = 320;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 16;
            cfg.dummy_read_bits = 1;
            cfg.readable = true;
            cfg.invert = false;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 21;
            cfg.invert = false;
            cfg.freq = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

LGFX tft;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== MINIMAL TERMINADO TEST ===");

  tft.begin();
  Serial.println("✓ Display initialized");

  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  Serial.println("✓ Display cleared");

  // Draw a simple terminal interface
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);

  tft.setCursor(5, 5);
  tft.println("Terminado VT100 Terminal");
  tft.println("ESP32-2432S028R (2-port CYD)");
  tft.println("ST7789 Display Driver");
  tft.println();
  tft.println("Display test: OK");
  tft.println("Next: Add VT100 emulation");
  tft.println();
  tft.println("Ready for serial input...");

  Serial.println("✓ Terminal interface drawn");
  Serial.println("✓✓✓ MINIMAL TEST PASSED ✓✓✓");
}

void loop() {
  // Echo serial input to screen
  if (Serial.available()) {
    char c = Serial.read();
    tft.print(c);
    Serial.print(c);
  }

  // Blink status indicator
  static bool blink = false;
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 1000) {
    blink = !blink;
    tft.fillRect(230, 310, 10, 10, blink ? TFT_GREEN : TFT_BLACK);
    lastBlink = millis();
  }
}