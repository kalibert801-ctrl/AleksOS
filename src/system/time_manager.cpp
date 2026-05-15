// ─────────────────────────────────────────────────────────────────────────────
//  time_manager.cpp  —  Модульные часы AleksOS
// ─────────────────────────────────────────────────────────────────────────────
#include "system/time_manager.h"
#include "settings.h"
#include <Arduino.h>

static uint32_t _baseMs  = 0;
static uint32_t _baseSec = 0;   // секунды с начала суток (от полуночи)

void timeInit() {
    _baseMs  = millis();
    _baseSec = (uint32_t)settings.timeH * 3600
             + (uint32_t)settings.timeM * 60;
}

void timeSet(uint8_t h, uint8_t m) {
    h = h % 24;
    m = m % 60;
    settings.timeH = h;
    settings.timeM = m;
    _baseMs  = millis();
    _baseSec = (uint32_t)h * 3600 + (uint32_t)m * 60;
}

static uint32_t _nowSec() {
    return _baseSec + (millis() - _baseMs) / 1000;
}

uint8_t timeGetH() { return (uint8_t)((_nowSec() / 3600) % 24); }
uint8_t timeGetM() { return (uint8_t)((_nowSec() / 60)   % 60); }

String timeGetString() {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", timeGetH(), timeGetM());
    return String(buf);
}

void timeUpdate() {
    // Зарезервировано: здесь можно подключить DS1307/DS3231 в будущем
}
