#pragma once
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789  _panel;   // original repo used ST7789
    lgfx::Bus_SPI       _bus;
    lgfx::Light_PWM     _light;

public:
    LGFX(void) {
        {
            auto cfg = _bus.config();
            cfg.spi_host    = HSPI_HOST;
            cfg.spi_mode    = 0;
            cfg.freq_write  = 27000000;
            cfg.freq_read   = 16000000;
            cfg.spi_3wire   = true;
            cfg.use_lock    = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk    = 14;
            cfg.pin_miso    = -1;
            cfg.pin_mosi    = 13;
            cfg.pin_dc      = 2;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs           = 15;
            cfg.pin_rst          = -1;
            cfg.pin_busy         = -1;
            cfg.memory_width     = 240;
            cfg.memory_height    = 320;
            cfg.panel_width      = 240;
            cfg.panel_height     = 320;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 2;  // match TFT_eSPI ROT=2 default
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;
            cfg.invert           = true;   // ST7789 usually needs hardware inversion
            cfg.rgb_order        = true;   // TFT_BGR equivalent
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = false;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl      = 21;
            cfg.invert      = false;
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        setPanel(&_panel);
    }
};

using LGFX_Sprite = lgfx::LGFX_Sprite;
using TFT_eSprite = lgfx::LGFX_Sprite;

// Map TFT_eSPI font numbers to LovyanGFX font pointers
inline const lgfx::IFont* lgfxFont(uint8_t n) {
    switch (n) {
        case 1:  return &lgfx::fonts::Font0;
        case 2:  return &lgfx::fonts::Font2;
        case 4:  return &lgfx::fonts::Font4;
        case 6:  return &lgfx::fonts::Font6;
        case 7:  return &lgfx::fonts::Font7;
        case 8:  return &lgfx::fonts::Font8;
        default: return &lgfx::fonts::Font2;
    }
}
