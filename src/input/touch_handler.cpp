#include "input/touch_handler.h"
#include "config.h"
#include "settings.h"
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

static SPIClass tsSPI(VSPI);
static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

TouchHandler touch;
static uint32_t _startTime = 0;

void TouchHandler::init() {
    _startTime = millis();
    // Force full SPI teardown before reinit.
    // initDisplay() (LovyanGFX) reconfigures VSPI hardware registers for the
    // display pins — after that, tsSPI's internal state is stale.
    // end() + begin() forces a fresh hardware init on the touch SPI pins.
    tsSPI.end();
    delay(5);
    tsSPI.begin(TOUCH_CLK, TOUCH_OUT, TOUCH_DIN, TOUCH_CS);
    ts.begin(tsSPI);
    ts.setRotation(1);  // match screen rotation
}

bool TouchHandler::isTouched() {
    if (millis() - _startTime < 500) return false;
    bool now = ts.touched();
    if (now && !_pressed) {
        TS_Point p = ts.getPoint();
        if (p.x < 100 || p.x > 3950 || p.y < 100 || p.y > 3950) return false;
        _px = constrain(map(p.x, 200, 3800, 0, SCREEN_W-1), 0, SCREEN_W-1);
        _py = constrain(map(p.y, 200, 3800, 0, SCREEN_H-1), 0, SCREEN_H-1);
        if (settings.diagTouch)
            Serial.printf("Touch: raw(%d,%d)->(%d,%d)\n", p.x,p.y,_px,_py);
        _pressed = true; _pressTime = millis();
        return true;
    }
    if (!now) _pressed = false;
    return false;
}

void TouchHandler::getXY(int &x, int &y) { x=_px; y=_py; }

// Returns true WHILE finger is held down — used for NES button polling.
// No edge detection, no debounce guard, just current physical state.
bool TouchHandler::rawTouched() {
    if (!ts.touched()) return false;
    TS_Point p = ts.getPoint();
    if (p.x < 100 || p.x > 3950 || p.y < 100 || p.y > 3950) return false;
    _px = constrain(map(p.x, 200, 3800, 0, SCREEN_W-1), 0, SCREEN_W-1);
    _py = constrain(map(p.y, 200, 3800, 0, SCREEN_H-1), 0, SCREEN_H-1);
    return true;
}

bool TouchHandler::zapperTouched() {
    // Direct XPT2046 Z-pressure read via SPI — works regardless of PENIRQ/isrWake.
    // Called from Core 1 (emulator); VSPI (touch) is separate from HSPI (display) — no conflict.
    // Protocol: send cmd byte → read 16 bits (null + 12-bit result + 3 zeros) → shift right 3.
    tsSPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(TOUCH_CS, LOW);
    delayMicroseconds(1);
    tsSPI.transfer(0xB1);                               // Z1 conversion command
    int16_t z1 = (int16_t)(tsSPI.transfer16(0) >> 3);  // read Z1 result (12-bit)
    tsSPI.transfer(0xC1);                               // Z2 conversion command
    int16_t z2 = (int16_t)(tsSPI.transfer16(0) >> 3);  // read Z2 result (12-bit)
    tsSPI.transfer(0x00);                               // power-down XPT2046
    digitalWrite(TOUCH_CS, HIGH);
    tsSPI.endTransaction();
    return (z1 + 4095 - z2) > 400;
}

TapType TouchHandler::checkDoubleTap(int x, int y, int row) {
    (void)x; (void)y;
    uint32_t now = millis();
    if (row == _lastTapRow && (now - _lastTapTime) < 400) {
        _lastTapRow  = -1;
        _lastTapTime = 0;
        return TAP_DOUBLE;
    }
    _lastTapRow  = row;
    _lastTapTime = now;
    return TAP_SINGLE;
}
