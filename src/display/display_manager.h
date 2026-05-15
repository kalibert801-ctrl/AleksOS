#pragma once
#include <LovyanGFX.hpp>
#include "config.h"

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel;
    lgfx::Bus_SPI      _bus;
    lgfx::Light_PWM    _light;
public:
    LGFX() {
        auto bc = _bus.config();
        bc.spi_host    = VSPI_HOST;
        bc.spi_mode    = 0;
        bc.freq_write  = 80000000;
        bc.dma_channel = 1;
        bc.pin_sclk   = TFT_SCLK;
        bc.pin_mosi   = TFT_MOSI;
        bc.pin_miso   = TFT_MISO;
        bc.pin_dc     = TFT_DC;
        _bus.config(bc);
        _panel.setBus(&_bus);

        auto pc = _panel.config();
        pc.pin_cs        = TFT_CS;
        pc.pin_rst       = TFT_RST;
        pc.memory_width  = 240;
        pc.memory_height = 320;
        pc.panel_width   = 240;
        pc.panel_height  = 320;
        pc.invert        = true;
        _panel.config(pc);

        auto lc = _light.config();
        lc.pin_bl      = TFT_BL;
        lc.invert      = false;
        lc.freq        = 5000;
        lc.pwm_channel = 7;
        _light.config(lc);
        _panel.setLight(&_light);
        setPanel(&_panel);
    }
};

extern LGFX lcd;
void initDisplay();
void setBrightness(uint8_t pct);
void updateAutoBrightness();
