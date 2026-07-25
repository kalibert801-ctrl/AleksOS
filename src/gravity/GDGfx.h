#pragma once
#include "display/display_manager.h"
#include <cmath>
#include <string>

// LovyanGFX wrapper replacing SDL2 Graphics class
class GDGfx {
    uint16_t _color = 0x0000;
    int _sw, _sh;
    static constexpr float DEG = 3.14159265f / 180.0f;

    static int lgfxDatum(int anchor) {
        // J2ME anchors: HCENTER=1 VCENTER=2 LEFT=4 RIGHT=8 TOP=16 BOTTOM=32
        bool hc = anchor & 1, vc = anchor & 2, lft = anchor & 4,
             rgt = anchor & 8, top = anchor & 16, bot = anchor & 32;
        if (lft && top)  return TL_DATUM;
        if (hc  && top)  return TC_DATUM;
        if (rgt && top)  return TR_DATUM;
        if (lft && vc)   return ML_DATUM;
        if (hc  && vc)   return MC_DATUM;
        if (rgt && vc)   return MR_DATUM;
        if (lft && bot)  return BL_DATUM;
        if (hc  && bot)  return BC_DATUM;
        if (rgt && bot)  return BR_DATUM;
        // HCENTER only
        if (hc)          return TC_DATUM;
        if (lft)         return TL_DATUM;
        return TL_DATUM;
    }

public:
    enum Anchors { HCENTER=1,VCENTER=2,LEFT=4,RIGHT=8,TOP=16,BOTTOM=32,BASELINE=64 };

    GDGfx(int w = 320, int h = 240) : _sw(w), _sh(h) {}

    void setColor(int r, int g, int b) {
        // Panel has invert=true, so negate colors to get correct display
        _color = lcd.color565(255 - r, 255 - g, 255 - b);
    }

    void drawLine(int x1, int y1, int x2, int y2) {
        lcd.drawLine(x1, y1, x2, y2, _color);
    }

    void fillRect(int x, int y, int w, int h) {
        if (w > 0 && h > 0) lcd.fillRect(x, y, w, h, _color);
    }

    // drawArc: (x,y) = upper-left corner of bounding rect
    // angles: 0° at 3 o'clock, counter-clockwise
    void drawArc(int x, int y, int w, int h, int startAngle, int arcAngle) {
        if (w <= 0 || h <= 0 || arcAngle == 0) return;
        int r = w / 2;
        if (r == 0) return;
        int cx = x + r, cy = y + r;
        if (arcAngle >= 360) {
            lcd.drawCircle(cx, cy, r, _color);
            return;
        }
        // Convert game angles (0=right, CCW) → LovyanGFX (0=top, CW)
        // LGFX_end   = 90 - startAngle
        // LGFX_start = 90 - (startAngle + arcAngle)
        float a0 = 90.0f - (startAngle + arcAngle);
        float a1 = 90.0f - startAngle;
        if (a0 < 0)   a0 += 360.0f;
        if (a1 < 0)   a1 += 360.0f;
        lcd.drawArc(cx, cy, r, r > 1 ? r - 1 : 0, a0, a1, _color);
    }

    void fillArc(int x, int y, int w, int h, int startAngle, int arcAngle) {
        if (w <= 0 || h <= 0 || arcAngle == 0) return;
        int r = w / 2;
        if (r == 0) return;
        int cx = x + r, cy = y + r;
        float a0 = 90.0f - (startAngle + arcAngle);
        float a1 = 90.0f - startAngle;
        if (a0 < 0) a0 += 360.0f;
        if (a1 < 0) a1 += 360.0f;
        lcd.fillArc(cx, cy, r, 0, a0, a1, _color);
    }

    void fillCircle(int cx, int cy, int r) {
        if (r <= 0) return;
        lcd.fillCircle(cx, cy, r, _color);
    }

    void startFrame() { lcd.startWrite(); }
    void endFrame()   { lcd.endWrite();   }

    void setClip(int x, int y, int w, int h) { /* clip not needed */ }

    void drawString(const std::string& s, int x, int y, int anchor) {
        lcd.setFont(&lgfx::fonts::DejaVu9);
        lcd.setTextDatum(lgfxDatum(anchor));
        lcd.setTextColor(_color);
        lcd.drawString(s.c_str(), x, y);
    }

    void drawString(const char* s, int x, int y, int anchor) {
        lcd.setFont(&lgfx::fonts::DejaVu9);
        lcd.setTextDatum(lgfxDatum(anchor));
        lcd.setTextColor(_color);
        lcd.drawString(s, x, y);
    }

    int getWidth()  { return _sw; }
    int getHeight() { return _sh; }
    uint16_t getColor() { return _color; }
};

// Alias so engine files can use the name Graphics
using Graphics = GDGfx;

// Minimal Image stub (sprites disabled, but references remain in code)
class Image {
public:
    Image() {}
    int getWidth()  { return 0; }
    int getHeight() { return 0; }
};
