// ─────────────────────────────────────────────────────────────────────────────
//  time_manager.cpp  —  Модульные часы AleksOS
// ─────────────────────────────────────────────────────────────────────────────
#include "system/time_manager.h"
#include "settings.h"
#include <Arduino.h>

static uint32_t _baseMs     = 0;
static uint32_t _baseSec    = 0;   // секунды с начала суток (от полуночи)
static uint32_t _dateBaseMs = 0;   // moment когда дата последний раз была установлена

static int _daysInMonth(int mon, int year) {
    static const int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (mon == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) return 29;
    return (mon >= 1 && mon <= 12) ? days[mon] : 30;
}

static void _computeDate(uint8_t &d, uint8_t &mon, uint16_t &year) {
    uint32_t daysElapsed = (millis() - _dateBaseMs) / 86400000UL;
    d    = settings.timeDay;
    mon  = settings.timeMon;
    year = settings.timeYear;
    while (daysElapsed > 0) {
        int dim = _daysInMonth(mon, year);
        uint32_t toEndOfMonth = (uint32_t)(dim - d + 1);
        if (daysElapsed < toEndOfMonth) {
            d += (uint8_t)daysElapsed;
            break;
        }
        daysElapsed -= toEndOfMonth;
        d = 1;
        if (++mon > 12) { mon = 1; year++; }
    }
}

void timeInit() {
    _baseMs     = millis();
    _dateBaseMs = millis();
    _baseSec    = (uint32_t)settings.timeH * 3600
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

void timeSetDate(uint8_t d, uint8_t mon, uint16_t y) {
    if (d < 1)  d = 1;
    if (d > 31) d = 31;
    if (mon < 1)  mon = 1;
    if (mon > 12) mon = 12;
    settings.timeDay  = d;
    settings.timeMon  = mon;
    settings.timeYear = y;
    _dateBaseMs = millis();
}

static uint32_t _nowSec() {
    return _baseSec + (millis() - _baseMs) / 1000;
}

uint8_t timeGetH() { return (uint8_t)((_nowSec() / 3600) % 24); }
uint8_t timeGetM() { return (uint8_t)((_nowSec() / 60)   % 60); }

uint8_t timeGetD() {
    uint8_t d, mon; uint16_t year;
    _computeDate(d, mon, year);
    return d;
}
uint8_t timeGetMon() {
    uint8_t d, mon; uint16_t year;
    _computeDate(d, mon, year);
    return mon;
}
uint16_t timeGetY() {
    uint8_t d, mon; uint16_t year;
    _computeDate(d, mon, year);
    return year;
}

String timeGetString() {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", timeGetH(), timeGetM());
    return String(buf);
}

String timeGetDateString() {
    uint8_t d, mon; uint16_t year;
    _computeDate(d, mon, year);
    char buf[12];
    snprintf(buf, sizeof(buf), "%02d.%02d.%04d", d, mon, year);
    return String(buf);
}

