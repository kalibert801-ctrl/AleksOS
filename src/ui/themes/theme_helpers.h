#pragma once
// ════════════════════════════════════════════════════════════════════
//  theme_helpers.h — общие вспомогательные определения для всех тем
//  Включается в начало каждого theme_*.cpp
// ════════════════════════════════════════════════════════════════════
#include "ui/theme_plugin.h"
#include "display/display_manager.h"   // lcd (LGFX)
#include "storage/sd_manager.h"        // sdMgr
#include "system/time_manager.h"       // timeGetString(), timeGetH/M
#include "lang.h"                      // S()
#include "config.h"                    // SCREEN_W/H, HDR_H, ROW_H, BTN_*
#include "ui/font_cyr9.h"              // CyrDejaVu9 — Кириллический шрифт

// ── Шрифты ────────────────────────────────────────────────────────
// Для русского языка используем CyrDejaVu9 (ASCII + Кириллица)
static inline void th_fsm() {
    lcd.setFont(settings.language == LANG_RU ? &CyrDejaVu9 : &lgfx::fonts::DejaVu9);
}
static inline void th_fmd() {
    lcd.setFont(settings.language == LANG_RU ? &CyrDejaVu9 : &lgfx::fonts::DejaVu12);
}
static inline void th_flg() { lcd.setFont(&lgfx::fonts::DejaVu18); }

// ── Константы цветов (тематические) ──────────────────────────────
static constexpr uint16_t TH_GOLD   = 0xFD20u;   // золото — дефолтный акцент
static constexpr uint16_t TH_CYAN   = 0x07FFu;   // время — дефолтный синий
static constexpr uint16_t TH_WHITE  = 0xFFFFu;
static constexpr uint16_t TH_SEP    = 0x2104u;   // тонкий разделитель
static constexpr uint16_t TH_TOPBAR = 0x3186u;   // линия поверх нижней панели

// ── Геометрия нижней панели ───────────────────────────────────────
static constexpr int TH_DPAD_Y    = SCREEN_H - BTNBAR_H;
static constexpr int TH_DPAD_CY   = TH_DPAD_Y + BTNBAR_H / 2;
static constexpr int TH_MBAR_PW   = 107;   // ширина зоны PLAY
static constexpr int TH_MBAR_TX   = 107;   // x начала зоны TIME
static constexpr int TH_MBAR_SX   = 214;   // x начала зоны SETUP

// ── Иконки (маленькие пиксельные рисунки, cx/cy = центр) ─────────

static inline void th_iconSystem(int cx, int cy, uint16_t c) {
    uint16_t bg = getTheme().header;
    lcd.fillCircle(cx,    cy,    5, c);
    lcd.fillCircle(cx,    cy,    2, bg);
    lcd.fillRect(cx - 2, cy - 9, 4, 5, c);
    lcd.fillRect(cx - 2, cy + 4, 4, 5, c);
    lcd.fillRect(cx - 9, cy - 2, 5, 4, c);
    lcd.fillRect(cx + 4, cy - 2, 5, 4, c);
}

static inline void th_iconControls(int cx, int cy, uint16_t c) {
    lcd.fillRect(cx - 2,  cy - 9, 4, 18, c);
    lcd.fillRect(cx - 9,  cy - 2, 18, 4, c);
    lcd.fillCircle(cx + 8, cy - 6, 3, c);
    lcd.fillCircle(cx - 8, cy - 6, 3, c);
}

static inline void th_iconClock(int cx, int cy, uint16_t c) {
    lcd.drawCircle(cx, cy, 7, c);
    lcd.drawFastVLine(cx,     cy - 4, 5, c);
    lcd.drawFastHLine(cx,     cy,     4, c);
    lcd.fillCircle(cx, cy, 1, c);
}

static inline void th_iconGamepad(int cx, int cy, uint16_t c) {
    lcd.drawRect(cx - 7, cy, 14, 10, c);
    lcd.fillRect(cx - 2, cy - 3, 4, 3, c);
    lcd.drawFastHLine(cx - 5, cy + 4, 3, c);
    lcd.drawFastVLine(cx - 4, cy + 3, 3, c);
    lcd.fillCircle(cx + 4, cy + 4, 2, c);
}

// ── Вспомогательные функции для строк ROM ────────────────────────

// Обрезать имя до maxLen символов с ".." суффиксом
static inline String th_trimName(const String& name, int maxLen) {
    if ((int)name.length() <= maxLen) return name;
    return name.substring(0, maxLen - 2) + "..";
}
