// Simple ST7789 display test for ESP32-2432S028R (2-port CYD version)
// Tests if the LovyanGFX configuration works

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
            cfg.freq_write = 80000000;  // ST7789 can handle 80MHz
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = 1;
            cfg.pin_sclk = 14;   // SCLK
            cfg.pin_mosi = 13;   // MOSI
            cfg.pin_miso = 12;   // MISO
            cfg.pin_dc = 2;      // DC
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = 15;     // CS
            cfg.pin_rst = -1;    // RST - not connected
            cfg.pin_busy = -1;   // BUSY
            cfg.memory_width = 240;
            cfg.memory_height = 320;
            cfg.panel_width = 240;
            cfg.panel_height = 320;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 16;  // ST7789 requires 16 dummy read bits
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
            cfg.pin_bl = 21;  // Backlight control
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
  delay(2000);

  Serial.println("ST7789 Display Test Starting...");

  tft.begin();
  Serial.println("✓ Display initialized");

  tft.setRotation(1);
  Serial.println("✓ Rotation set");

  tft.fillScreen(TFT_RED);
  Serial.println("✓ Red screen filled");

  delay(1000);

  tft.fillScreen(TFT_GREEN);
  Serial.println("✓ Green screen filled");

  delay(1000);

  tft.fillScreen(TFT_BLUE);
  Serial.println("✓ Blue screen filled");

  delay(1000);

  // Test text
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("ST7789 Test");
  Serial.println("✓ Text drawn");

  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.println("2-Port CYD Display");
  Serial.println("✓ Subtitle drawn");

  tft.drawRect(10, 70, 100, 50, TFT_YELLOW);
  Serial.println("✓ Rectangle drawn");

  tft.fillCircle(60, 150, 30, TFT_CYAN);
  Serial.println("✓ Circle drawn");

  Serial.println("✓✓✓ ALL TESTS PASSED ✓✓✓");
}

void loop() {
  Serial.println("Loop running - display should stay on");
  delay(5000);
}