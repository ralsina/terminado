#ifndef GFX_CONF_H
#define GFX_CONF_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/panel/Panel_ILI9341.hpp>
#include <lgfx/v1/panel/Panel_ST7789.hpp>
#include <lgfx/v1/platforms/esp32/Bus_SPI.hpp>
#include <driver/i2c.h>

/*******************************************************************************
 * Please define the corresponding macros based on the board you have purchased.
 * CrowPanel_43 means CrowPanel 4.3inch Board
 * CrowPanel_50 means CrowPanel 5.0inch Board
 * CrowPanel_70 means CrowPanel 7.0inch Board
 * ESP32_2432S028R means ESP32-2432S028R (320x240 ILI9341 SPI display)
 ******************************************************************************/
// #define CrowPanel_70
// #define CrowPanel_50
// #define CrowPanel_43
#define ESP32_2432S028R


#if defined (CrowPanel_50)

#define screenWidth   800
#define screenHeight  480
class LGFX : public lgfx::LGFX_Device
{
public:
    lgfx::Bus_RGB _bus_instance;
    lgfx::Panel_RGB _panel_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_GT911 _touch_instance;
    LGFX(void)
    {
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width = screenWidth;
            cfg.memory_height = screenHeight;
            cfg.panel_width = screenWidth;
            cfg.panel_height = screenHeight;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }

        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            cfg.pin_d0 = GPIO_NUM_8;  // B0
            cfg.pin_d1 = GPIO_NUM_3;  // B1
            cfg.pin_d2 = GPIO_NUM_46; // B2
            cfg.pin_d3 = GPIO_NUM_9;  // B3
            cfg.pin_d4 = GPIO_NUM_1;  // B4

            cfg.pin_d5 = GPIO_NUM_5;  // G0
            cfg.pin_d6 = GPIO_NUM_6;  // G1
            cfg.pin_d7 = GPIO_NUM_7;  // G2
            cfg.pin_d8 = GPIO_NUM_15; // G3
            cfg.pin_d9 = GPIO_NUM_16; // G4
            cfg.pin_d10 = GPIO_NUM_4; // G5

            cfg.pin_d11 = GPIO_NUM_45; // R0
            cfg.pin_d12 = GPIO_NUM_48; // R1
            cfg.pin_d13 = GPIO_NUM_47; // R2
            cfg.pin_d14 = GPIO_NUM_21; // R3
            cfg.pin_d15 = GPIO_NUM_14; // R4

            cfg.pin_henable = GPIO_NUM_40;
            cfg.pin_vsync = GPIO_NUM_41;
            cfg.pin_hsync = GPIO_NUM_39;
            cfg.pin_pclk = GPIO_NUM_0;
            cfg.freq_write = 12000000;

            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch  = 43;

            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch  = 12;

            cfg.pclk_active_neg = 1;
            cfg.de_idle_high = 0;
            cfg.pclk_idle_high = 0;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = GPIO_NUM_2;
            _light_instance.config(cfg);
            _panel_instance.light(&_light_instance);
        }

        // Touch disabled to prevent I2C conflict with BBQ20 keyboard
        // {
        //     auto cfg = _touch_instance.config();
        //     cfg.x_min      = 0;
        //     cfg.x_max      = 799;
        //     cfg.y_min      = 0;
        //     cfg.y_max      = 479;
        //     cfg.pin_int    = -1;
        //     cfg.pin_rst    = -1;
        //     cfg.bus_shared = true;
        //     cfg.offset_rotation = 0;
        //     cfg.i2c_port   = I2C_NUM_1;
        //     cfg.pin_sda    = GPIO_NUM_19;
        //     cfg.pin_scl    = GPIO_NUM_20;
        //     cfg.freq       = 400000;
        //     cfg.i2c_addr   = 0x14;
        //     _touch_instance.config(cfg);
        //     _panel_instance.setTouch(&_touch_instance);
        // }
        setPanel(&_panel_instance);
    }
};

#elif defined (ESP32_2432S028R)

#define screenWidth   320
#define screenHeight  240

class LGFX : public lgfx::LGFX_Device
{
public:
    lgfx::Bus_SPI _bus_instance;
    lgfx::Panel_ST7789 _panel_instance;  // ST7789 for 2-port CYD version
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

#endif

LGFX tft;

#endif // GFX_CONF_H

