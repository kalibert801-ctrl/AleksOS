// ui.cpp — RetroESP UI — AleksOS v2 (модульная система тем)
#include "ui/ui.h"
// ui/theme_plugin.h подтянут через ui.h
#include "network/wifi_manager.h"
#include "network/web_manager.h"
#include "network/ntp_manager.h"
#include "network/ota_manager.h"
#include "network/pico_ota.h"
#include <Update.h>

// ── Forward declarations для иконок и меню (используются до определения) ────
static void iconTag(int cx,int cy,uint16_t c);
static void iconChip(int cx,int cy,uint16_t c);
static void iconSave(int cx,int cy,uint16_t c);
static void iconClock2(int cx,int cy,uint16_t c);
// Вспомогательные из showRomInfo-секции (нужны в новом меню)
static void _drawRawFile(const char *path, int dx, int dy, int dw, int dh, bool noArtHint = true, const char *saveThmAs = nullptr);
static void _drawCoverArt(const char *romName, int dx, int dy, int dw, int dh);
static void _uiRomName(const char *path, char *out, size_t maxLen);
static String _fmtPlaytime(uint32_t secs);
// ─────────────────────────────────────────────────────────────────────────
#include "display/display_manager.h"
#include "storage/sd_manager.h"
#include "system/time_manager.h"
#include "system/battery.h"
#include "settings.h"
#include "config.h"
#include "lang.h"
#include "input/button_handler.h"
#include "input/audio.h"
#include <SD.h>
#include <Arduino.h>
#include "storage/game_stats.h"
#include <esp_heap_caps.h>   // heap_caps_malloc — буферы в DRAM (не PSRAM) для SD read
#include <algorithm>         // std::sort (для сортировки списка ROM)
#include <time.h>            // time(), localtime() — дата в подвале меню

// ── Шрифты ───────────────────────────────────────────────────
#include "ui/font_cyr9.h"   // CyrDejaVu9 — ASCII + Кириллика 5×7

#define FLG  (&lgfx::fonts::DejaVu18)
#define FXL  (&lgfx::fonts::DejaVu40)

// fsm/fmd: кириллический шрифт для RU, стандартный для EN/CZ
static void fsm() {
    lcd.setFont(settings.language == LANG_RU ? &CyrDejaVu9 : &lgfx::fonts::DejaVu9);
}
static void fmd() {
    lcd.setFont(settings.language == LANG_RU ? &CyrDejaVu9 : &lgfx::fonts::DejaVu12);
}
static void flg() { lcd.setFont(FLG); }
static void fxl() { lcd.setFont(FXL); }

// ── Геометрия ─────────────────────────────────────────────────
#define DPAD_W     80
#define DPAD_Y     (SCREEN_H - BTNBAR_H)
#define DPAD_CY    (DPAD_Y + BTNBAR_H/2)

// Зоны нижней панели меню
#define MBAR_PLAY_W  107   // x 0..106
#define MBAR_TIME_X  107   // x 107..213
#define MBAR_SET_X   214   // x 214..319

// Зоны старой шапки (для настроек — остаются)
#define HDR_PLAY_X   175
#define HDR_SETUP_X  255
#define HDR_BACK_W   85

// Цвета меню (поверх темы)
#define COL_GOLD   0xFD20   // золото для заголовка и стрелок
#define COL_CYAN   0x07FF   // время
#define COL_WHITE  0xFFFF
#define COL_SEP    0x2104   // тонкий разделитель строк
#define COL_TOPBAR 0x3186   // линия поверх нижней панели

// ── D-pad helper ───────────────────────────────────────────────
int getDpadBtn(int x, int y) {
    if (y < DPAD_Y || y >= SCREEN_H) return -1;
    return x / DPAD_W;
}

// ══════════════════════════════════════════════════════════════
// ИКОНКИ (20×20, cx/cy = центр)
// ══════════════════════════════════════════════════════════════

static void iconDisplay(int cx, int cy, uint16_t c) {
    uint16_t bg = getTheme().header;  // фон плитки/шапки — для "экрана" внутри иконки
    lcd.drawRect(cx-9, cy-6, 18, 12, c);
    lcd.fillRect(cx-7, cy-4, 14, 8, c);
    lcd.fillRect(cx-5, cy-2, 10, 4, bg);
    lcd.fillRect(cx-1, cy+6, 3, 3, c);
    lcd.fillRect(cx-5, cy+9, 11, 2, c);
}
static void iconAudio(int cx, int cy, uint16_t c) {
    lcd.fillCircle(cx-4, cy+5, 3, c);
    lcd.fillRect(cx-1, cy-7, 2, 13, c);
    lcd.fillCircle(cx+5, cy+3, 3, c);
    lcd.fillRect(cx+7, cy-7, 2, 11, c);
    lcd.fillRect(cx-1, cy-7, 9, 2, c);
}
static void iconLook(int cx, int cy, uint16_t c) {
    lcd.drawLine(cx, cy-9, cx+9, cy, c);
    lcd.drawLine(cx+9, cy, cx, cy+9, c);
    lcd.drawLine(cx, cy+9, cx-9, cy, c);
    lcd.drawLine(cx-9, cy, cx, cy-9, c);
    lcd.fillCircle(cx, cy, 3, c);
}
static void iconSystem(int cx, int cy, uint16_t c) {
    uint16_t bg = getTheme().header;  // "дырка" шестерёнки = цвет фона
    lcd.fillCircle(cx, cy, 5, c);
    lcd.fillCircle(cx, cy, 2, bg);
    lcd.fillRect(cx-2, cy-9, 4, 5, c);
    lcd.fillRect(cx-2, cy+4, 4, 5, c);
    lcd.fillRect(cx-9, cy-2, 5, 4, c);
    lcd.fillRect(cx+4, cy-2, 5, 4, c);
}
static void iconControls(int cx, int cy, uint16_t c) {
    lcd.fillRect(cx-2, cy-9, 4, 18, c);
    lcd.fillRect(cx-9, cy-2, 18, 4, c);
    lcd.fillCircle(cx+8, cy-6, 3, c);
    lcd.fillCircle(cx-8, cy-6, 3, c);
}
static void iconInfo(int cx, int cy, uint16_t c) {
    lcd.drawCircle(cx, cy, 9, c);
    lcd.fillCircle(cx, cy-4, 2, c);
    lcd.fillRect(cx-1, cy-1, 3, 8, c);
}
static void iconClock(int cx, int cy, uint16_t c) {
    lcd.drawCircle(cx, cy, 9, c);
    lcd.drawFastVLine(cx, cy-6, 7, c);
    lcd.drawFastHLine(cx, cy, 5, c);
    lcd.fillCircle(cx, cy, 2, c);
}

// ══════════════════════════════════════════════════════════════
// НИЖНЯЯ ПАНЕЛЬ НАСТРОЕК (оригинальная, для settings/remap)
// ══════════════════════════════════════════════════════════════

static void drawArrow(int cx, int cy, int dir, uint16_t c) {
    int s = 7;
    switch (dir) {
        case 0: lcd.fillTriangle(cx, cy-s, cx-s, cy+s, cx+s, cy+s, c); break;
        case 1: lcd.fillTriangle(cx, cy+s, cx-s, cy-s, cx+s, cy-s, c); break;
        case 2: lcd.fillTriangle(cx-s, cy, cx+s, cy-s, cx+s, cy+s, c); break;
        case 3: lcd.fillTriangle(cx+s, cy, cx-s, cy-s, cx-s, cy+s, c); break;
    }
}

static void drawNavBar(int zones[4], uint16_t fg) {
    const Theme565 &t = getTheme();
    lcd.fillRect(0, DPAD_Y, SCREEN_W, BTNBAR_H, t.header);
    lcd.drawFastHLine(0, DPAD_Y, SCREEN_W, 0x2945);
    int cx[4] = { 40, 120, 200, 280 };
    for (int i = 0; i < 4; i++) {
        if (i > 0) lcd.drawFastVLine(DPAD_W*i, DPAD_Y, BTNBAR_H, 0x2945);
        if (zones[i] >= 0) drawArrow(cx[i], DPAD_CY, zones[i], fg);
    }
}

// ══════════════════════════════════════════════════════════════
// ШАПКИ (для настроек — без изменений)
// ══════════════════════════════════════════════════════════════

static void drawHeaderBack(const char *title, bool showSave = false) {
    const Theme565 &t = getTheme();
    bool flat = (t.style & THEME_STYLE_FLAT);

    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);

    // Нижняя граница шапки
    if (t.style & THEME_STYLE_BEVEL) {
        lcd.drawFastHLine(0, HDR_H-2, SCREEN_W, t.hilite);
        lcd.drawFastHLine(0, HDR_H-1, SCREEN_W, t.shadow);
    } else {
        lcd.drawFastHLine(0, HDR_H-1, SCREEN_W, t.accent);
    }

    // Кнопка "назад"
    if (flat && (t.style & THEME_STYLE_BEVEL)) {
        // Win98: объёмная кнопка на сером фоне поверх синей шапки — рисуем серый прямоугольник
        int bx=4, by_=7, bw=HDR_BACK_W-8, bh=HDR_H-14;
        lcd.fillRect(bx, by_, bw, bh, t.bg);
        lcd.drawFastHLine(bx,    by_,      bw, t.hilite);
        lcd.drawFastVLine(bx,    by_,      bh, t.hilite);
        lcd.drawFastHLine(bx,    by_+bh-1, bw, t.shadow);
        lcd.drawFastVLine(bx+bw-1, by_,    bh, t.shadow);
        fsm(); lcd.setTextDatum(MC_DATUM);
        lcd.setTextColor(t.textPri);  // чёрный текст на сером
        lcd.drawString(cyrStr(S().back), HDR_BACK_W/2, HDR_H/2);
    } else {
        // Тонкая рамка (прямая для flat, со стандартным цветом для rounded)
        uint16_t borderCol = flat ? t.accent : (uint16_t)0x2945;
        lcd.drawRect(4, 7, HDR_BACK_W-8, HDR_H-14, borderCol);
        fsm(); lcd.setTextDatum(MC_DATUM);
        lcd.setTextColor(t.accent);
        lcd.drawString(cyrStr(S().back), HDR_BACK_W/2, HDR_H/2);
    }

    // Заголовок экрана
    uint16_t titleTxt;
    if      (t.style & THEME_STYLE_BEVEL) titleTxt = t.selText ? t.selText : 0xFFFF;
    else if (flat)                         titleTxt = t.textPri;
    else                                   titleTxt = t.textPri;
    fmd(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(titleTxt);
    lcd.drawString(title, (HDR_BACK_W + SCREEN_W)/2, HDR_H/2);
}

// ══════════════════════════════════════════════════════════════
// ВСПОМОГАТЕЛЬНЫЕ (настройки)
// ══════════════════════════════════════════════════════════════

static void drawScrollbar(int total, int visible, int offset) {
    if (total <= visible) return;
    const Theme565 &t = getTheme();
    int trackH = DPAD_Y - HDR_H;
    int thumbH = max(12, trackH * visible / total);
    int thumbY = HDR_H + (offset * (trackH - thumbH)) / max(1, total - visible);
    lcd.fillRect(SCREEN_W-4, HDR_H, 4, trackH, t.rowOdd);
    lcd.fillRect(SCREEN_W-4, thumbY, 4, thumbH, t.accent);
}

static void drawListRow(int y, const char *left, const char *right,
                        bool sel, bool alt, uint16_t rowH = ROW_H) {
    const Theme565 &t = getTheme();
    bool flat = (t.style & THEME_STYLE_FLAT);
    uint16_t bg = sel ? t.selected : (alt ? t.rowOdd : t.rowEven);
    lcd.fillRect(0, y, SCREEN_W-4, rowH, bg);
    if (sel) {
        if (!flat) {
            lcd.fillRect(0, y, 3, rowH, t.accent);  // левый акцент-бар (rounded)
        }
        if (t.style & THEME_STYLE_BEVEL) {
            lcd.drawFastHLine(0, y,        SCREEN_W-4, t.hilite);
            lcd.drawFastHLine(0, y+rowH-1, SCREEN_W-4, t.shadow);
        }
    }
    // Цвет текста на выбранной строке
    uint16_t stc = t.selText ? t.selText : (uint16_t)COL_WHITE;
    fsm();
    if (left) {
        lcd.setTextColor(sel ? stc : t.textPri);
        lcd.setTextDatum(ML_DATUM);
        lcd.drawString(left, 10, y + rowH/2);
    }
    if (right) {
        lcd.setTextColor(sel ? stc : t.accent);
        lcd.setTextDatum(MR_DATUM);
        lcd.drawString(right, SCREEN_W-9, y + rowH/2);
    }
}

// ══════════════════════════════════════════════════════════════
// BOOT / ERROR (сохраняем для совместимости)
// ══════════════════════════════════════════════════════════════

void drawBoot() {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);
    lcd.fillCircle(SCREEN_W/2, 88, 52, t.accent);
    lcd.fillCircle(SCREEN_W/2, 88, 44, t.header);
    flg(); lcd.setTextColor(t.accent); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("NES", SCREEN_W/2, 88);
    fmd(); lcd.setTextColor(t.textPri);
    lcd.drawString("RetroESP Console", SCREEN_W/2, 158);
    fsm(); lcd.setTextColor(t.textSec);
    lcd.drawString(FIRMWARE_VERSION, SCREEN_W/2, 176);
    lcd.setTextColor(t.accent);
    lcd.drawString(cyrStr(S().loading), SCREEN_W/2, 198);
}

void drawSDError() {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);
    lcd.fillCircle(SCREEN_W/2, 80, 44, t.danger);
    fxl(); lcd.setTextColor(t.bg); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("!", SCREEN_W/2, 80);
    fmd(); lcd.setTextColor(t.textPri);
    lcd.drawString(cyrStr(S().noSD), SCREEN_W/2, 148);
    fsm(); lcd.setTextColor(t.textSec);
    lcd.drawString(cyrStr(S().noSDHint), SCREEN_W/2, 168);
}

// ══════════════════════════════════════════════════════════════
// ══ ГЛАВНОЕ МЕНЮ v2 — компактный layout с правой панелью ═════
// ══  v14.72: шапка 30px | список 7×24px | панель 124px | подвал 40px
// ══════════════════════════════════════════════════════════════

// ── Состояние ──────────────────────────────────────────────────
static int  _menuSel    = 0;
static int  _menuOffset = 0;

static bool _favMode   = false;
static int  _favIdx[256];
static int  _favCount  = 0;

static int  _sortMode  = 0;   // 0=original, 1=A→Z, 2=Z→A, 3=size
static int  _sortIdx[256];
static int  _sortCount = 0;

static bool _searchActive = false;
static char _searchQuery[48] = "";
static int  _searchList[256];
static int  _searchCount  = 0;

static int  _coverLastRom = -2;

// Drag scroll
static bool _dragActive  = false;
static int  _dragStartY  = 0;
static int  _dragOff0    = 0;

// ── Публичные аксессоры ─────────────────────────────────────────
bool menuIsFavMode()  { return _favMode; }
int  menuSortMode()   { return _sortMode; }

// ── Списковые хелперы ──────────────────────────────────────────
static int _menuListCount() {
    if (_searchActive) return _searchCount;
    if (_favMode)      return _favCount;
    return (_sortMode > 0) ? _sortCount : sdMgr.count();
}
static int _menuActual(int listIdx) {
    if (listIdx < 0) return -1;
    if (_searchActive) return (listIdx < _searchCount) ? _searchList[listIdx] : -1;
    if (_favMode)      return (listIdx < _favCount)    ? _favIdx[listIdx]     : -1;
    if (_sortMode > 0) return (listIdx < _sortCount)   ? _sortIdx[listIdx]    : -1;
    return (listIdx < sdMgr.count()) ? listIdx : -1;
}
static void _menuBuildFavList() {
    _favCount = 0;
    for (int i = 0; i < sdMgr.count() && _favCount < 256; i++)
        if (GameStats::favCheck(sdMgr.get(i).path.c_str()))
            _favIdx[_favCount++] = i;
}
static void _menuBuildSortedList() {
    int n = sdMgr.count();
    if (n > 256) n = 256;
    _sortCount = n;
    for (int i = 0; i < n; i++) _sortIdx[i] = i;
    if (_sortMode == 0) return;
    if (_sortMode == 4) {
        // Избранные вверху, потом остальные в исходном порядке
        int head = 0, tail = 0;
        static int tmp[256];
        for (int i = 0; i < n; i++) {
            if (GameStats::favCheck(sdMgr.get(i).path.c_str()))
                _sortIdx[head++] = i;
            else
                tmp[tail++] = i;
        }
        for (int i = 0; i < tail; i++) _sortIdx[head + i] = tmp[i];
        return;
    }
    std::sort(_sortIdx, _sortIdx + n, [](int a, int b) -> bool {
        if (_sortMode == 2) return sdMgr.get(a).name > sdMgr.get(b).name;
        if (_sortMode == 3) return sdMgr.get(a).size > sdMgr.get(b).size;
        return sdMgr.get(a).name < sdMgr.get(b).name;  // 1=A→Z
    });
}
static void _menuBuildSearchList() {
    _searchCount = 0;
    String q = String(_searchQuery); q.toLowerCase();
    for (int i = 0; i < sdMgr.count() && _searchCount < 256; i++) {
        String nm = sdMgr.get(i).name; nm.toLowerCase();
        if (nm.indexOf(q) >= 0) _searchList[_searchCount++] = i;
    }
}

// ── Иконки нового меню (маленькие, используются в шапке/подвале) ─
// Звезда (6-конечная из двух треугольников)
static void _mStar(int cx, int cy, uint16_t c) {
    lcd.fillTriangle(cx, cy-5, cx-3, cy+2, cx+3, cy+2, c);
    lcd.fillTriangle(cx, cy+4, cx-3, cy-2, cx+3, cy-2, c);
}
// Лупа поиска
static void _mSearch(int cx, int cy, uint16_t c) {
    lcd.drawCircle(cx-1, cy-1, 5, c);
    lcd.drawLine(cx+3, cy+3, cx+7, cy+7, c);
    lcd.drawLine(cx+4, cy+3, cx+8, cy+7, c);
}
// WiFi (цвет передаётся явно: зелёный/красный/жёлтый)
static void _mWifi(int cx, int cy, uint16_t c) {
    lcd.drawLine(cx-7, cy+2, cx, cy-5, c);
    lcd.drawLine(cx+7, cy+2, cx, cy-5, c);
    lcd.drawLine(cx-4, cy+2, cx, cy-2, c);
    lcd.drawLine(cx+4, cy+2, cx, cy-2, c);
    lcd.fillCircle(cx, cy+4, 2, c);
}
// Фаза анимации зарядки (0-3), обновляется в menuBatteryAnimTick()
static uint8_t _batAnimPhase = 0;

// Батарея: динамическая иконка.
// При зарядке — синяя анимация "заполнения" от текущего уровня к 100%.
// В норме — зелёный/оранжевый/красный по уровню заряда.
// Корпус 16×8 px, нуб 2×4 px, внутри 13×6 px (0..12 px заливки).
// Батарея: корпус 20×10 px, нуб 3×6, внутренний заряд 17×8
static void _mBattery(int cx, int cy) {
    int pct = batteryPercent();
    const Theme565& t = getTheme();

    if (batteryCharging()) {
        constexpr uint16_t COL_CHARGE = 0x07FFu;  // cyan
        lcd.drawRect(cx - 10, cy - 5, 20, 10, COL_CHARGE);
        lcd.fillRect(cx + 10, cy - 3,  3,  6, COL_CHARGE);
        lcd.fillRect(cx -  9, cy - 4, 17,  8, t.header);
        int base    = (pct >= 0) ? pct : 0;
        int extra   = (100 - base) * (int)_batAnimPhase / 4;
        int animPct = base + extra;
        int fillW   = (animPct * 16 + 50) / 100;
        if (fillW > 16) fillW = 16;
        if (fillW > 0)  lcd.fillRect(cx - 9, cy - 4, fillW, 8, COL_CHARGE);
        return;
    }

    uint16_t fc;
    if      (pct < 0)   fc = 0xAD55u;
    else if (pct <= 20) fc = 0xF800u;
    else if (pct <= 50) fc = 0xFFE0u;
    else                fc = 0x07E0u;

    lcd.drawRect(cx - 10, cy - 5, 20, 10, fc);
    lcd.fillRect(cx + 10, cy - 3,  3,  6, fc);
    lcd.fillRect(cx -  9, cy - 4, 17,  8, t.header);

    if (pct < 0) {
        lcd.fillRect(cx - 5, cy - 2, 9, 1, 0xAD55u);
        lcd.fillRect(cx - 5, cy + 1, 9, 1, 0xAD55u);
        return;
    }

    int fillW = (pct * 16 + 50) / 100;
    if (fillW > 16) fillW = 16;
    if (fillW > 0)  lcd.fillRect(cx - 9, cy - 4, fillW, 8, fc);
}

// ── Шапка нового меню ──────────────────────────────────────────
static void _menuDrawHeader() {
    const Theme565& t = getTheme();
    lcd.fillRect(0, 0, SCREEN_W, M_HDR_H, t.header);
    lcd.drawFastHLine(0, M_HDR_H - 1, SCREEN_W, t.accent);

    // ★ Избранные (x=0..29)
    _mStar(14, M_HDR_H / 2, _favMode ? (uint16_t)COL_GOLD : t.textSec);

    // 🔍 Поиск (x=30..58)
    _mSearch(44, M_HDR_H / 2, _searchActive ? t.accent : t.textSec);

    // Заголовок по центру
    lcd.setTextDatum(MC_DATUM); lcd.setTextColor(COL_GOLD);
    if (settings.language == LANG_RU) {
        fmd();
        const char *s = cyrStr(S().listTitle);
        lcd.drawString(s, SCREEN_W/2,   M_HDR_H/2);
        lcd.drawString(s, SCREEN_W/2+1, M_HDR_H/2);
    } else {
        flg(); lcd.drawString(S().listTitle, SCREEN_W/2, M_HDR_H/2);
    }

    // Счётчик ROM (слева от ⋮)
    char badge[8]; snprintf(badge, sizeof(badge), "%d", sdMgr.count());
    fsm(); lcd.setTextDatum(MR_DATUM); lcd.setTextColor(t.textSec);
    lcd.drawString(badge, SCREEN_W - 22, M_HDR_H / 2);

    // ⋮ три точки (x=290..319)
    int dx = 306;
    for (int i = -1; i <= 1; i++)
        lcd.fillCircle(dx, M_HDR_H / 2 + i * 5, 2, t.textSec);
}

// ── Подвал нового меню ─────────────────────────────────────────
static void _menuDrawFooter() {
    const Theme565& t = getTheme();
    int by = M_DPAD_Y, cy = M_DPAD_CY;

    lcd.fillRect(0, by, SCREEN_W, M_BAR_H, t.header);
    lcd.drawFastHLine(0, by, SCREEN_W, (uint16_t)COL_TOPBAR);

    // ▶ PLAY кнопка (x=2..93)
    lcd.fillRoundRect(2, by + 4, 91, 32, 8, t.selected);
    lcd.fillTriangle(13, cy - 7, 13, cy + 7, 23, cy, (uint16_t)COL_GOLD);
    fmd(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor((uint16_t)COL_WHITE);
    lcd.drawString(settings.language==LANG_RU ? cyrStr("ИГРАТЬ") : "PLAY", 27, cy);

    // WiFi иконка (x=97..117, немного шире)
    bool wConn = wifiMgr.isConnected();
    _mWifi(108, cy, wConn ? (uint16_t)0x07E0u : (uint16_t)0xF800u);

    // Время (x=125..178)
    flg(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor((uint16_t)COL_CYAN);
    lcd.drawString(timeGetString().c_str(), 152, cy);

    // Дата (x=186..252)
    time_t now = time(nullptr);
    struct tm *ti = localtime(&now);
    char dt[12];
    snprintf(dt, sizeof(dt), "%02d.%02d.%02d",
             ti->tm_mday, ti->tm_mon + 1, ti->tm_year % 100);
    fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textSec);
    lcd.drawString(dt, 218, cy);

    // Батарея — правее, с учётом нового размера 20+3=23px + 2 отступ
    _mBattery(293, cy);
}

// ── Правая панель (обложка + инфо) ─────────────────────────────
static void _menuDrawRightPanel(int romIdx) {
    const Theme565& t = getTheme();
    lcd.fillRect(M_PANEL_X, M_HDR_H, M_PANEL_W, M_DPAD_Y - M_HDR_H, t.bg);

    if (romIdx < 0 || romIdx >= sdMgr.count()) {
        // Заглушка: нет выбранной игры
        fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textSec);
        lcd.drawString("Select a ROM", M_PANEL_X + M_PANEL_W / 2,
                       M_HDR_H + (M_DPAD_Y - M_HDR_H) / 2);
        return;
    }

    const ROMInfo& rom = sdMgr.get(romIdx);

    // ── Обложка (cover art) ──────────────────────────────────
    char romName[64];
    _uiRomName(rom.path.c_str(), romName, sizeof(romName));
    _drawCoverArt(romName, M_PANEL_X + 2, M_HDR_H + 2, M_PANEL_W - 4, M_COVER_H - 4);

    // ── Инфо-секция (y=138..200) ─────────────────────────────
    lcd.fillRect(M_PANEL_X, M_INFO_Y, M_PANEL_W, M_INFO_H, t.header);
    lcd.drawFastHLine(M_PANEL_X, M_INFO_Y, M_PANEL_W, t.accent);

    int ix = M_PANEL_X + 4;
    int iy = M_INFO_Y + 9;

    // Playtime
    fsm(); lcd.setTextDatum(ML_DATUM);
    lcd.setTextColor(t.textSec); lcd.drawString("T:", ix, iy);
    lcd.setTextColor(t.textPri);
    lcd.drawString(_fmtPlaytime(GameStats::playTimeGet(rom.path.c_str())).c_str(), ix + 15, iy);
    iy += 18;

    // Size
    char szStr[12];
    uint32_t kb = rom.size / 1024;
    if (kb > 0) snprintf(szStr, sizeof(szStr), "%uKB", (unsigned)kb);
    else        snprintf(szStr, sizeof(szStr), "%uB",  (unsigned)rom.size);
    lcd.setTextColor(t.textSec); lcd.drawString("S:", ix, iy);
    lcd.setTextColor(t.textPri); lcd.drawString(szStr, ix + 15, iy);
    iy += 18;

    // ROM name (truncated to fit panel)
    String sName = rom.name.length() > 13
                   ? rom.name.substring(0, 11) + ".."
                   : rom.name;
    lcd.setTextColor(t.textSec); lcd.drawString(sName.c_str(), ix, iy);

    _coverLastRom = romIdx;
}

// ── Стрелки прокрутки (в списке, правый край) ──────────────────
static void _menuDrawScrollArrows(int total) {
    const Theme565& t = getTheme();
    int ax = M_LIST_W - 9;
    if (_menuOffset > 0) {
        int ay = M_HDR_H + 10;
        lcd.fillTriangle(ax, ay, ax - 5, ay + 8, ax + 5, ay + 8, t.accent);
    }
    if (_menuOffset + M_ROWS < total) {
        int ay = M_DPAD_Y - 10;
        lcd.fillTriangle(ax, ay, ax - 5, ay - 8, ax + 5, ay - 8, t.accent);
    }
}

// ── Частичное обновление (только изменившиеся строки + правая панель) ──
static void _menuPartialUpdate(int oldIdx, int newIdx) {
    const Theme565&    t  = getTheme();
    const ThemePlugin* tp = ThemeRegistry::active();
    int total = _menuListCount();

    int oldSlot = oldIdx - _menuOffset;
    int newSlot = newIdx - _menuOffset;

    // Старая строка (снять выделение)
    lcd.fillRect(0, M_HDR_H + oldSlot * M_ROW_H, M_LIST_W - 1, M_ROW_H, t.bg);
    if (oldIdx >= 0 && oldIdx < total)
        tp->drawRomRow(_menuActual(oldIdx), oldSlot, false);

    // Новая строка (выделить)
    lcd.fillRect(0, M_HDR_H + newSlot * M_ROW_H, M_LIST_W - 1, M_ROW_H, t.bg);
    if (newIdx >= 0 && newIdx < total)
        tp->drawRomRow(_menuActual(newIdx), newSlot, true);

    // Разделитель
    lcd.drawFastVLine(M_PANEL_X - 1, M_HDR_H, M_DPAD_Y - M_HDR_H, t.accent);

    // Стрелки
    lcd.fillRect(M_LIST_W - 16, M_HDR_H, 16, M_DPAD_Y - M_HDR_H, t.bg);
    _menuDrawScrollArrows(total);

    // Правая панель
    _menuDrawRightPanel(_menuActual(newIdx));
}

// ── Диалог подтверждения удаления ──────────────────────────────
static bool _menuConfirmDelete(const char *name) {
    const Theme565& t = getTheme();
    const int PX = 10, PY = 70, PW = 300, PH = 92;
    const int BW = 130, BH = 26, BY = PY + 56;

    lcd.fillRoundRect(PX, PY, PW, PH, 8, t.header);
    lcd.drawRoundRect(PX, PY, PW, PH, 8, t.danger);

    bool ru = (settings.language==LANG_RU);
    fmd(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.danger);
    lcd.drawString(ru ? cyrStr("Удалить ROM?") : "Delete ROM?", SCREEN_W / 2, PY + 20);

    String n = String(name);
    if (n.length() > 26) n = n.substring(0, 24) + "..";
    fsm(); lcd.setTextColor(t.textPri);
    lcd.drawString(n.c_str(), SCREEN_W / 2, PY + 38);

    lcd.fillRoundRect(PX + 8,           BY, BW, BH, 6, t.danger);
    lcd.fillRoundRect(PX + PW - 8 - BW, BY, BW, BH, 6, t.selected);
    fsm(); lcd.setTextColor((uint16_t)COL_WHITE); lcd.setTextDatum(MC_DATUM);
    lcd.drawString(ru ? cyrStr("ДА") : "YES Delete", PX + 8 + BW / 2,           BY + BH / 2);
    lcd.setTextColor(t.textSec);
    lcd.drawString(ru ? cyrStr("НЕТ") : "NO Cancel",  PX + PW - 8 - BW / 2,     BY + BH / 2);

    delay(300);
    uint32_t until = millis() + 8000;
    while (millis() < until) {
        if (touch.isTouched()) {
            delay(40);
            int tx = 0, ty = 0;
            touch.getXY(tx, ty);
            if (ty >= BY && ty < BY + BH) {
                if (tx >= PX + 8 && tx < PX + 8 + BW) return true;
            }
            return false;
        }
        delay(20);
    }
    return false;
}

// ── Три-точечное контекстное меню ──────────────────────────────
// Возвращает: 0=ничего, 1=Settings, 2=Gallery, 3=Music Player
static uint8_t _menuDotMenu() {
    const Theme565& t = getTheme();
    bool ru = (settings.language==LANG_RU);
    // Используем String чтобы cyrStr-буферы не перезаписывались
    static const char *sortEN[] = {"Sort A-Z","Sort Z-A","Sort Size","* Favs First","Sort Default"};
    static const char *sortRU[] = {"А-Я","Я-А","По размеру","* Избр. верх","По умолч."};
    static const char *itemEN[] = {"","Delete ROM","Gallery","Music Player","Web Upload","Settings"};
    static const char *itemRU[] = {"","Удалить ROM","Галерея","Плеер","Загрузить","Настройки"};
    const int N = 6;
    // Строим String-массив (копирует cyrStr-содержимое)
    String itemStrs[N];
    itemStrs[0] = ru ? cyrStr(sortRU[_sortMode % 5]) : sortEN[_sortMode % 5];
    for (int i = 1; i < N; i++)
        itemStrs[i] = ru ? cyrStr(itemRU[i]) : itemEN[i];

    const int PX = 172, PY = M_HDR_H + 1, PW = 146, PH = 184, RH = 30;

    auto drawItem = [&](int idx, bool hi) {
        int iy = PY + 1 + idx * RH;
        uint16_t bg = hi ? t.accent : t.header;
        lcd.fillRect(PX + 1, iy, PW - 2, RH, bg);
        if (idx > 0)     lcd.drawFastHLine(PX + 4, iy,      PW - 8, (uint16_t)COL_SEP);
        if (idx < N - 1) lcd.drawFastHLine(PX + 4, iy + RH, PW - 8, (uint16_t)COL_SEP);
        uint16_t ic = (idx == 1) ? (hi ? (uint16_t)COL_WHITE : t.danger)
                                 : (hi ? (uint16_t)COL_WHITE : t.textPri);
        fsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(ic);
        lcd.drawString(itemStrs[idx].c_str(), PX + 10, iy + RH / 2);
    };

    lcd.fillRoundRect(PX, PY, PW, PH, 6, t.header);
    lcd.drawRoundRect(PX, PY, PW, PH, 6, t.accent);
    for (int i = 0; i < N; i++) drawItem(i, false);

    delay(120);
    while (touch.isTouched()) delay(10);
    delay(60);

    uint32_t until = millis() + 6000;
    int choice = -1;
    int hiSel  = -1;

    while (millis() < until && choice == -1) {
        if (touch.isTouched()) {
            delay(40);
            int tx = 0, ty = 0;
            touch.getXY(tx, ty);
            if (tx >= PX && tx < PX + PW && ty >= PY && ty < PY + PH)
                choice = min((ty - PY - 1) / RH, N - 1);
            else
                { choice = -2; break; }
        }

        buttons.update();
        uint8_t btn = buttons.applyBtnMap(buttons.readNew());

        if (btn & (BTN_B | BTN_SEL)) { choice = -2; break; }

        if (btn & BTN_UP) {
            int prev = hiSel;
            hiSel = (hiSel <= 0) ? N - 1 : hiSel - 1;
            if (prev >= 0) drawItem(prev, false);
            drawItem(hiSel, true);
            until = millis() + 6000;
        }
        if (btn & BTN_DOWN) {
            int prev = hiSel;
            hiSel = (hiSel < 0) ? 0 : (hiSel + 1) % N;
            if (prev >= 0) drawItem(prev, false);
            drawItem(hiSel, true);
            until = millis() + 6000;
        }
        if (btn & (BTN_A | BTN_STA)) {
            if (hiSel >= 0) { choice = hiSel; break; }
        }
        delay(10);
    }

    switch (choice) {
        case 0: {  // Сортировка (0=default,1=A-Z,2=Z-A,3=size,4=favs first)
            _sortMode = (_sortMode + 1) % 5;
            _menuBuildSortedList();
            _menuSel = 0; _menuOffset = 0;
            menuDraw();
            break;
        }
        case 1: {  // Удалить ROM
            int actual = _menuActual(_menuSel);
            if (actual >= 0 && actual < sdMgr.count()) {
                const char* rname = sdMgr.get(actual).name.c_str();
                if (_menuConfirmDelete(rname)) {
                    sdMgr.removeROM(actual);
                    _menuBuildFavList();
                    _menuBuildSortedList();
                    int total = _menuListCount();
                    if (_menuSel >= total) _menuSel = max(0, total - 1);
                    _menuOffset = max(0, min(_menuOffset, max(0, total - M_ROWS)));
                    _coverLastRom = -2;
                }
            }
            menuDraw();
            break;
        }
        case 2:  // Gallery
            menuDraw();
            return 2;
        case 3:  // Music Player
            menuDraw();
            return 3;
        case 4:  // Web Upload
            menuDraw();
            return 4;
        case 5:  // Settings
            menuDraw();
            return 1;
        default:
            menuDraw();
            break;
    }
    return 0;
}

// ══════════════════════════════════════════════════════════════
// PUBLIC MENU FUNCTIONS
// ══════════════════════════════════════════════════════════════

void menuDraw() {
    const ThemePlugin* tp = ThemeRegistry::active();
    const Theme565& t     = getTheme();

    lcd.fillScreen(t.bg);

    // ── Шапка ──────────────────────────────────────────────────
    _menuDrawHeader();

    // ── Разделитель список | панель ────────────────────────────
    lcd.drawFastVLine(M_PANEL_X - 1, M_HDR_H, M_DPAD_Y - M_HDR_H, t.accent);

    // ── Список ROM ─────────────────────────────────────────────
    int total = _menuListCount();
    if (total == 0) {
        fmd(); lcd.setTextColor(t.textSec); lcd.setTextDatum(MC_DATUM);
        int ey = M_HDR_H + (M_DPAD_Y - M_HDR_H) / 2;
        bool ru = (settings.language==LANG_RU);
        if (_searchActive)
            lcd.drawString(ru ? cyrStr("Не найдено") : "No results", M_LIST_W / 2, ey);
        else if (_favMode)
            lcd.drawString(ru ? cyrStr("Нет избранных") : "No favourites", M_LIST_W / 2, ey);
        else {
            lcd.drawString(cyrStr(S().noRoms),      M_LIST_W / 2, ey - 10);
            fsm(); lcd.drawString(cyrStr(S().noRomsHint), M_LIST_W / 2, ey + 10);
        }
    } else {
        int end = min(_menuOffset + M_ROWS, total);
        for (int i = _menuOffset; i < end; i++)
            tp->drawRomRow(_menuActual(i), i - _menuOffset, i == _menuSel);
        _menuDrawScrollArrows(total);
    }

    // ── Правая панель ──────────────────────────────────────────
    int selActual = _menuActual(_menuSel);
    _menuDrawRightPanel(selActual);

    // ── Подвал ─────────────────────────────────────────────────
    _menuDrawFooter();
}

// ── Обновление времени раз в минуту + батарея раз в 30 с ────────────────────
static uint8_t  _menuLastMin = 0xFF;
static uint32_t _batLastMs   = 0;

void menuTimeTick() {
    uint8_t m   = timeGetM();
    uint32_t ms = millis();

    // Батарея: обновляем показания каждые 5 секунд
    // (быстрое определение зарядки: при подключении TP4056 напряжение
    //  резко растёт, детектируем за 1 цикл = 5 с вместо 30 с)
    if (ms - _batLastMs >= 5000u || _batLastMs == 0) {
        _batLastMs = ms;
        batteryUpdate();
        batteryStatsAddTime(5);  // +5 сек к session + total (0 если идёт зарядка)
    }

    if (m == _menuLastMin) return;  // время не изменилось — перерисовывать не надо
    _menuLastMin = m;

    const Theme565& t = getTheme();
    int by = M_DPAD_Y, cy = M_DPAD_CY;

    // Перерисовываем зону: WiFi + время + дата + батарея (x=96..319)
    lcd.fillRect(96, by + 1, SCREEN_W - 96, M_BAR_H - 2, t.header);

    bool wConn = wifiMgr.isConnected();
    _mWifi(108, cy, wConn ? (uint16_t)0x07E0u : (uint16_t)0xF800u);

    flg(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor((uint16_t)COL_CYAN);
    lcd.drawString(timeGetString().c_str(), 152, cy);

    time_t now = time(nullptr);
    struct tm *ti = localtime(&now);
    char dt[12];
    snprintf(dt, sizeof(dt), "%02d.%02d.%02d",
             ti->tm_mday, ti->tm_mon + 1, ti->tm_year % 100);
    fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textSec);
    lcd.drawString(dt, 218, cy);

    _mBattery(293, cy);
}

// ── Анимация зарядки батареи (вызывать из loop() каждый кадр) ────────────────
// Работает только когда batteryCharging() == true.
// Обновляет иконку каждые 600 мс, продвигая фазу 0→1→2→3→0...
// Перерисовывает только зону батареи (x=278..319) — без мерцания всего футера.
void menuBatteryAnimTick() {
    if (!batteryCharging()) {
        _batAnimPhase = 0;
        return;
    }

    static uint32_t _lastAnim = 0;
    uint32_t ms = millis();
    if (ms - _lastAnim < 600u) return;
    _lastAnim = ms;

    _batAnimPhase = (_batAnimPhase + 1) & 3;  // 0..3

    const Theme565& t = getTheme();
    int by = M_DPAD_Y, cy = M_DPAD_CY;
    lcd.fillRect(274, by + 1, SCREEN_W - 274, M_BAR_H - 2, t.header);
    _mBattery(293, cy);
}

// ── Частичная отрисовка при скролле: только список + правая панель ──────────
// Не трогает шапку и футер — они не меняются при прокрутке.
// Экономит ~40% пикселей vs menuDraw() (fillRect 320×170 вместо fillScreen 320×240).
static void _menuRefreshListArea() {
    const ThemePlugin* tp = ThemeRegistry::active();
    const Theme565& t = getTheme();
    int total = _menuListCount();

    lcd.fillRect(0, M_HDR_H, SCREEN_W, M_DPAD_Y - M_HDR_H, t.bg);
    lcd.drawFastVLine(M_PANEL_X - 1, M_HDR_H, M_DPAD_Y - M_HDR_H, t.accent);

    if (total == 0) {
        fmd(); lcd.setTextColor(t.textSec); lcd.setTextDatum(MC_DATUM);
        int ey = M_HDR_H + (M_DPAD_Y - M_HDR_H) / 2;
        bool ru2 = (settings.language==LANG_RU);
        if (_searchActive)      lcd.drawString(ru2 ? cyrStr("Не найдено")    : "No results",    M_LIST_W / 2, ey);
        else if (_favMode)      lcd.drawString(ru2 ? cyrStr("Нет избранных") : "No favourites", M_LIST_W / 2, ey);
        else {
            lcd.drawString(cyrStr(S().noRoms),      M_LIST_W / 2, ey - 10);
            fsm(); lcd.drawString(cyrStr(S().noRomsHint), M_LIST_W / 2, ey + 10);
        }
    } else {
        int end = min(_menuOffset + M_ROWS, total);
        for (int i = _menuOffset; i < end; i++)
            tp->drawRomRow(_menuActual(i), i - _menuOffset, i == _menuSel);
        _menuDrawScrollArrows(total);
    }

    _menuDrawRightPanel(_menuActual(_menuSel));
}

void menuScrollUp() {
    if (_menuSel <= 0) return;
    int oldSel = _menuSel--;
    if (_menuSel < _menuOffset) {
        _menuOffset = _menuSel;
        _menuRefreshListArea();
    } else {
        _menuPartialUpdate(oldSel, _menuSel);
    }
}

void menuScrollDown() {
    int total = _menuListCount();
    if (_menuSel >= total - 1) return;
    int oldSel = _menuSel++;
    if (_menuSel >= _menuOffset + M_ROWS) {
        _menuOffset = _menuSel - M_ROWS + 1;
        _menuRefreshListArea();
    } else {
        _menuPartialUpdate(oldSel, _menuSel);
    }
}

uint8_t menuHandleTouch(int x, int y, int &romAction) {
    romAction = 0;

    // ── Подвал ─────────────────────────────────────────────────
    if (y >= M_DPAD_Y) {
        if (x <= 93)  { romAction = 1; return BTN_A; }  // ▶ PLAY
        if (x <= 124) return 0xE0;  // WiFi иконка → WiFi settings
        if (x <= 182) return 0xE1;  // Время → настройки времени
        if (x <= 252) return 0xE2;  // Дата → настройки системы
        if (x >= 270) return 0xE3;  // Батарея → Battery screen
        return 0;
    }

    // ── Шапка ──────────────────────────────────────────────────
    if (y < M_HDR_H) {
        if (x < 29) {
            // ★ Избранные
            _favMode = !_favMode;
            _searchActive = false;
            if (_favMode) _menuBuildFavList();
            _menuSel = 0; _menuOffset = 0;
            menuDraw(); soundClick();
            return 0;
        }
        if (x >= 29 && x < 62) {
            // 🔍 Поиск — сигнал в main.cpp
            return 0xC0;
        }
        if (x >= 280) {
            // ⋮ три точки
            uint8_t r = _menuDotMenu();
            if (r == 1) return BTN_SEL;  // → Settings
            if (r == 2) return 0xD0;     // → Gallery
            if (r == 3) return 0xD1;     // → Music Player
            if (r == 4) return 0xD2;     // → Web Upload
            return 0;
        }
        return 0;
    }

    // ── Правая панель — тап → ROM Info ────────────────────────
    if (x >= M_PANEL_X) {
        int actual = _menuActual(_menuSel);
        if (actual >= 0 && actual < sdMgr.count()) {
            romAction = 2; return BTN_A;
        }
        return 0;
    }

    // ── Стрелки прокрутки (правый край списка) ─────────────────
    if (x >= M_LIST_W - 16) {
        if (y < M_HDR_H + 24) { menuScrollUp();   return 0; }
        if (y > M_DPAD_Y - 24){ menuScrollDown(); return 0; }
    }

    // ── Строки ROM ─────────────────────────────────────────────
    if (y >= M_HDR_H && y < M_DPAD_Y) {
        int total = _menuListCount();
        if (total == 0) return 0;

        int row     = (y - M_HDR_H) / M_ROW_H;
        int listIdx = _menuOffset + row;
        if (listIdx < 0 || listIdx >= total) return 0;

        int actual = _menuActual(listIdx);
        if (actual < 0) return 0;

        // ★ Звезда в конце строки
        if (x >= M_LIST_W - 18) {
            GameStats::favToggle(sdMgr.get(actual).path.c_str());
            GameStats::save();
            soundClick();
            if (_favMode) {
                _menuBuildFavList();
                int tc = _menuListCount();
                if (_menuSel >= tc) {
                    _menuSel = max(0, tc - 1);
                    _menuOffset = max(0, _menuSel - M_ROWS / 2);
                }
            } else if (_sortMode == 4) {
                _menuBuildSortedList();  // обновить "favs first" порядок
            }
            menuDraw();
            return 0;
        }

        // Обычный тап — выбрать / запустить
        TapType tt = touch.checkDoubleTap(x, y, listIdx);
        if (listIdx != _menuSel) {
            int oldSel = _menuSel;
            _menuSel = listIdx;
            _menuPartialUpdate(oldSel, _menuSel);
        }
        if (tt == TAP_DOUBLE) {
            romAction = 1; return BTN_A;
        }
        return 0;
    }

    return 0;
}

int menuSelected() { return _menuActual(_menuSel); }

// ── Drag scroll (вызывается из main.cpp пока touch.rawTouched()) ──
void menuRawTouch(int x, int y) {
    // Только для области списка
    if (x >= M_PANEL_X || y < M_HDR_H || y >= M_DPAD_Y) {
        _dragActive = false;
        return;
    }
    if (!_dragActive) {
        _dragActive = true;
        _dragStartY = y;
        _dragOff0   = _menuOffset;
        return;
    }
    int dy = _dragStartY - y;
    int steps = dy / (M_ROW_H / 2);
    int newOff = _dragOff0 + steps;
    int total = _menuListCount();
    int maxOff = max(0, total - M_ROWS);
    newOff = constrain(newOff, 0, maxOff);
    if (newOff != _menuOffset) {
        _menuOffset = newOff;
        _menuSel    = constrain(_menuSel, _menuOffset, _menuOffset + M_ROWS - 1);
        menuDraw();
    }
}

void menuTouchUp() {
    _dragActive = false;
}

// ── Поиск ──────────────────────────────────────────────────────
void menuSetSearch(const char *q) {
    strncpy(_searchQuery, q, sizeof(_searchQuery) - 1);
    _searchQuery[sizeof(_searchQuery) - 1] = '\0';
    _searchActive = (_searchQuery[0] != '\0');
    if (_searchActive) {
        _menuBuildSearchList();
        _favMode = false;
    }
    _menuSel = 0; _menuOffset = 0;
    menuDraw();
}

void menuClearSearch() {
    _searchActive = false;
    _searchQuery[0] = '\0';
    _menuSel = 0; _menuOffset = 0;
    menuDraw();
}

// ══════════════════════════════════════════════════════════════
// ROM INFO POPUP
// ══════════════════════════════════════════════════════════════

// ── Helpers for ROM info screen ───────────────────────────────────────────────

// Extract ROM base name (strip directory + extension) from full path
static void _uiRomName(const char *path, char *out, size_t maxLen) {
    const char *slash = strrchr(path, '/');
    const char *base  = slash ? slash + 1 : path;
    strncpy(out, base, maxLen - 1);
    out[maxLen - 1] = '\0';
    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';
}

// Format total seconds → "Xh Ym" / "Ym Xs" / "Xs" / "---"
static String _fmtPlaytime(uint32_t secs) {
    if (secs == 0) return "---";
    uint32_t h = secs / 3600;
    uint32_t m = (secs % 3600) / 60;
    uint32_t s = secs % 60;
    char buf[24];
    if      (h > 0) snprintf(buf, sizeof(buf), "%uh %02um", (unsigned)h, (unsigned)m);
    else if (m > 0) snprintf(buf, sizeof(buf), "%um %02us", (unsigned)m, (unsigned)s);
    else            snprintf(buf, sizeof(buf), "%us",        (unsigned)s);
    return String(buf);
}

// ── Отрисовка .raw файла (общая функция) ─────────────────────────────────────
// Читает строго ПОСЛЕДОВАТЕЛЬНО все строки — без seek, надёжно на любой SD.
// Пиксели в файле pre-inverted (~rgb565). Делаем un-invert через pushImage().
// noArtHint — показывать подсказку если файл не найден.
static void _drawRawFile(const char *path, int dx, int dy, int dw, int dh,
                         bool noArtHint, const char *saveThmAs) {
    lcd.fillRect(dx, dy, dw, dh, 0x18C3);
    lcd.drawRect(dx, dy, dw, dh, 0x4208);

    File f = SD.open(path, FILE_READ);
    if (!f) {
        if (noArtHint) {
            lcd.setFont(&lgfx::fonts::DejaVu9);
            lcd.setTextDatum(MC_DATUM);
            lcd.setTextColor(0x4208);
            lcd.drawString("NO ART", dx + dw / 2, dy + dh / 2 - 7);
            lcd.setTextColor(0x2965);
            lcd.drawString("HOME>Screenshot", dx + dw / 2, dy + dh / 2 + 7);
        }
        return;
    }

    uint8_t hdr[4] = {};
    f.read(hdr, 4);
    uint16_t srcW = (uint16_t)(hdr[0] | (hdr[1] << 8));
    uint16_t srcH = (uint16_t)(hdr[2] | (hdr[3] << 8));

    if (srcW == 0 || srcH == 0 || srcW > 400 || srcH > 300) {
        f.close(); return;
    }

    // Быстрый путь: весь файл в PSRAM одним чтением
    uint32_t totalPx  = (uint32_t)srcW * srcH;
    uint16_t *fullBuf = (uint16_t *)heap_caps_malloc(totalPx * 2, MALLOC_CAP_SPIRAM);
    if (!fullBuf || f.read((uint8_t *)fullBuf, totalPx * 2) != (int)(totalPx * 2)) {
        // Запасной путь: построчное чтение
        heap_caps_free(fullBuf); fullBuf = nullptr;
        if (f.seek(4) == false) { f.close(); return; }  // перемотка после хедера
    }
    f.close();

    // Открываем файл thumbnail для записи (если нужно)
    File thmFile;
    if (saveThmAs && fullBuf) {
        SD.remove(saveThmAs);
        thmFile = SD.open(saveThmAs, FILE_WRITE);
        if (thmFile) {
            uint8_t ohdr[4] = { (uint8_t)dw, (uint8_t)(dw >> 8),
                                 (uint8_t)dh, (uint8_t)(dh >> 8) };
            thmFile.write(ohdr, 4);
        }
    }

    uint16_t *dstBuf = (uint16_t *)heap_caps_malloc(dw * sizeof(uint16_t),
                                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (fullBuf) {
        // Быстрый путь: данные в PSRAM, один setAddrWindow на весь блок
        lcd.startWrite();
        lcd.setAddrWindow(dx, dy, dw, dh);
        for (int dstY = 0; dstY < dh; dstY++) {
            int srcY = (dstY * (int)srcH) / dh;
            const uint16_t *srcRow = fullBuf + srcY * srcW;
            if ((int)srcW == dw) {
                // Thumbnail 1:1 — прямая передача без копирования
                lcd.writePixels(srcRow, dw, true);
                if (thmFile) thmFile.write((const uint8_t *)srcRow, dw * 2);
            } else {
                if (dstBuf) {
                    for (int x = 0; x < dw; x++)
                        dstBuf[x] = srcRow[(x * (int)srcW) / dw];
                    lcd.writePixels(dstBuf, dw, true);
                    if (thmFile) thmFile.write((uint8_t *)dstBuf, dw * 2);
                }
            }
        }
        lcd.endWrite();
        heap_caps_free(fullBuf);
    } else {
        // Запасной путь: построчное чтение (PSRAM не хватило)
        File f2 = SD.open(path, FILE_READ);
        uint16_t *srcBuf = dstBuf ? (uint16_t *)heap_caps_malloc(srcW * 2,
                                         MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) : nullptr;
        if (f2 && srcBuf) {
            f2.seek(4);
            lcd.startWrite();
            lcd.setAddrWindow(dx, dy, dw, dh);
            // Нам нужно итерировать по srcY и писать только нужные dstY
            int lastSrcY = -1;
            for (int dstY = 0; dstY < dh; dstY++) {
                int srcY = (dstY * (int)srcH) / dh;
                if (srcY != lastSrcY) {
                    // пропускаем строки SD до нужной
                    while (lastSrcY < srcY - 1) {
                        f2.seek(f2.position() + srcW * 2);
                        lastSrcY++;
                    }
                    f2.read((uint8_t *)srcBuf, srcW * 2);
                    lastSrcY = srcY;
                }
                for (int x = 0; x < dw; x++)
                    dstBuf[x] = srcBuf[(x * (int)srcW) / dw];
                lcd.writePixels(dstBuf, dw, true);
            }
            lcd.endWrite();
        }
        if (srcBuf) heap_caps_free(srcBuf);
        if (f2) f2.close();
    }

    if (thmFile) { thmFile.flush(); thmFile.close(); }
    heap_caps_free(dstBuf);
}

// ── Отрисовка стандартного BMP файла (24-bit RGB) ─────────────────────────────
// Читает BMP, масштабирует до dw×dh и выводит на дисплей.
// Используется для пронумерованных скриншотов (_NNN.bmp).
static void _drawBmpFile(const char *path, int dx, int dy, int dw, int dh, bool noArtHint) {
    lcd.fillRect(dx, dy, dw, dh, 0x18C3);
    lcd.drawRect(dx, dy, dw, dh, 0x4208);

    File f = SD.open(path, FILE_READ);
    if (!f) {
        if (noArtHint) {
            lcd.setFont(&lgfx::fonts::DejaVu9);
            lcd.setTextDatum(MC_DATUM); lcd.setTextColor(0x4208);
            lcd.drawString("NO ART", dx + dw / 2, dy + dh / 2 - 7);
            lcd.setTextColor(0x2965);
            lcd.drawString("HOME>Screenshot", dx + dw / 2, dy + dh / 2 + 7);
        }
        return;
    }

    // Читаем заголовок BMP (54 байта)
    uint8_t hdr[54] = {};
    if (f.read(hdr, 54) != 54 || hdr[0] != 'B' || hdr[1] != 'M') { f.close(); return; }
    uint32_t dataOff = (uint32_t)hdr[10] | ((uint32_t)hdr[11]<<8) | ((uint32_t)hdr[12]<<16) | ((uint32_t)hdr[13]<<24);
    uint16_t srcW   = (uint16_t)hdr[18] | ((uint16_t)hdr[19] << 8);
    int32_t  srcHs  = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23]<<8) | ((uint32_t)hdr[24]<<16) | ((uint32_t)hdr[25]<<24));
    uint16_t bpp    = (uint16_t)hdr[28] | ((uint16_t)hdr[29] << 8);
    if (bpp != 24 || srcW == 0 || srcW > 1024 || srcHs == 0) { f.close(); return; }
    uint16_t srcH   = (uint16_t)(srcHs < 0 ? -srcHs : srcHs);
    uint32_t rowBytes = ((uint32_t)srcW * 3 + 3) & ~3u;

    // Загружаем весь кадр в PSRAM (если хватает)
    uint32_t totalPx = (uint32_t)srcW * srcH;
    uint16_t *fullBuf = (uint16_t *)heap_caps_malloc(totalPx * 2, MALLOC_CAP_SPIRAM);
    uint8_t  *rowBuf  = (uint8_t  *)malloc(rowBytes);
    f.seek(dataOff);

    if (fullBuf && rowBuf) {
        for (int y = 0; y < (int)srcH; y++) {
            if (f.read(rowBuf, rowBytes) != (int)rowBytes) break;
            uint16_t *dst = fullBuf + (size_t)y * srcW;
            for (int x = 0; x < (int)srcW; x++) {
                uint8_t b = rowBuf[x*3+0], g = rowBuf[x*3+1], r = rowBuf[x*3+2];
                // BGR888 → lcd.color565() format
                dst[x] = ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
            }
        }
        f.close(); free(rowBuf); rowBuf = nullptr;

        uint16_t *dstBuf = (uint16_t *)heap_caps_malloc((size_t)dw * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        lcd.startWrite();
        lcd.setAddrWindow(dx, dy, dw, dh);
        for (int dstY = 0; dstY < dh; dstY++) {
            int srcY = (dstY * (int)srcH) / dh;
            const uint16_t *srcRow = fullBuf + (size_t)srcY * srcW;
            if ((int)srcW == dw) {
                lcd.writePixels(srcRow, dw, true);
            } else if (dstBuf) {
                for (int x = 0; x < dw; x++)
                    dstBuf[x] = srcRow[(x * (int)srcW) / dw];
                lcd.writePixels(dstBuf, dw, true);
            }
        }
        lcd.endWrite();
        heap_caps_free(fullBuf);
        if (dstBuf) heap_caps_free(dstBuf);
    } else {
        // Запасной путь: построчно без PSRAM
        if (!rowBuf) { heap_caps_free(fullBuf); f.close(); return; }
        uint16_t *dstBuf = (uint16_t *)malloc((size_t)dw * 2);
        uint16_t *srcBuf = (uint16_t *)malloc((size_t)srcW * 2);
        if (dstBuf && srcBuf) {
            lcd.startWrite();
            lcd.setAddrWindow(dx, dy, dw, dh);
            int lastSrcY = -1;
            for (int dstY = 0; dstY < dh; dstY++) {
                int srcY = (dstY * (int)srcH) / dh;
                if (srcY != lastSrcY) {
                    while (lastSrcY < srcY - 1) { f.seek(f.position() + rowBytes); lastSrcY++; }
                    if (f.read(rowBuf, rowBytes) == (int)rowBytes) {
                        for (int x = 0; x < (int)srcW; x++) {
                            uint8_t b = rowBuf[x*3+0], g = rowBuf[x*3+1], r = rowBuf[x*3+2];
                            srcBuf[x] = ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
                        }
                    }
                    lastSrcY = srcY;
                }
                for (int x = 0; x < dw; x++) dstBuf[x] = srcBuf[(x * (int)srcW) / dw];
                lcd.writePixels(dstBuf, dw, true);
            }
            lcd.endWrite();
        }
        free(dstBuf); free(srcBuf); free(rowBuf);
        heap_caps_free(fullBuf);
        f.close();
    }
}

// ── Генерация thumbnail без отрисовки (для предзагрузки при буте) ─────────────
// Читает .raw, масштабирует до tw×th, записывает .thm. Не трогает LCD.
static void _generateThumbnail(const char *rawPath, const char *thmPath, int tw, int th) {
    File f = SD.open(rawPath, FILE_READ);
    if (!f) return;
    uint8_t hdr[4] = {};
    f.read(hdr, 4);
    uint16_t srcW = (uint16_t)(hdr[0] | (hdr[1] << 8));
    uint16_t srcH = (uint16_t)(hdr[2] | (hdr[3] << 8));
    if (!srcW || !srcH || srcW > 400 || srcH > 300) { f.close(); return; }
    uint32_t totalPx = (uint32_t)srcW * srcH;
    uint16_t *buf = (uint16_t *)heap_caps_malloc(totalPx * 2, MALLOC_CAP_SPIRAM);
    if (!buf || f.read((uint8_t *)buf, totalPx * 2) != (int)(totalPx * 2)) {
        heap_caps_free(buf); f.close(); return;
    }
    f.close();
    uint16_t *row = (uint16_t *)heap_caps_malloc(tw * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!row) { heap_caps_free(buf); return; }
    SD.remove(thmPath);
    File out = SD.open(thmPath, FILE_WRITE);
    if (out) {
        uint8_t ohdr[4] = { (uint8_t)tw, (uint8_t)(tw >> 8),
                             (uint8_t)th, (uint8_t)(th >> 8) };
        out.write(ohdr, 4);
        for (int dy = 0; dy < th; dy++) {
            int sy = (dy * (int)srcH) / th;
            const uint16_t *sr = buf + sy * srcW;
            for (int x = 0; x < tw; x++)
                row[x] = sr[(x * (int)srcW) / tw];
            out.write((uint8_t *)row, tw * 2);
        }
        out.flush(); out.close();
    }
    heap_caps_free(buf);
    heap_caps_free(row);
}

// ── Публичная функция предзагрузки thumbnail при буте ─────────────────────────
void uiPreCacheThumbnails(void (*onProgress)(int cur, int total)) {
    int n = sdMgr.count();
    if (n == 0) return;

    // Быстрая проверка: есть ли вообще что генерировать
    int needsGen = 0;
    for (int i = 0; i < n; i++) {
        char romName[64], thmPath[96], rawPath[96];
        _uiRomName(sdMgr.get(i).path.c_str(), romName, sizeof(romName));
        snprintf(rawPath, sizeof(rawPath), "/Screenshots/%s.raw", romName);
        snprintf(thmPath, sizeof(thmPath), "/Screenshots/%s.thm", romName);
        if (SD.exists(rawPath) && !SD.exists(thmPath)) needsGen++;
    }
    if (needsGen == 0) return;  // все thumbnail уже есть — мгновенный возврат

    int done = 0;
    for (int i = 0; i < n; i++) {
        char romName[64], thmPath[96], rawPath[96];
        _uiRomName(sdMgr.get(i).path.c_str(), romName, sizeof(romName));
        snprintf(rawPath, sizeof(rawPath), "/Screenshots/%s.raw", romName);
        snprintf(thmPath, sizeof(thmPath), "/Screenshots/%s.thm", romName);
        if (!SD.exists(rawPath)) continue;   // нет обложки — пропуск
        if ( SD.exists(thmPath)) continue;   // thumbnail уже есть — пропуск
        if (onProgress) onProgress(done, needsGen);
        _generateThumbnail(rawPath, thmPath, M_PANEL_W, M_COVER_H);
        done++;
    }
}

// ── Обёртка для обложки игры (по имени ROM) ───────────────────────────────────
// Первый показ: грузит полный .raw (150KB) и сохраняет .thm (27KB) для следующего раза.
// Последующие показы: грузит .thm (27KB) — в ~5 раз быстрее.
static void _drawCoverArt(const char *romName, int dx, int dy, int dw, int dh) {
    char rawPath[96], thmPath[96];
    snprintf(rawPath, sizeof(rawPath), "/Screenshots/%s.raw", romName);
    snprintf(thmPath, sizeof(thmPath), "/Screenshots/%s.thm", romName);

    if (SD.exists(thmPath)) {
        // Thumbnail уже есть — грузим маленький файл
        _drawRawFile(thmPath, dx, dy, dw, dh, /*noArtHint=*/true);
    } else if (SD.exists(rawPath)) {
        // Первый раз: грузим полный и попутно сохраняем thumbnail
        _drawRawFile(rawPath, dx, dy, dw, dh, /*noArtHint=*/true, thmPath);
    }
    // Иначе: обложки нет — рисуем ничего (без ошибок SD)
}

// ── Main ROM info popup ────────────────────────────────────────────────────────
void showRomInfo(int idx) {
    if (idx < 0 || idx >= sdMgr.count()) return;
    const ROMInfo &rom = sdMgr.get(idx);
    const Theme565 &t = getTheme();

    char romName[64];
    _uiRomName(rom.path.c_str(), romName, sizeof(romName));

    // Layout constants
    const int PX = 4,  PY = 4;
    const int PW = SCREEN_W - 8, PH = SCREEN_H - 8;   // 312 × 232
    const int POP_HDR  = 30;                            // popup header height
    const int COVER_X  = PX + 10;
    const int COVER_W  = 110, COVER_H = 110;
    const int INFO_X   = COVER_X + COVER_W + 10;       // ≈ 134
    const int BTN_Y    = PY + PH - 36;                 // 200
    const int BTN_H    = 28;
    const int BTN_W    = (PW - 24) / 2;                // ≈ 144

    extern TouchHandler touch;
    extern ButtonHandler buttons;

    bool redraw = true;
    while (redraw) {
        redraw = false;

        bool isFav = GameStats::favCheck(rom.path.c_str());

        // ── Popup shell ────────────────────────────────────────────────────
        lcd.fillRoundRect(PX + 3, PY + 3, PW, PH, 10, 0x0841);    // shadow
        lcd.fillRoundRect(PX, PY, PW, PH, 10, t.bg);
        lcd.drawRoundRect(PX, PY, PW, PH, 10, t.accent);

        // Header bar
        lcd.fillRoundRect(PX, PY, PW, POP_HDR + 2, 10, t.header);
        lcd.fillRect(PX, PY + POP_HDR / 2, PW, POP_HDR / 2 + 2, t.header);
        lcd.drawFastHLine(PX, PY + POP_HDR, PW, t.accent);
        fmd();
        lcd.setTextColor(t.accent);
        lcd.setTextDatum(MC_DATUM);
        lcd.drawString(cyrStr(S().romInfo), SCREEN_W / 2, PY + POP_HDR / 2);

        // Game title (full width strip below header)
        const int TITLE_Y = PY + POP_HDR + 2;
        const int TITLE_H = 18;
        lcd.fillRect(PX + 1, TITLE_Y, PW - 2, TITLE_H, t.bg);
        String dispName = rom.name;
        if (dispName.length() > 28) dispName = dispName.substring(0, 26) + "..";
        fsm();
        lcd.setTextColor(t.textPri);
        lcd.setTextDatum(MC_DATUM);
        lcd.drawString(dispName.c_str(), SCREEN_W / 2, TITLE_Y + TITLE_H / 2);
        lcd.drawFastHLine(PX + 8, TITLE_Y + TITLE_H, PW - 16, 0x2945);

        // Cover art
        const int COVER_Y2 = TITLE_Y + TITLE_H + 4;
        _drawCoverArt(romName, COVER_X, COVER_Y2, COVER_W, COVER_H);

        // Info rows (right of cover)
        fsm();
        lcd.setTextDatum(ML_DATUM);
        int iy = COVER_Y2 + 4;

        auto infoRow = [&](const char *lbl, const String &val,
                           uint16_t valCol = 0) {
            lcd.setTextColor(t.textSec);
            lcd.drawString(lbl, INFO_X, iy);
            iy += 14;
            lcd.setTextColor(valCol ? valCol : t.textPri);
            lcd.drawString(val.c_str(), INFO_X, iy);
            iy += 18;
        };

        // Size
        String sz = rom.size < 1024
                    ? String(rom.size) + " B"
                    : String(rom.size / 1024) + " KB";
        infoRow("Size:", sz);

        // Playtime
        infoRow("Played:", _fmtPlaytime(GameStats::playTimeGet(rom.path.c_str())));

        // Favourite
        infoRow("Fav:", isFav ? "YES  *" : "No",
                isFav ? 0xFFE0 : t.textSec);

        // Path (short)
        String shortPath = rom.path;
        if (shortPath.length() > 18) shortPath = ".." + shortPath.substring(shortPath.length() - 16);
        infoRow("Path:", shortPath);

        // ── Bottom buttons ─────────────────────────────────────────────────
        int btn1X = PX + 8;
        int btn2X = PX + PW - 8 - BTN_W;

        // FAV button
        uint16_t favBg  = isFav ? 0xFBE0 : t.hilite;
        uint16_t favTxt = isFav ? 0x8400 : t.selText;
        lcd.fillRoundRect(btn1X, BTN_Y, BTN_W, BTN_H, 6, favBg);
        lcd.drawRoundRect(btn1X, BTN_Y, BTN_W, BTN_H, 6, isFav ? 0xFD40 : t.accent);
        fsm(); lcd.setTextColor(favTxt); lcd.setTextDatum(MC_DATUM);
        lcd.drawString(isFav ? "* Unfavourite" : "* Favourite",
                       btn1X + BTN_W / 2, BTN_Y + BTN_H / 2);

        // OK button
        lcd.fillRoundRect(btn2X, BTN_Y, BTN_W, BTN_H, 6, t.accent);
        lcd.setTextColor(t.header);
        lcd.drawString("OK  [B]", btn2X + BTN_W / 2, BTN_Y + BTN_H / 2);

        // ── Input loop ─────────────────────────────────────────────────────
        delay(250);
        uint32_t until = millis() + 12000;
        while (millis() < until) {

            if (touch.isTouched()) {
                delay(40);
                int tx = 0, ty = 0;
                touch.getXY(tx, ty);

                // Tap FAV button
                if (ty >= BTN_Y && ty < BTN_Y + BTN_H &&
                    tx >= btn1X && tx < btn1X + BTN_W) {
                    GameStats::favToggle(rom.path.c_str());
                    GameStats::save();
                    soundClick();
                    redraw = true;
                    break;
                }
                // Tap OK or anywhere else → close
                break;
            }

            buttons.update();
            uint8_t b = buttons.readNew();
            if (b & BTN_A) {                  // A → toggle favourite
                GameStats::favToggle(rom.path.c_str());
                GameStats::save();
                soundClick();
                redraw = true;
                break;
            }
            if (b) break;                     // any other button → close
            delay(20);
        }
    }
}

// ══════════════════════════════════════════════════════════════
// НАСТРОЙКИ — данные
// ══════════════════════════════════════════════════════════════

// gi=41 → подменю Appearance (virtual cat=4), gi=42 → подменю Controls (virtual cat=5)
static const char *_catName[] = {
    "Display", "Audio", "System", "Info",
    "Appearance",  // cat=4: virtual sub из Display (gi=41)
    "Controls",    // cat=5: virtual sub из System  (gi=42)
    "Debug",       // cat=6: virtual, из System gi=25
    "Battery",     // cat=7: virtual, из System gi=33
    "Web Debug"    // cat=8: virtual, из Debug  gi=37
};
static const char* _catLabel(int cat) {
    if (settings.language == LANG_RU) {
        static const char *ru[] = {
            "Дисплей", "Звук", "Система", "Инфо",
            "Внеш.вид", "Геймпад", "Debug", "Батарея", "Web"
        };
        if (cat >= 0 && cat < 9) return cyrStr(ru[cat]);
    }
    return (cat >= 0 && cat < 9) ? _catName[cat] : "";
}

// ── _catItems[][8]: максимум 8 пунктов на категорию ─────────────────────
// gi=24 → WiFi manager (0x80), gi=25 → Debug sub, gi=26 → OTA (0xA0)
// gi=33 → Battery screen, gi=41 → Appearance sub, gi=42 → Controls sub
static const int _catItems[][8] = {
    { 0, 4, 7, 34, 35, 41, -1, -1 },           // 0: Display  (41=Appearance→)
    { 1, 14, 8, 9, -1, -1, -1, -1 },           // 1: Audio
    { 6, 17, 18, 19, 24, 26, 33, 42 },         // 2: System   (42=Controls→, убран Debug в Info)
    { 10, 11, 28, 25, -1, -1, -1, -1 },        // 3: Info     (25=Debug→ перенесён сюда)
    { 2, 3, 29, -1, -1, -1, -1, -1 },          // 4: Appearance virtual (was cat=2)
    { 12, 13, 15, 16, 30, 31, 32, 36 },        // 5: Controls virtual   (was cat=4)
    { 5, 20, 21, 22, 23, 37, -1, -1 },         // 6: Debug virtual
    { -1, -1, -1, -1, -1, -1, -1, -1 },        // 7: Battery virtual
    { 38, -1, -1, -1, -1, -1, -1, -1 },        // 8: Web Debug virtual
};
#define CAT_COUNT 4  // в сетке видны только 0..3; cat=4..8 — виртуальные

static int _prevCat = -1;   // для возврата из Debug sub-экрана в System

// ── Кеш версії Pico (запит по UART, оновлюється раз на 60с) ─────────────────
// -2 = ещё не запрашивали, -1 = Pico не ответил, >0 = MAJOR*100+MINOR
static int      _cachedPicoVer   = -2;
static uint32_t _picoVerCacheMs  = 0;

static int getCachedPicoVer() {
    // Если кеш свежий (< 60с) — возвращаем без запроса
    uint32_t now = millis();
    if (_cachedPicoVer != -2 && now - _picoVerCacheMs < 60000UL)
        return _cachedPicoVer;
    // Запрашиваем у Pico
    _cachedPicoVer  = picoOtaGetVersion();
    _picoVerCacheMs = millis();
    return _cachedPicoVer;
}

static String picoVerStr() {
    int v = getCachedPicoVer();
    if (v <= 0) return "N/A";
    char buf[12];
    snprintf(buf, sizeof(buf), "v%d.%d", v / 100, v % 100);
    return String(buf);
}

// ── Публичная функция: вызвать из setup() после buttons.init() ───────────────
// Запрашивает версию Pico и кешует её — потом Settings и Info открываются мгновенно.
void settingsPrefetchPicoVer() {
    _cachedPicoVer  = picoOtaGetVersion();
    _picoVerCacheMs = millis();
    if (_cachedPicoVer > 0)
        Serial.printf("[UI] Pico fw: v%d.%d\n", _cachedPicoVer/100, _cachedPicoVer%100);
    else
        Serial.println("[UI] Pico fw: N/A (not connected or old fw)");
}

static int  _settingsCat    = -1;
static int  _catItemIdx     =  0;
static int  _gridSelected   = -1;
static int  _detailRowH     = 32;  // реальная высота строки в текущем detail
static int  _detailListTop  = 44;  // реальный Y начала списка
static int  _detailOffset   =  0;  // первая видимая строка (scroll)
static int  _detailVisible  =  5;  // сколько строк влезает на экран
static uint32_t _detailOpenedMs = 0;  // момент открытия — debounce тача
static bool _exitOnBack     = false; // открыто из подвала → Back = выйти из настроек

static int catItemCount(int cat) {
    int n = 0;
    while (n < 8 && _catItems[cat][n] >= 0) n++;
    return n;
}

static int globalSettingIdx(int cat, int itemInCat) {
    return _catItems[cat][itemInCat];
}

static const char* _onOff(bool v) {
    return v ? (settings.language==LANG_RU ? cyrStr("Вкл") : "On")
             : (settings.language==LANG_RU ? cyrStr("Выкл"): "Off");
}

static String settingValue(int gi) {
    const char *langs[]  = {"RU","EN","CZ"};
    bool ru = (settings.language==LANG_RU);
    const char *scales_fit = ru ? cyrStr("Авто") : "Fit";
    const char *scales[] = { scales_fit, "4:3", "1:1" };
    const char *snds[]   = {"Beep","Click","Chime"};
    switch(gi) {
        case 0:  return String(settings.brightness)+"%";
        case 1:  return String(settings.volume)+"%";
        case 2:  return ThemeRegistry::activeName();
        case 3:  return langs[(int)settings.language%LANG_COUNT];
        case 4:  return scales[(int)settings.scale%3];
        case 5:  return _onOff(settings.showFPS);
        case 6:  return _onOff(settings.autoSave);
        case 7:  return _onOff(settings.autoBrightness);
        case 8:  return _onOff(settings.soundEnabled);
        case 9:  return snds[(int)settings.soundType%3];
        case 10: return FIRMWARE_VERSION;
        case 11: return String(ESP.getFreeHeap()/1024)+"KB";
        case 12: return ru ? cyrStr("Изм >") : "Edit >";
        case 13: return buttons.isConnected() ? "OK" : "---";
        case 14: return String(settings.emuVolume)+"%";
        case 15: return _onOff(settings.vibroEnabled);
        case 16: return String(settings.vibroStrength)+"%";
        case 17: { char b[16]; snprintf(b,sizeof(b),"%02d %s", timeGetH(), ru ? cyrStr("ч") : "h"); return String(b); }
        case 18: { char b[16]; snprintf(b,sizeof(b),"%02d %s", timeGetM(), ru ? cyrStr("м") : "m"); return String(b); }
        case 19: return _onOff(settings.autoScroll);
        case 20: return _onOff(settings.diagButtons);
        case 21: return _onOff(settings.diagFPS);
        case 22: return _onOff(settings.diagEmu);
        case 23: return _onOff(settings.diagTouch);
        case 24: {  // WiFi
            if (wifiMgr.isConnected()) return wifiMgr.getSSID();
            return settings.wifiSSID[0] ? settings.wifiSSID : "Tap >";
        }
        case 25: return "Edit >";      // Debug sub-screen
        case 26: return "Check >";     // OTA update
        case 28: return picoVerStr();  // Pico firmware version (read-only)
        case 29: {  // NES Palette
            const char *pals[] = {"Default","Nestopia","FCEUX","Virtual"};
            return pals[settings.nesPalette % 4];
        }
        case 30: return settings.turboEnabled ? "On" : "Off";  // Turbo Buttons
        case 32: {  // Game Genie
            int n = 0;
            for (int i = 0; i < 8; i++) if (settings.ggCodes[i][0]) n++;
            if (!settings.ggEnabled) return "Off";
            static char buf[16]; snprintf(buf, sizeof(buf), "On (%d)", n); return buf;
        }
        case 33: {  // Battery screen
            int pct = batteryPercent();
            if (pct < 0) return "N/A >";
            static char buf[12];
            snprintf(buf, sizeof(buf), "%d%% >", pct);
            return buf;
        }
        case 31: {  // Turbo Mode
            if (settings.turboMask == 0x04) return "A";
            if (settings.turboMask == 0x08) return "B";
            if (settings.turboMask == 0x0C) return "A+B";
            return _onOff(false);
        }
        case 34: {  // Sleep Timeout
            static const char *lEn[]   = {"Off","1 min","2 min","5 min","10 min"};
            static const char *lRuUTF[]= {"Выкл","1 мин","2 мин","5 мин","10 мин"};
            static const uint8_t vals[]= {0,1,2,5,10};
            for (int i = 0; i < 5; i++)
                if (settings.sleepTimeout == vals[i])
                    return ru ? cyrStr(lRuUTF[i]) : lEn[i];
            return ru ? cyrStr("Выкл") : "Off";
        }
        case 35: return _onOff(settings.scanlines);  // Scanlines
        case 37: return ru ? cyrStr("Открыть >") : "Open >";
        case 38: {  // Web Debug toggle
            if (!webDbgRunning()) return (settings.language==LANG_RU) ? cyrStr("Выкл") : "Off";
            static char _dbgVal[24];
            snprintf(_dbgVal, sizeof(_dbgVal), "http://%s", webDbgIP());
            return _dbgVal;
        }
        case 41: return (settings.language==LANG_RU) ? cyrStr("→") : ">";
        case 42: return (settings.language==LANG_RU) ? cyrStr("→") : ">";
        case 36: {  // Game Shark
            int n = 0;
            for (int i = 0; i < 8; i++) if (settings.gsCodes[i][0]) n++;
            if (!settings.gsEnabled) return "Off";
            static char buf[16]; snprintf(buf, sizeof(buf), "On (%d)", n); return buf;
        }
    }
    return "";
}

static void settingInc(int gi) {
    switch(gi) {
        case 0: settings.brightness=min(100,(int)settings.brightness+10); setBrightness(settings.brightness); break;
        case 1: settings.volume=min(100,(int)settings.volume+10); break;
        case 2: {
            int n = ThemeRegistry::count();
            ThemeRegistry::setActive((ThemeRegistry::activeIndex() + 1) % n);
            strncpy(settings.themeName, ThemeRegistry::activeName(), sizeof(settings.themeName)-1);
            break;
        }
        case 3: settings.language=(Language)(((int)settings.language+1)%LANG_COUNT); break;
        case 4: settings.scale=(Scale)(((int)settings.scale+1)%SCALE_COUNT); break;
        case 5: settings.showFPS=!settings.showFPS; break;
        case 6: settings.autoSave=!settings.autoSave; break;
        case 7: settings.autoBrightness=!settings.autoBrightness; break;
        case 8: settings.soundEnabled=!settings.soundEnabled; break;
        case 9: settings.soundType=(SoundType)(((int)settings.soundType+1)%SND_COUNT); break;
        case 14: settings.emuVolume=min(100,(int)settings.emuVolume+10); break;
        case 15: settings.vibroEnabled=!settings.vibroEnabled;
                 buttons.picoHapticEnable(settings.vibroEnabled); break;
        case 16: settings.vibroStrength=min(100,(int)settings.vibroStrength+10); break;
        case 17: timeSet((timeGetH()+1)%24, timeGetM()); break;
        case 18: timeSet(timeGetH(), (timeGetM()+1)%60); break;
        case 19: settings.autoScroll  = !settings.autoScroll;  break;
        case 20: settings.diagButtons = !settings.diagButtons; break;
        case 21: settings.diagFPS     = !settings.diagFPS;     break;
        case 22: settings.diagEmu     = !settings.diagEmu;     break;
        case 23: settings.diagTouch   = !settings.diagTouch;   break;
        case 29: settings.nesPalette = (settings.nesPalette + 1) % 4; break;  // NES Palette
        case 30: settings.turboEnabled = !settings.turboEnabled; break;         // Turbo toggle
        case 32: settings.ggEnabled = !settings.ggEnabled; break;               // Game Genie toggle
        case 31: {  // Turbo mode cycle: Off→A→B→A+B
            static const uint8_t masks[] = {0x00, 0x04, 0x08, 0x0C};
            int cur = 0;
            for (int m = 0; m < 4; m++) if (masks[m] == settings.turboMask) { cur = m; break; }
            settings.turboMask = masks[(cur + 1) % 4];
            break;
        }
        case 34: {  // Sleep Timeout cycle forward
            static const uint8_t vals[] = {0,1,2,5,10};
            int cur = 0;
            for (int i = 0; i < 5; i++) if (settings.sleepTimeout == vals[i]) { cur = i; break; }
            settings.sleepTimeout = vals[(cur + 1) % 5];
            break;
        }
        case 35: settings.scanlines = !settings.scanlines; break;  // Scanlines toggle
        case 36: settings.gsEnabled = !settings.gsEnabled; break;  // Game Shark toggle
    }
}

static void settingDec(int gi) {
    switch(gi) {
        case 0: settings.brightness=max(10,(int)settings.brightness-10); setBrightness(settings.brightness); break;
        case 1: settings.volume=(settings.volume<10)?0:settings.volume-10; break;
        case 2: {
            int n = ThemeRegistry::count();
            ThemeRegistry::setActive((ThemeRegistry::activeIndex() - 1 + n) % n);
            strncpy(settings.themeName, ThemeRegistry::activeName(), sizeof(settings.themeName)-1);
            break;
        }
        case 3: settings.language=(Language)(((int)settings.language-1+LANG_COUNT)%LANG_COUNT); break;
        case 4: settings.scale=(Scale)(((int)settings.scale-1+SCALE_COUNT)%SCALE_COUNT); break;
        case 5: settings.showFPS=!settings.showFPS; break;
        case 6: settings.autoSave=!settings.autoSave; break;
        case 7: settings.autoBrightness=!settings.autoBrightness; break;
        case 8: settings.soundEnabled=!settings.soundEnabled; break;
        case 9: settings.soundType=(SoundType)(((int)settings.soundType-1+SND_COUNT)%SND_COUNT); break;
        case 14: settings.emuVolume=(settings.emuVolume<10)?0:settings.emuVolume-10; break;
        case 15: settings.vibroEnabled=!settings.vibroEnabled;
                 buttons.picoHapticEnable(settings.vibroEnabled); break;
        case 16: settings.vibroStrength=(settings.vibroStrength<10)?10:settings.vibroStrength-10; break;
        case 17: timeSet((timeGetH()-1+24)%24, timeGetM()); break;
        case 18: timeSet(timeGetH(), (timeGetM()-1+60)%60); break;
        case 19: settings.autoScroll  = !settings.autoScroll;  break;
        case 20: settings.diagButtons = !settings.diagButtons; break;
        case 21: settings.diagFPS     = !settings.diagFPS;     break;
        case 22: settings.diagEmu     = !settings.diagEmu;     break;
        case 23: settings.diagTouch   = !settings.diagTouch;   break;
        case 29: settings.nesPalette = (settings.nesPalette + 3) % 4; break;  // NES Palette (back)
        case 30: settings.turboEnabled = !settings.turboEnabled; break;
        case 32: settings.ggEnabled = !settings.ggEnabled; break;               // Game Genie toggle
        case 31: {  // Turbo mode cycle backward
            static const uint8_t masks[] = {0x00, 0x04, 0x08, 0x0C};
            int cur = 0;
            for (int m = 0; m < 4; m++) if (masks[m] == settings.turboMask) { cur = m; break; }
            settings.turboMask = masks[(cur + 3) % 4];
            break;
        }
        case 34: {  // Sleep Timeout cycle backward
            static const uint8_t vals[] = {0,1,2,5,10};
            int cur = 0;
            for (int i = 0; i < 5; i++) if (settings.sleepTimeout == vals[i]) { cur = i; break; }
            settings.sleepTimeout = vals[(cur + 4) % 5];
            break;
        }
        case 35: settings.scanlines = !settings.scanlines; break;  // Scanlines toggle
        case 36: settings.gsEnabled = !settings.gsEnabled; break;  // Game Shark toggle
    }
}

static void drawCatIcon(int cat, int cx, int cy, uint16_t c) {
    switch(cat) {
        case 0: iconDisplay (cx, cy, c); break;  // Display
        case 1: iconAudio   (cx, cy, c); break;  // Audio
        case 2: iconSystem  (cx, cy, c); break;  // System (was cat=3)
        case 3: iconInfo    (cx, cy, c); break;  // Info   (was cat=5)
        case 4: iconLook    (cx, cy, c); break;  // Appearance virtual
        case 5: iconControls(cx, cy, c); break;  // Controls virtual
        default: // Debug — иконка терминала
            lcd.fillRect(cx-9, cy-6, 18, 13, c);
            lcd.fillRect(cx-8, cy-5, 16, 11, 0x1082);
            lcd.drawLine(cx-6, cy-1, cx-3, cy+1, c);
            lcd.drawLine(cx-6, cy+3, cx-3, cy+1, c);
            lcd.drawFastHLine(cx-2, cy+1, 6, c);
            break;
    }
}

// ── Нижняя панель настроек: [← Back] [time] [nav arrows] ───────
static void drawSettingsBar(bool showBack, int navMode) {
    // navMode: 0=none, 1=up/down/LR, 2=up/down only
    const Theme565 &t = getTheme();
    bool flat = (t.style & THEME_STYLE_FLAT);
    int by = DPAD_Y;

    uint16_t barBg  = (t.style & THEME_STYLE_BEVEL) ? t.bg : t.header;
    uint16_t topLn  = (t.style & THEME_STYLE_BEVEL) ? t.shadow : (uint16_t)COL_TOPBAR;
    uint16_t divCol = (t.style & THEME_STYLE_BEVEL) ? t.shadow : (uint16_t)0x3186;
    uint16_t timCol = flat ? t.accent : (uint16_t)COL_CYAN;

    lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, barBg);
    lcd.drawFastHLine(0, by, SCREEN_W, topLn);
    int ty = by + BTNBAR_H/2;

    if (showBack) {
        // Кнопка BACK — стиль как PLAY в главном меню
        if (flat) {
            lcd.fillRect(4, by+5, 72, 34, t.selected);
            if (t.style & THEME_STYLE_BEVEL) {
                lcd.drawFastHLine(4,  by+5,  72, t.hilite);
                lcd.drawFastVLine(4,  by+5,  34, t.hilite);
                lcd.drawFastHLine(4,  by+38, 72, t.shadow);
                lcd.drawFastVLine(75, by+5,  34, t.shadow);
            }
        } else {
            lcd.fillRoundRect(4, by+5, 72, 34, 8, t.selected);
        }
        int ax = 16;
        uint16_t arrCol = flat ? (t.selText ? t.selText : 0xFFFF) : (uint16_t)COL_GOLD;
        uint16_t txtCol = flat ? (t.selText ? t.selText : 0xFFFF) : (uint16_t)COL_WHITE;
        lcd.fillTriangle(ax, ty, ax+8, ty-6, ax+8, ty+6, arrCol);
        fmd(); lcd.setTextColor(txtCol); lcd.setTextDatum(ML_DATUM);
        lcd.drawString(cyrStr(S().back), ax+12, ty);
    }
    // Vertical dividers
    lcd.drawFastVLine(80,  by+6, BTNBAR_H-12, divCol);
    lcd.drawFastVLine(240, by+6, BTNBAR_H-12, divCol);

    // Time (center)
    flg(); lcd.setTextColor(timCol); lcd.setTextDatum(MC_DATUM);
    lcd.drawString(timeGetString().c_str(), 160, ty);

    // Nav arrows (right zone x 240..319)
    if (navMode == 1) {
        // ▲ ▼ ◄ ►
        uint16_t nc = t.textSec;
        lcd.fillTriangle(258, ty-2, 253, ty+5, 263, ty+5, nc);  // ▲
        lcd.fillTriangle(278, ty+2, 273, ty-5, 283, ty-5, nc);  // ▼
        fsm(); lcd.setTextColor(nc); lcd.setTextDatum(MC_DATUM);
        lcd.drawString("< >", 303, ty);
    } else if (navMode == 2) {
        uint16_t nc = t.textSec;
        lcd.fillTriangle(268, ty-4, 260, ty+5, 276, ty+5, nc);  // ▲
        lcd.fillTriangle(298, ty+4, 290, ty-5, 306, ty-5, nc);  // ▼
    }
}

static void drawCategoryGrid() {
    const Theme565 &t = getTheme();
    bool flat = (t.style & THEME_STYLE_FLAT);
    lcd.fillScreen(t.bg);

    // ── Заголовок ─────────────────────────────────────────────
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    uint16_t hdrTxt;
    if      (t.style & THEME_STYLE_BEVEL) hdrTxt = t.selText ? t.selText : 0xFFFF;
    else if (flat)                         hdrTxt = t.accent;
    else                                   hdrTxt = (uint16_t)COL_GOLD;
    lcd.setTextColor(hdrTxt); lcd.setTextDatum(MC_DATUM);
    if (settings.language == LANG_RU) {
        // CyrDejaVu9 — единственный шрифт с кириллицей, рисуем жирным
        fmd();
        const char *s = cyrStr("НАСТРОЙКИ");
        lcd.drawString(s, SCREEN_W/2,   HDR_H/2);
        lcd.drawString(s, SCREEN_W/2+1, HDR_H/2);
    } else {
        flg();
        lcd.drawString("SETTINGS", SCREEN_W/2, HDR_H/2);
    }
    // Нижняя линия шапки
    if (t.style & THEME_STYLE_BEVEL) {
        lcd.drawFastHLine(0, HDR_H-2, SCREEN_W, t.hilite);
        lcd.drawFastHLine(0, HDR_H-1, SCREEN_W, t.shadow);
    } else {
        lcd.drawFastHLine(8, HDR_H, SCREEN_W-16, hdrTxt);
    }

    // ── Сетка тайлов — 2×2 ───────────────────────────────────
    const int COLS = 2, ROWS = (CAT_COUNT + COLS - 1) / COLS, PAD = 8;
    int cw = (SCREEN_W - PAD*(COLS+1)) / COLS;
    int ch = (DPAD_Y - HDR_H - 3 - PAD*(ROWS+1)) / ROWS;
    int r_tile = flat ? 4 : 9;  // радиус скругления плитки (flat = чуть скруглённые)

    for (int i = 0; i < CAT_COUNT; i++) {
        int col = i % COLS, row = i / COLS;
        int x = PAD + col*(cw+PAD);
        int y = HDR_H + 5 + PAD + row*(ch+PAD);
        bool sel = (i == _gridSelected);

        if (sel) {
            // ── ВЫБРАННЫЙ ─────────────────────────────────────
            lcd.fillRoundRect(x, y, cw, ch, r_tile, t.accent);
            if (t.style & THEME_STYLE_BEVEL) {
                // 3D sunken border
                lcd.drawFastHLine(x, y,      cw, t.shadow);
                lcd.drawFastVLine(x, y,      ch, t.shadow);
                lcd.drawFastHLine(x, y+ch-1, cw, t.hilite);
                lcd.drawFastVLine(x+cw-1, y, ch, t.hilite);
            } else {
                // Рамка accent/gold
                uint16_t borderSel = flat ? t.selText : (uint16_t)COL_GOLD;
                if (!borderSel) borderSel = t.accent;
                lcd.drawRoundRect(x,   y,   cw,   ch,   r_tile, borderSel);
                lcd.drawRoundRect(x+1, y+1, cw-2, ch-2, r_tile, borderSel);
            }
            // Иконка и текст на заливном фоне
            int icx = x + cw/2, icy = y + ch*2/5;
            uint16_t selIcCol = t.selText ? t.selText : t.header;
            drawCatIcon(i, icx, icy, selIcCol);
            fmd(); lcd.setTextDatum(MC_DATUM);
            lcd.setTextColor(selIcCol ? selIcCol : 0xFFFF);
            { String cl = _catLabel(i);
              lcd.drawString(cl.c_str(), x+cw/2,   y+ch-14);
              lcd.drawString(cl.c_str(), x+cw/2+1, y+ch-14); }
        } else {
            // ── ОБЫЧНЫЙ ───────────────────────────────────────
            lcd.fillRoundRect(x, y, cw, ch, r_tile, t.header);
            if (t.style & THEME_STYLE_BEVEL) {
                // 3D raised border
                lcd.drawFastHLine(x, y,      cw, t.hilite);
                lcd.drawFastVLine(x, y,      ch, t.hilite);
                lcd.drawFastHLine(x, y+ch-1, cw, t.shadow);
                lcd.drawFastVLine(x+cw-1, y, ch, t.shadow);
            } else {
                uint16_t glowDim = (uint16_t)((t.accent >> 1) & 0x7BEF);
                lcd.drawRoundRect(x-1, y-1, cw+2, ch+2, r_tile+1, glowDim);
                lcd.drawRoundRect(x,   y,   cw,   ch,   r_tile,   t.accent);
            }
            int icx = x + cw/2, icy = y + ch*2/5;
            uint16_t icCol = flat ? t.accent : (uint16_t)COL_GOLD;
            drawCatIcon(i, icx, icy, icCol);
            fmd(); lcd.setTextColor(icCol); lcd.setTextDatum(MC_DATUM);
            lcd.drawString(_catLabel(i), x+cw/2, y+ch-14);
        }
    }

    // ── Нижняя панель ─────────────────────────────────────────
    drawSettingsBar(true, 0);
}

// ── Info screen ────────────────────────────────────────────────
struct InfoRow { String label; String value; uint16_t vc; bool isDiv; };
static InfoRow _infoRows[52];
static int     _infoCount  = 0;
static int     _infoScroll = 0;

static void buildInfoRows() {
    _infoCount = 0; _infoScroll = 0;
    auto d = [&](const char *t){ _infoRows[_infoCount++] = {t,"",0,true}; };
    auto r = [&](const char *l, String v, uint16_t c=0){ _infoRows[_infoCount++]={l,v,c,false}; };

    // ── Прошивка ──────────────────────────────────────────────
    d("FIRMWARE");
    r("ESP32:",        FIRMWARE_VERSION,                   0x4EF0);
    r("Pico Controller:", picoVerStr(),
      getCachedPicoVer() > 0 ? (uint16_t)0x4EF0 : (uint16_t)0xFD20);
    r("Build Date:",   String(__DATE__) + " " + __TIME__);

    // ── Процессор ─────────────────────────────────────────────
    d("PROCESSOR");
    r("Chip:",         "ESP32 Xtensa LX6");
    r("Cores:",        "2 cores");
    r("Clock Speed:",  String(getCpuFrequencyMhz()) + " MHz");
    r("Flash Memory:", String(ESP.getFlashChipSize()/1024/1024) + " MB SPI");
    r("SDK:",          ESP.getSdkVersion());

    // ── Оперативная память ────────────────────────────────────
    d("MEMORY");
    uint32_t ht = ESP.getHeapSize(), hf = ESP.getFreeHeap();
    uint32_t hu = ht - hf;
    uint8_t  pct = (uint8_t)(hu * 100 / max(ht, (uint32_t)1));
    char memUsed[20]; snprintf(memUsed, sizeof(memUsed), "%u KB  (%u%%)", hu/1024, pct);
    char memFree[20]; snprintf(memFree, sizeof(memFree), "%u KB  (%u%%)", hf/1024, 100-pct);
    r("Total RAM:",    String(ht/1024) + " KB");
    r("RAM Used:",     String(memUsed),  pct > 75 ? (uint16_t)0xFB8E : (uint16_t)0xFD20);
    r("RAM Free:",     String(memFree),  hf > 80000 ? (uint16_t)0x4EF0 : (uint16_t)0xFD20);
    r("Min Free:",     String(ESP.getMinFreeHeap()/1024) + " KB  (lowest ever)");
    r("Biggest Block:",String(ESP.getMaxAllocHeap()/1024) + " KB  (max alloc)");
    if (psramFound()) {
        r("PSRAM Total:", String(ESP.getPsramSize()/1024)  + " KB", 0x4EF0);
        r("PSRAM Free:",  String(ESP.getFreePsram()/1024)  + " KB", 0x4EF0);
    } else {
        r("PSRAM:",       "Not installed — 320 KB only", 0xFD20);
    }

    // ── Дисплей ───────────────────────────────────────────────
    d("DISPLAY");
    r("Screen:",       "2.8\" TFT Color LCD");
    r("Driver Chip:",  "ST7789V via SPI");
    r("Resolution:",   "320 x 240 pixels");
    r("Touch Panel:",  "XPT2046 Resistive");
    r("Brightness:",   "GPIO 21  PWM ctrl");

    // ── Карта памяти ──────────────────────────────────────────
    d("STORAGE");
    if (SD.cardType() != CARD_NONE) {
        const char *ct[]={"Unknown","MMC","SD","SDHC","?"};
        char sdFree[24];
        uint64_t used = SD.usedBytes(), total = SD.cardSize();
        uint8_t sdPct = (uint8_t)(used * 100 / max(total, (uint64_t)1));
        snprintf(sdFree, sizeof(sdFree), "%llu MB  (%u%%)", used/1024/1024, sdPct);
        r("Card Type:",   ct[min((int)SD.cardType(), 4)]);
        r("Card Size:",   String(SD.cardSize()/1024/1024) + " MB");
        r("Used Space:",  String(sdFree),   sdPct > 85 ? (uint16_t)0xFB8E : (uint16_t)0xFD20);
        r("ROM Files:",   String(sdMgr.count()) + " NES ROMs found", 0x4EF0);
    } else {
        r("SD Card:",     "Not inserted", 0xFB8E);
        r("Tip:",         "Insert SD with /roms/");
    }

    // ── О системе ─────────────────────────────────────────────
    d("ABOUT");
    r("Board:",        "ESP32-2432S028 (CYD)");
    r("Controller:",   "Raspberry Pi Pico");
    r("NES Core:",     "nofrendo");
    r("Made by:",      "AleksOS  v" + String(FIRMWARE_VERSION).substring(
                       String(FIRMWARE_VERSION).lastIndexOf('v')+1), 0x4EF0);
}

#define INFO_ROW_H  18
#define INFO_DIV_H  22
#define INFO_AREA   (DPAD_Y - HDR_H)

static int infoTotalH() {
    int h=0;
    for(int i=0;i<_infoCount;i++) h+=_infoRows[i].isDiv?INFO_DIV_H:INFO_ROW_H;
    return h;
}

// Иконки для секций Info (по порядку появления в buildInfoRows)
static void drawInfoSectionIcon(int secIdx, int cx, int cy, uint16_t c) {
    switch (secIdx) {
        case 0: iconTag(cx,cy,c);      break;  // FIRMWARE
        case 1: iconChip(cx,cy,c);     break;  // PROCESSOR
        case 2: {                               // MEMORY — простая RAM-иконка
            lcd.fillRect(cx-7,cy-4,14,8,c);
            for(int s=-4;s<=4;s+=4) {
                lcd.drawFastVLine(cx+s,cy-6,3,c);
                lcd.drawFastVLine(cx+s,cy+4,3,c);
            }
            lcd.fillRect(cx-5,cy-2,10,5,0x1082);
            break;
        }
        case 3: iconDisplay(cx,cy,c);  break;  // DISPLAY
        case 4: iconSave(cx,cy,c);     break;  // STORAGE — дискета/SD
        case 5: iconInfo(cx,cy,c);     break;  // ABOUT
        default: iconInfo(cx,cy,c);    break;
    }
}

// ── Battery info screen (cat=7) ─────────────────────────────────────────────
// Заполняет _infoRows статистикой батареи (разделяет буфер с buildInfoRows).
static void buildBatteryRows() {
    _infoCount = 0; _infoScroll = 0;
    bool ru = (settings.language == LANG_RU);
    auto d = [&](const char *en, const char *ruUTF){
        _infoRows[_infoCount++] = { ru ? String(cyrStr(ruUTF)) : String(en), "", 0, true };
    };
    auto r = [&](const char *en, const char *ruUTF, String v, uint16_t c=0){
        _infoRows[_infoCount++] = { ru ? String(cyrStr(ruUTF)) : String(en), v, c, false };
    };

    // ── Текущий заряд ─────────────────────────────────────────
    d("CURRENT", "ЗАРЯД");
    int   pct   = batteryPercent();
    float vbat  = batteryVoltage();

    char pctBuf[12];
    if (pct >= 0) snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
    else          strncpy(pctBuf, "N/A", sizeof(pctBuf));
    uint16_t pctCol = (pct < 0)   ? (uint16_t)0xAD55u :
                      (pct <= 20) ? (uint16_t)0xF800u  :
                      (pct <= 50) ? (uint16_t)0xFFE0u  : (uint16_t)0x07E0u;
    r("Charge:", "Заряд:", pctBuf, pctCol);

    char vBuf[12];
    if (vbat > 0.1f) snprintf(vBuf, sizeof(vBuf), "%.2f V", vbat);
    else             strncpy(vBuf, "N/A", sizeof(vBuf));
    r("Voltage:", "Напряж.:", vBuf);

    const char *statusStr = batteryCharging()   ? (ru ? cyrStr("Зарядка") : "Charging")   :
                            (pct < 0)           ? (ru ? cyrStr("Нет дат.") : "No sensor") :
                            (pct <= 15)         ? (ru ? cyrStr("Мало!")    : "Low!")       :
                                                  (ru ? cyrStr("OK")       : "OK");
    uint16_t stCol = batteryCharging()               ? (uint16_t)0x07FFu  :
                     (pct >= 0 && pct <= 15)         ? (uint16_t)0xF800u  :
                                                       (uint16_t)0x4EF0u;
    r("Status:", "Статус:", statusStr, stCol);

    // ── Время работы ─────────────────────────────────────────
    d("RUNTIME", "РАБОТА");
    BattStats st = batteryStatsGet();

    auto fmtTime = [](uint32_t sec) -> String {
        if (sec < 60)    { char b[16]; snprintf(b,16,"%ds",sec);                        return b; }
        if (sec < 3600)  { char b[16]; snprintf(b,16,"%dm %ds",sec/60,sec%60);          return b; }
        if (sec < 86400) { char b[16]; snprintf(b,16,"%dh %dm",sec/3600,(sec%3600)/60); return b; }
        char b[16]; snprintf(b,16,"%dd %dh",sec/86400,(sec%86400)/3600); return b;
    };

    r("Since charge:", "С зарядки:", fmtTime(st.sessionSec));
    r("Total runtime:", "Всего:", fmtTime(st.totalSec));

    // ── История зарядок ───────────────────────────────────────
    d("HISTORY", "ИСТОРИЯ");
    r("Charge cycles:", "Циклов:", String(st.cycles), st.cycles > 0 ? (uint16_t)0x4EF0u : (uint16_t)0xAD55u);
}

static void drawInfoScreen(const char *title = "INFO") {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);

    // ── Заголовок ─────────────────────────────────────────────
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    flg(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor((uint16_t)COL_GOLD);
    lcd.drawString(title, SCREEN_W/2, HDR_H/2);
    lcd.drawFastHLine(0, HDR_H,   SCREEN_W, t.accent);
    lcd.drawFastHLine(0, HDR_H+1, SCREEN_W, t.accent);

    // ── Карточка ───────────────────────────────────────────────
    lcd.fillRoundRect(4, HDR_H+4, SCREEN_W-8, DPAD_Y-HDR_H-8, 6, t.header);
    lcd.drawFastHLine(0, DPAD_Y-2, SCREEN_W, t.accent);
    lcd.drawFastHLine(0, DPAD_Y-1, SCREEN_W, t.accent);

    // ── Список ────────────────────────────────────────────────
    const int LX = 10, RX = SCREEN_W - 9;
    int y         = HDR_H + 6;
    int skipped   = 0;
    int totalH    = infoTotalH();
    int secIdx    = -1;   // счётчик секций для иконок

    for (int i = 0; i < _infoCount; i++) {
        if (y >= DPAD_Y - 2) break;
        const InfoRow &row = _infoRows[i];
        int rh = row.isDiv ? INFO_DIV_H : INFO_ROW_H;
        if (skipped + rh <= _infoScroll) { skipped += rh; continue; }
        int yOff = (skipped < _infoScroll) ? (_infoScroll - skipped) : 0;
        skipped += rh;
        int ry = y;
        y += (rh - yOff);

        if (row.isDiv) {
            secIdx++;
            // Полоска-заголовок секции
            lcd.fillRect(5, ry + yOff, SCREEN_W - 10, rh - yOff, t.rowOdd);
            // Нижняя граница секции (не перекрывает предыдущий ряд)
            int secBottom = ry + rh - 1;
            if (secBottom < DPAD_Y - 2)
                lcd.drawFastHLine(5, secBottom, SCREEN_W-10, t.accent);
            // Иконка
            drawInfoSectionIcon(secIdx, LX + 8, ry + (rh / 2) + yOff/2, (uint16_t)COL_GOLD);
            // Заголовок секции
            fsm(); lcd.setTextColor((uint16_t)COL_GOLD); lcd.setTextDatum(ML_DATUM);
            lcd.drawString(row.label.c_str(), LX + 20, ry + (rh / 2) + yOff / 2);
            lcd.drawString(row.label.c_str(), LX + 21, ry + (rh / 2) + yOff / 2); // bold
        } else {
            // ── Разделитель и текст строки данных ─────────────────────
            int visH = rh - yOff;  // видимая высота строки

            // Клип: текст не выходит за пределы видимой части строки
            // Это предотвращает наезд текста частично-скрытой строки
            // на следующую строку
            if (yOff > 0) lcd.setClipRect(LX, ry, SCREEN_W - LX, visH);

            if (yOff == 0) lcd.drawFastHLine(LX + 2, ry, SCREEN_W - LX * 2, 0x2104);

            // Текст у центра видимой части (не полной строки)
            int textY = ry + visH / 2;

            fsm();
            lcd.setTextColor(t.textSec); lcd.setTextDatum(ML_DATUM);
            lcd.drawString(row.label.c_str(), LX + 6, textY);

            uint16_t vc = row.vc ? row.vc : (uint16_t)COL_WHITE;
            lcd.setTextColor(vc); lcd.setTextDatum(MR_DATUM);
            lcd.drawString(row.value.c_str(), RX - 4, textY);

            if (yOff > 0) lcd.clearClipRect();
        }
    }

    // ── Полоска прокрутки ──────────────────────────────────────
    if (totalH > INFO_AREA) {
        int trackH = INFO_AREA - 6;
        int thumbH = max(10, trackH * INFO_AREA / totalH);
        int thumbY = HDR_H + 4 + (_infoScroll * (trackH - thumbH)) / max(1, totalH - INFO_AREA);
        lcd.fillRect(SCREEN_W - 5, HDR_H + 4, 3, trackH, t.rowOdd);
        lcd.fillRect(SCREEN_W - 5, thumbY,     3, thumbH, t.accent);
    }

    drawSettingsBar(true, 2);
}

static void infoScrollBy(int delta) {
    int maxS=max(0,infoTotalH()-INFO_AREA);
    _infoScroll=constrain(_infoScroll+delta,0,maxS);
    drawInfoScreen(_settingsCat == 7 ? "BATTERY" : "INFO");
}

// ── Детальная категория настроек ───────────────────────────────

// ── Иконка для конкретной настройки (gi = global index) ──────────────────
static void iconSun(int cx,int cy,uint16_t c) {
    lcd.fillCircle(cx,cy,4,c);
    lcd.drawFastHLine(cx-8,cy,3,c); lcd.drawFastHLine(cx+6,cy,3,c);
    lcd.drawFastVLine(cx,cy-8,3,c); lcd.drawFastVLine(cx,cy+6,3,c);
    lcd.drawLine(cx-4,cy-4,cx-6,cy-6,c); lcd.drawLine(cx+4,cy-4,cx+6,cy-6,c);
    lcd.drawLine(cx-4,cy+4,cx-6,cy+6,c); lcd.drawLine(cx+4,cy+4,cx+6,cy+6,c);
}
static void iconSpeaker(int cx,int cy,uint16_t c) {
    lcd.fillRect(cx-7,cy-3,4,7,c);
    lcd.drawLine(cx-3,cy-3,cx+3,cy-7,c);
    lcd.drawLine(cx-3,cy+4,cx+3,cy+8,c);
    lcd.drawLine(cx+3,cy-7,cx+3,cy+8,c);
    lcd.drawLine(cx+6,cy-2,cx+8,cy-4,c);
    lcd.drawLine(cx+7,cy+1,cx+9,cy+1,c);
    lcd.drawLine(cx+6,cy+4,cx+8,cy+6,c);
}
static void iconSpeakerMute(int cx,int cy,uint16_t c) {
    lcd.fillRect(cx-7,cy-3,4,7,c);
    lcd.drawLine(cx-3,cy-3,cx+3,cy-7,c);
    lcd.drawLine(cx-3,cy+4,cx+3,cy+8,c);
    lcd.drawLine(cx+3,cy-7,cx+3,cy+8,c);
    lcd.drawLine(cx+6,cy-2,cx+10,cy+5,c);
    lcd.drawLine(cx+6,cy+5,cx+10,cy-2,c);
}
static void iconNote(int cx,int cy,uint16_t c) {
    lcd.fillRect(cx-1,cy-7,3,10,c);
    lcd.fillRect(cx+4,cy-9,3,9,c);
    lcd.fillRect(cx-1,cy+3,5,3,c);
    lcd.fillRect(cx+4,cy+2,4,2,c);
    lcd.fillRect(cx-1,cy-7,7,3,c);
}
static void iconDiamond(int cx,int cy,uint16_t c) {
    lcd.fillRect(cx-2,cy-8,5,4,c);
    lcd.fillRect(cx-6,cy-4,14,3,c);
    lcd.drawLine(cx-7,cy-2,cx,cy+8,c);
    lcd.drawLine(cx+8,cy-2,cx,cy+8,c);
}
static void iconGlobe(int cx,int cy,uint16_t c) {
    lcd.drawCircle(cx,cy,7,c);
    lcd.drawFastVLine(cx,cy-7,15,c);
    lcd.drawFastHLine(cx-7,cy,15,c);
    lcd.drawLine(cx-6,cy-4,cx+6,cy-4,c);
    lcd.drawLine(cx-6,cy+4,cx+6,cy+4,c);
}
static void iconResize(int cx,int cy,uint16_t c) {
    lcd.drawRect(cx-7,cy-5,8,7,c);
    lcd.drawRect(cx-1,cy-1,8,7,c);
    lcd.fillRect(cx-2,cy-1,3,2,c);
    lcd.fillRect(cx-2,cy-2,2,3,c);
}
static void iconClock2(int cx,int cy,uint16_t c) {
    lcd.drawCircle(cx,cy,7,c);
    lcd.drawFastVLine(cx,cy-5,6,c);
    lcd.drawFastHLine(cx,cy,4,c);
    lcd.fillCircle(cx,cy,1,c);
}
static void iconSave(int cx,int cy,uint16_t c) {
    lcd.drawRect(cx-6,cy-7,13,14,c);
    lcd.fillRect(cx-3,cy-7,5,4,c);
    lcd.fillRect(cx-1,cy-6,1,2,c);
    lcd.fillRect(cx-5,cy+2,10,5,c);
    lcd.fillRect(cx-3,cy+3,6,3,0x1082);
}
static void iconSunA(int cx,int cy,uint16_t c) {
    lcd.drawCircle(cx,cy,5,c);
    lcd.drawFastHLine(cx-7,cy,3,c); lcd.drawFastHLine(cx+5,cy,3,c);
    lcd.drawFastVLine(cx,cy-7,3,c); lcd.drawFastVLine(cx,cy+5,3,c);
    lcd.drawLine(cx-4,cy-4,cx-5,cy-5,c); lcd.drawLine(cx+4,cy-4,cx+5,cy-5,c);
    lcd.drawLine(cx-4,cy+4,cx-5,cy+5,c); lcd.drawLine(cx+4,cy+4,cx+5,cy+5,c);
    fsm(); lcd.setTextColor(c); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("A",cx,cy+1);
}
static void iconGamepad(int cx,int cy,uint16_t c) {
    lcd.fillRoundRect(cx-8,cy-4,16,8,3,c);
    lcd.fillRect(cx-6,cy-2,3,5,0x1082);
    lcd.fillRect(cx+4,cy-2,3,5,0x1082);
    lcd.fillCircle(cx-5,cy,1,c); lcd.fillCircle(cx+6,cy-1,1,c);
    lcd.fillCircle(cx-5,cy-3,1,c); lcd.fillCircle(cx+6,cy+1,1,c);
}
static void iconChip(int cx,int cy,uint16_t c) {
    lcd.fillRect(cx-5,cy-5,10,10,c);
    lcd.fillRect(cx-3,cy-3,6,6,0x1082);
    for(int i=-2;i<=2;i+=2) {
        lcd.drawFastHLine(cx-7,cy+i,3,c);
        lcd.drawFastHLine(cx+5,cy+i,3,c);
        lcd.drawFastVLine(cx+i,cy-7,3,c);
        lcd.drawFastVLine(cx+i,cy+5,3,c);
    }
}
static void iconVibrate(int cx,int cy,uint16_t c) {
    lcd.fillRect(cx-3,cy-6,6,12,c);
    for(int i=0;i<3;i++) {
        lcd.drawLine(cx-6,cy-4+i*4,cx-9,cy-2+i*4,c);
        lcd.drawLine(cx+6,cy-4+i*4,cx+9,cy-2+i*4,c);
    }
}
static void iconTag(int cx,int cy,uint16_t c) {
    lcd.fillRoundRect(cx-7,cy-5,14,10,3,c);
    lcd.fillCircle(cx-3,cy,2,0x1082);
    fsm(); lcd.setTextColor(0x1082); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("v",cx+2,cy+1);
}
static void iconHeadphones(int cx,int cy,uint16_t c) {
    lcd.drawArc(cx,cy,7,6,180,360,c);
    lcd.fillRect(cx-8,cy-1,3,6,c);
    lcd.fillRect(cx+6,cy-1,3,6,c);
}
static void iconHour(int cx,int cy,uint16_t c) {
    lcd.drawCircle(cx,cy,7,c);
    lcd.drawFastVLine(cx,cy-5,6,c);
    lcd.fillCircle(cx,cy,1,c);
}
static void iconMinute(int cx,int cy,uint16_t c) {
    lcd.drawCircle(cx,cy,7,c);
    lcd.drawFastHLine(cx,cy,5,c);
    lcd.fillCircle(cx,cy,1,c);
}
static void iconWifi(int cx,int cy,uint16_t c) {
    // 3 arcs radiating from a point (WiFi symbol)
    lcd.drawLine(cx-8, cy+1, cx, cy-8, c);
    lcd.drawLine(cx+8, cy+1, cx, cy-8, c);
    lcd.drawLine(cx-5, cy+1, cx, cy-5, c);
    lcd.drawLine(cx+5, cy+1, cx, cy-5, c);
    lcd.drawLine(cx-2, cy+1, cx, cy-2, c);
    lcd.drawLine(cx+2, cy+1, cx, cy-2, c);
    lcd.fillCircle(cx, cy+4, 2, c);
}
static void iconDownload(int cx,int cy,uint16_t c) {
    // Arrow pointing down + baseline
    lcd.drawFastVLine(cx, cy-8, 10, c);
    lcd.fillTriangle(cx-5, cy+3, cx+5, cy+3, cx, cy+8, c);
    lcd.drawFastHLine(cx-7, cy+9, 15, c);
}

static void drawSettingIcon(int gi, int cx, int cy, uint16_t c) {
    switch(gi) {
        case 0:             iconSun(cx,cy,c); break;
        case 1: case 14:    iconSpeaker(cx,cy,c); break;
        case 2:             iconDiamond(cx,cy,c); break;
        case 3:             iconGlobe(cx,cy,c); break;
        case 4:             iconResize(cx,cy,c); break;
        case 5:             iconClock2(cx,cy,c); break;
        case 6:             iconSave(cx,cy,c); break;
        case 7:             iconSunA(cx,cy,c); break;
        case 8:             settings.soundEnabled?iconSpeaker(cx,cy,c):iconSpeakerMute(cx,cy,c); break;
        case 9:             iconNote(cx,cy,c); break;
        case 10:            iconTag(cx,cy,c); break;
        case 11:            iconChip(cx,cy,c); break;
        case 12:            iconGamepad(cx,cy,c); break;
        case 13:            iconChip(cx,cy,c); break;
        case 15: case 16:   iconVibrate(cx,cy,c); break;
        case 17:            iconHour(cx,cy,c); break;
        case 18:            iconMinute(cx,cy,c); break;
        case 19:            iconClock2(cx,cy,c); break;   // auto-scroll
        case 20:            iconGamepad(cx,cy,c); break; // diagButtons
        case 21:            iconClock2(cx,cy,c); break;  // diagFPS
        case 22:            iconChip(cx,cy,c); break;    // diagEmu
        case 23:            iconLook(cx,cy,c); break;    // diagTouch
        case 24:            iconWifi(cx,cy,c); break;    // WiFi
        case 25:            // Debug sub — terminal icon
            lcd.fillRect(cx-9, cy-6, 18, 13, c);
            lcd.fillRect(cx-8, cy-5, 16, 11, 0x1082);
            lcd.drawLine(cx-6, cy-1, cx-3, cy+1, c);
            lcd.drawLine(cx-6, cy+3, cx-3, cy+1, c);
            lcd.drawFastHLine(cx-2, cy+1, 6, c);
            break;
        case 26:            iconDownload(cx,cy,c); break; // OTA
        case 28:            iconChip(cx,cy,c); break;    // Pico version (read-only)
        case 29:                iconDiamond(cx,cy,c); break; // NES Palette
        case 30: case 31: case 32: case 36: iconGamepad(cx,cy,c); break; // Turbo / Game Genie / Game Shark
        case 33: {  // Battery — мини-иконка батареи
            lcd.drawRect(cx-7, cy-3, 13, 7, c);
            lcd.fillRect(cx+6, cy-1, 2, 3, c);
            int pct = batteryPercent();
            int fw = (pct > 0) ? (pct * 10 + 50) / 100 : 0;
            if (fw > 10) fw = 10;
            if (fw > 0) lcd.fillRect(cx-6, cy-2, fw, 5, c);
            break;
        }
        case 34: {  // Sleep — moon icon
            lcd.drawCircle(cx, cy, 6, c);
            lcd.fillCircle(cx+3, cy-2, 5, 0x0000);
            break;
        }
        case 35: {  // Scanlines — horizontal lines icon
            lcd.drawFastHLine(cx-6, cy-4, 12, c);
            lcd.drawFastHLine(cx-6, cy-2, 12, c);
            lcd.drawFastHLine(cx-6, cy,   12, c);
            lcd.drawFastHLine(cx-6, cy+2, 12, c);
            lcd.drawFastHLine(cx-6, cy+4, 12, c);
            break;
        }
        case 37: case 38: {  // Web Debug — WiFi icon с жучком
            iconWifi(cx, cy-1, c);
            lcd.fillCircle(cx, cy+4, 2, c);
            break;
        }
        default:            iconInfo(cx,cy,c); break;
    }
}

static const char* getLabelForGi(int gi) {
    if (gi >= 0 && gi < 12) {
        // S().sLbl[] хранит UTF-8 — нужно cyrStr для CyrDejaVu шрифта
        if (settings.language == LANG_RU) return cyrStr(S().sLbl[gi]);
        return S().sLbl[gi];
    }
    bool ru = (settings.language == LANG_RU);
    switch(gi) {
        case 12: return ru ? cyrStr("Кнопки")        : "Remap Buttons";
        case 13: return "Pico";
        case 14: return ru ? cyrStr("Громк. эмул.")  : "Emu Volume";
        case 15: return ru ? cyrStr("Вибрация")      : "Vibration";
        case 16: return ru ? cyrStr("Сила вибрации") : "Vibro Strength";
        case 17: return ru ? cyrStr("Часы")          : "Hour";
        case 18: return ru ? cyrStr("Минуты")        : "Minute";
        case 19: return ru ? cyrStr("Авто-прокрутка"): "Auto Scroll";
        case 20: return ru ? cyrStr("Лог кнопок")   : "Buttons Log";
        case 21: return ru ? cyrStr("Лог FPS")      : "FPS Log";
        case 22: return ru ? cyrStr("Лог эмулятора"): "Emu Log";
        case 23: return ru ? cyrStr("Лог касаний")  : "Touch Log";
        case 24: return "WiFi";
        case 25: return ru ? cyrStr("Отладка")      : "Debug";
        case 26: return ru ? cyrStr("Обновление")   : "Check Update";
        case 28: return ru ? cyrStr("Контроллер")   : "Pico Controller";
        case 29: return ru ? cyrStr("Палитра NES")  : "NES Palette";
        case 30: return ru ? cyrStr("Турбо")        : "Turbo Buttons";
        case 31: return ru ? cyrStr("Режим турбо")  : "Turbo Mode";
        case 32: return "Game Genie";
        case 33: return ru ? cyrStr("Батарея")      : "Battery";
        case 34: return ru ? cyrStr("Сон экрана")   : "Sleep";
        case 35: return ru ? cyrStr("Сканлайны")    : "Scanlines";
        case 36: return "Game Shark";
        case 37: return ru ? cyrStr("Веб-отладка")  : "Web Debug";
        case 38: return ru ? cyrStr("Веб-консоль")  : "Web Debug";
        case 41: return ru ? cyrStr("Внеш.вид >")   : "Appearance >";
        case 42: return ru ? cyrStr("Управление >") : "Controls >";
    }
    return "";
}
static void drawCategoryDetail() {
    if (_settingsCat == 3) { buildInfoRows();    drawInfoScreen("INFO");    return; }
    if (_settingsCat == 7) { buildBatteryRows(); drawInfoScreen("BATTERY"); return; }

    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);

    // ── Заголовок ─────────────────────────────────────────────
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor((uint16_t)COL_GOLD);
    if (settings.language == LANG_RU) {
        fmd();
        String cl = _catLabel(_settingsCat);
        lcd.drawString(cl.c_str(), SCREEN_W/2,   HDR_H/2);
        lcd.drawString(cl.c_str(), SCREEN_W/2+1, HDR_H/2);
    } else {
        flg(); lcd.drawString(_catLabel(_settingsCat), SCREEN_W/2, HDR_H/2);
    }
    lcd.drawFastHLine(0, HDR_H,   SCREEN_W, t.accent);
    lcd.drawFastHLine(0, HDR_H+1, SCREEN_W, t.accent);

    // ── Геометрия ─────────────────────────────────────────────
    int n       = catItemCount(_settingsCat);
    int listTop = HDR_H + 4;
    int listH   = DPAD_Y - listTop - 5;

    // Фиксированная высота строки: 34px — одинаково для любого числа пунктов.
    // При n <= 4 есть запас, при n > 4 появляется прокрутка.
    const int rowH = 34;
    int visible = listH / rowH;            // сколько строк влезает (~4–5)
    if (visible < 1) visible = 1;

    // Clamp scroll: выбранный элемент всегда виден
    if (_catItemIdx < _detailOffset) _detailOffset = _catItemIdx;
    if (_catItemIdx >= _detailOffset + visible) _detailOffset = _catItemIdx - visible + 1;
    if (_detailOffset < 0) _detailOffset = 0;
    if (_detailOffset + visible > n) _detailOffset = max(0, n - visible);

    // Сохраняем для touch-handler
    _detailRowH    = rowH;
    _detailListTop = listTop;
    _detailVisible = visible;

    // Фон карточки (высота = сколько строк реально видно)
    int cardH = min(n, visible) * rowH + 4;
    lcd.fillRoundRect(4, listTop, SCREEN_W-8, cardH, 6, t.header);
    lcd.drawFastHLine(0, DPAD_Y-2, SCREEN_W, t.accent);
    lcd.drawFastHLine(0, DPAD_Y-1, SCREEN_W, t.accent);

    // ── Строки ────────────────────────────────────────────────
    int end = min(_detailOffset + visible, n);
    for (int i = _detailOffset; i < end; i++) {
        int gi   = globalSettingIdx(_settingsCat, i);
        int slot = i - _detailOffset;          // позиция на экране (0-based)
        int y    = listTop + 2 + slot * rowH;
        bool sel = (i == _catItemIdx);

        if (sel) {
            lcd.fillRoundRect(5, y+1, SCREEN_W-10, rowH-2, 4, t.rowOdd);
            lcd.fillRect(5, y+2, 3, rowH-4, t.accent);
        }
        if (slot > 0) lcd.drawFastHLine(12, y, SCREEN_W-24, 0x2104);

        uint16_t icCol = sel ? t.accent : (uint16_t)COL_GOLD;
        drawSettingIcon(gi, 22, y + rowH/2, icCol);

        // Копируем в String чтобы cyrStr-буфер не перезаписался при вызове settingValue
        String lbl = getLabelForGi(gi);
        String val = settingValue(gi);

        fmd(); lcd.setTextDatum(ML_DATUM);
        lcd.setTextColor((uint16_t)COL_WHITE);
        lcd.drawString(lbl.c_str(), 40, y + rowH/2);
        if (sel) lcd.drawString(lbl.c_str(), 41, y + rowH/2);

        uint16_t vCol = sel ? t.accent : (uint16_t)COL_GOLD;
        lcd.setTextColor(vCol);
        lcd.setTextDatum(MR_DATUM);
        lcd.drawString(val.c_str(), SCREEN_W-12, y + rowH/2);
        if (sel) lcd.drawString(val.c_str(), SCREEN_W-11, y + rowH/2);
    }

    // ── Индикаторы прокрутки ──────────────────────────────────
    if (_detailOffset > 0) {
        // ▲ справа вверху
        int ax = SCREEN_W - 10, ay = listTop + 6;
        lcd.fillTriangle(ax, ay, ax-5, ay+8, ax+5, ay+8, t.accent);
    }
    if (_detailOffset + visible < n) {
        // ▼ справа внизу
        int ax = SCREEN_W - 10, ay = DPAD_Y - 14;
        lcd.fillTriangle(ax, ay+8, ax-5, ay, ax+5, ay, t.accent);
    }

    drawSettingsBar(true, 0);
}

void settingsDraw() {
    _exitOnBack = false;   // обычный вход — сброс флага быстрого выхода
    if (_settingsCat < 0) drawCategoryGrid();
    else                  drawCategoryDetail();
}

void settingsOpenCat(int cat) {
    _settingsCat    = cat;
    _catItemIdx     = 0;
    _detailOffset   = 0;
    _prevCat        = -1;
    _gridSelected   = cat;
    _exitOnBack     = true;
    _detailOpenedMs = millis();
    drawCategoryDetail();
}

// ── Обработка тача настроек ────────────────────────────────────
static uint8_t handleCategoryGrid(int x, int y) {
    // Bottom bar: BACK (x < 80), time (80..239), no action (240+)
    if (y >= DPAD_Y) {
        if (x < 80) return BTN_B;   // BACK
        return 0;
    }
    if (y < HDR_H) return 0;

    const int COLS = 2, ROWS = (CAT_COUNT + COLS - 1) / COLS, PAD = 8;
    int cw = (SCREEN_W - PAD*(COLS+1)) / COLS;
    int ch = (DPAD_Y - HDR_H - 3 - PAD*(ROWS+1)) / ROWS;

    for (int i = 0; i < CAT_COUNT; i++) {
        int col = i % COLS, row = i / COLS;
        int cx = PAD + col*(cw+PAD);
        int cy = HDR_H + 5 + PAD + row*(ch+PAD);
        if (x >= cx && x < cx+cw && y >= cy && y < cy+ch) {
            if (_gridSelected == i) {
                // ── Второй тап по тому же — открываем ────────────
                _gridSelected   = -1;
                _settingsCat    = i;
                _catItemIdx     = 0;
                _detailOffset   = 0;
                _detailOpenedMs = millis();
                drawCategoryDetail();
            } else {
                // ── Первый тап — подсвечиваем (инверт + bold) ────
                _gridSelected = i;
                drawCategoryGrid();   // перерисовываем сетку с новой подсветкой
            }
            return 0;
        }
    }
    return 0;
}

static uint8_t handleCategoryDetail(int x, int y) {
    // ── Нижняя панель ─────────────────────────────────────────
    if (y >= DPAD_Y) {
        if (x < 80) {
            // BACK
            if (_exitOnBack) {
                // открыто из подвала главного экрана → сразу выйти из настроек
                _exitOnBack = false;
                _settingsCat = -1;
                _gridSelected = -1;
                _prevCat = -1;
                return BTN_B;
            }
            if (_settingsCat == 4 || _settingsCat == 5 ||
            _settingsCat == 6 || _settingsCat == 7 || _settingsCat == 8) {
                // virtual sub → возврат в родительскую категорию
                _settingsCat = (_prevCat >= 0) ? _prevCat : -1;
                _prevCat = -1;
                _detailOffset = 0;  // drawCategoryDetail auto-scrolls to _catItemIdx
                if (_settingsCat >= 0) drawCategoryDetail();
                else { _gridSelected = -1; drawCategoryGrid(); }
            } else {
                _settingsCat = -1;
                _gridSelected = -1;
                drawCategoryGrid();
            }
            return 0;
        }
        // Info / Battery: скролл в правой половине
        if (_settingsCat == 3 || _settingsCat == 7) {
            if (x < SCREEN_W/2) infoScrollBy(-INFO_ROW_H * 2);
            else                 infoScrollBy( INFO_ROW_H * 2);
        }
        return 0;
    }

    if (y < HDR_H) return 0;

    // ── Дебаунс: игнорируем тач 200 мс после открытия категории ──────────
    if (millis() - _detailOpenedMs < 200) return 0;

    // Info / Battery — скролл тапом по содержимому
    if (_settingsCat == 3 || _settingsCat == 7) {
        infoScrollBy(y < (HDR_H + DPAD_Y)/2 ? -INFO_ROW_H*2 : INFO_ROW_H*2);
        return 0;
    }

    // ── Строки настроек ───────────────────────────────────────
    // Используем АКТУАЛЬНУЮ геометрию из drawCategoryDetail()
    int n = catItemCount(_settingsCat);
    if (y >= _detailListTop && y < DPAD_Y - 2) {
        // slot = визуальная позиция строки на экране (0, 1, 2 …)
        // Реальный индекс с учётом прокрутки:
        int slot = (y - _detailListTop) / _detailRowH;
        int row  = _detailOffset + slot;
        if (row < 0 || row >= n) return 0;

        int gi = globalSettingIdx(_settingsCat, row);

        if (row != _catItemIdx) {
            // Первый тап — выбираем строку
            _catItemIdx = row;
            drawCategoryDetail();
            return 0;  // НЕ BTN_A — просто обновление экрана
        }
        // Второй тап по той же строке — меняем значение
        if (gi == 12) return 0x40;   // Remap Buttons → открыть remap
        if (gi == 24) return 0x80;   // WiFi → открыть WiFi менеджер
        if (gi == 26) return 0xA0;   // Check Update → OTA screen
        if (gi == 32) return 0xB0;   // Game Genie → открыть GG экран
        if (gi == 36) return 0xD0;   // Game Shark → открыть GS экран
        if (gi == 25 || gi == 33 || gi == 37 || gi == 41 || gi == 42) {
            // Debug→6 / Battery→7 / WebDebug→8 / Appearance→4 / Controls→5
            _prevCat = _settingsCat;
            _settingsCat = (gi==33)?7 : (gi==37)?8 : (gi==41)?4 : (gi==42)?5 : 6;
            _catItemIdx  = 0;
            _detailOffset = 0;
            _detailOpenedMs = millis();
            drawCategoryDetail();
            return 0;
        }
        if (gi == 38) return 0xC0;   // Web Debug toggle → main.cpp
        settingInc(gi);
        drawCategoryDetail();
        return 0;  // НЕ BTN_A — значение изменено, экран обновлён
    }
    return 0;
}

uint8_t settingsHandleTouch(int x, int y) {
    if (_settingsCat < 0) return handleCategoryGrid(x, y);
    else                  return handleCategoryDetail(x, y);
}


// ══════════════════════════════════════════════════════════════
// REMAP BUTTONS
// ══════════════════════════════════════════════════════════════

static const char *_btnPhysName[BTN_MAP_COUNT] = {
    "A","B","SELECT","START","UP","DOWN","LEFT","RIGHT"
};
static const struct { uint8_t mask; const char *name; } _nesFunc[] = {
    {0x01,"A"},{0x02,"B"},{0x04,"SELECT"},{0x08,"START"},
    {0x10,"UP"},{0x20,"DOWN"},{0x40,"LEFT"},{0x80,"RIGHT"},{0x00,"---"}
};
static const int _nesFuncCount = 9;
static int _rmIdx    = 0;
static int _rmOffset = 0;
#define REMAP_ROW_H  30
#define REMAP_VISIBLE ((DPAD_Y - HDR_H) / REMAP_ROW_H)

static int nesFuncIndex(uint8_t mask) {
    for (int i = 0; i < _nesFuncCount; i++)
        if (_nesFunc[i].mask == mask) return i;
    return 0;
}

void btnMapDraw() {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);
    // Золотой заголовок
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    flg(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor((uint16_t)COL_GOLD);
    lcd.drawString("Remap Buttons", SCREEN_W/2, HDR_H/2);
    lcd.drawFastHLine(8, HDR_H, SCREEN_W-16, (uint16_t)COL_GOLD);

    if (_rmIdx < _rmOffset) _rmOffset = _rmIdx;
    if (_rmIdx >= _rmOffset + REMAP_VISIBLE) _rmOffset = _rmIdx - REMAP_VISIBLE + 1;
    if (_rmOffset < 0) _rmOffset = 0;

    fsm(); lcd.setTextDatum(MR_DATUM);
    lcd.setTextColor(buttons.isConnected() ? t.ok : t.danger);
    lcd.drawString(buttons.isConnected() ? "Pico OK" : "No Pico", SCREEN_W-8, HDR_H/2);

    int end = min(_rmOffset + REMAP_VISIBLE, BTN_MAP_COUNT);
    for (int i = _rmOffset; i < end; i++) {
        int row = i - _rmOffset;
        int y   = HDR_H + row * REMAP_ROW_H;
        bool sel = (i == _rmIdx);
        uint16_t bg = sel ? t.selected : (row%2 ? t.rowOdd : t.rowEven);
        lcd.fillRect(0, y, SCREEN_W-4, REMAP_ROW_H, bg);
        if (sel) lcd.fillRect(0, y, 3, REMAP_ROW_H, t.accent);

        fsm();
        lcd.setTextColor(sel ? t.bg : t.textSec);
        lcd.setTextDatum(ML_DATUM);
        lcd.drawString(_btnPhysName[i], 10, y + REMAP_ROW_H/2);
        lcd.setTextDatum(MC_DATUM);
        lcd.drawString("->", SCREEN_W/2, y + REMAP_ROW_H/2);
        lcd.setTextColor(sel ? t.bg : t.accent);
        lcd.setTextDatum(MR_DATUM);
        lcd.drawString(_nesFunc[nesFuncIndex(settings.btnMap[i])].name, SCREEN_W-9, y + REMAP_ROW_H/2);
    }

    drawScrollbar(BTN_MAP_COUNT, REMAP_VISIBLE, _rmOffset);
    drawSettingsBar(true, 1);
}

uint8_t btnMapHandleTouch(int x, int y) {
    if (y >= DPAD_Y) {
        if (x < 80) return BTN_B;  // BACK
        if (x >= 80  && x < 160 && _rmIdx > 0)                { _rmIdx--; btnMapDraw(); }
        if (x >= 160 && x < 240 && _rmIdx < BTN_MAP_COUNT-1)  { _rmIdx++; btnMapDraw(); }
        if (x >= 240 && x < 280) {
            int fi=(nesFuncIndex(settings.btnMap[_rmIdx])-1+_nesFuncCount)%_nesFuncCount;
            settings.btnMap[_rmIdx]=_nesFunc[fi].mask; btnMapDraw();
        }
        if (x >= 280) {
            int fi=(nesFuncIndex(settings.btnMap[_rmIdx])+1)%_nesFuncCount;
            settings.btnMap[_rmIdx]=_nesFunc[fi].mask; btnMapDraw();
        }
        return 0;
    }
    if (y < HDR_H) return 0;
    if (y > HDR_H && y < DPAD_Y) {
        int row = (y - HDR_H) / REMAP_ROW_H;
        int idx = _rmOffset + row;
        if (idx >= 0 && idx < BTN_MAP_COUNT) { _rmIdx = idx; btnMapDraw(); }
    }
    return 0;
}


// ══════════════════════════════════════════════════════════════
// КНОПОЧНАЯ НАВИГАЦИЯ
// ══════════════════════════════════════════════════════════════

static int _gridCur = 0;

// highlightGridCell removed — full redraw used instead

uint8_t settingsNavBtn(uint8_t btn) {
    if (_settingsCat < 0) {
        int prev = _gridCur;
        if (btn & BTN_UP)    _gridCur = max(0, _gridCur - 3);
        if (btn & BTN_DOWN)  _gridCur = min(CAT_COUNT-1, _gridCur + 3);
        if (btn & BTN_LEFT)  _gridCur = max(0, _gridCur - 1);
        if (btn & BTN_RIGHT) _gridCur = min(CAT_COUNT-1, _gridCur + 1);

        // Физические кнопки — сразу показываем подсветку на текущем тайле
        if (_gridSelected != _gridCur) {
            _gridSelected = _gridCur;
            drawCategoryGrid();
        } else if (prev != _gridCur) {
            drawCategoryGrid();
        }

        // START — открываем категорию (кнопки не требуют двойного тапа)
        if (btn & BTN_STA) {
            _gridSelected   = -1;
            _settingsCat    = _gridCur;
            _catItemIdx     = 0;
            _detailOffset   = 0;
            _detailOpenedMs = millis();
            drawCategoryDetail();
        }
        if (btn & BTN_SEL) { _gridSelected = -1; return BTN_B; }  // SELECT = назад
    } else if (_settingsCat == 3) {
        // Info — только прокрутка, BACK → сетка
        if (btn & BTN_UP)   infoScrollBy(-INFO_ROW_H * 3);
        if (btn & BTN_DOWN) infoScrollBy( INFO_ROW_H * 3);
        if (btn & BTN_SEL) { _settingsCat = -1; _gridSelected = -1; drawCategoryGrid(); }
    } else if (_settingsCat == 7) {
        // Battery info — только прокрутка, BACK → parent (System)
        if (btn & BTN_UP)   infoScrollBy(-INFO_ROW_H * 3);
        if (btn & BTN_DOWN) infoScrollBy( INFO_ROW_H * 3);
        if (btn & BTN_SEL) {
            _settingsCat = (_prevCat >= 0) ? _prevCat : -1;
            _prevCat = -1;
            if (_settingsCat >= 0) drawCategoryDetail();
            else { _gridSelected = -1; drawCategoryGrid(); }
        }
    } else if (_settingsCat == 6) {
        // Debug virtual sub-экран
        int n = catItemCount(6);
        if (btn & BTN_UP)   { if(_catItemIdx>0){_catItemIdx--;drawCategoryDetail();} }
        if (btn & BTN_DOWN) { if(_catItemIdx<n-1){_catItemIdx++;drawCategoryDetail();} }
        if (btn & BTN_RIGHT) {
            int gi = globalSettingIdx(6, _catItemIdx);
            settingInc(gi); drawCategoryDetail();
        }
        if (btn & BTN_LEFT) {
            int gi = globalSettingIdx(6, _catItemIdx);
            settingDec(gi); drawCategoryDetail();
        }
        if (btn & BTN_SEL) {  // SELECT = назад
            _settingsCat = (_prevCat >= 0) ? _prevCat : -1;
            _prevCat = -1;
            if (_settingsCat >= 0) drawCategoryDetail();
            else { _gridSelected = -1; drawCategoryGrid(); }
        }
    } else {
        int n = catItemCount(_settingsCat);
        if (btn & BTN_UP)   { if(_catItemIdx>0){_catItemIdx--;drawCategoryDetail();} }
        if (btn & BTN_DOWN) { if(_catItemIdx<n-1){_catItemIdx++;drawCategoryDetail();} }
        if (btn & BTN_RIGHT) {
            int gi = globalSettingIdx(_settingsCat, _catItemIdx);
            if (gi == 12) return 0x40;  // open remap signal
            if (gi == 24) return 0x80;  // open WiFi
            if (gi == 26) return 0xA0;  // OTA check
            if (gi == 32) return 0xB0;  // open Game Genie codes screen
            if (gi == 36) return 0xD0;  // open Game Shark codes screen
            if (gi == 38) return 0xC0;  // Web Debug toggle
            if (gi == 25 || gi == 33 || gi == 37) {
                _prevCat = _settingsCat;
                _settingsCat = (gi == 33) ? 7 : (gi == 37) ? 8 : 6;
                _catItemIdx = 0; _detailOffset = 0; _detailOpenedMs = millis();
                drawCategoryDetail(); return 0;
            }
            settingInc(gi); drawCategoryDetail();
        }
        if (btn & BTN_LEFT) {
            int gi = globalSettingIdx(_settingsCat, _catItemIdx);
            if (gi == 12) return 0x40;  // open remap signal
            if (gi == 24) return 0x80;  // open WiFi
            if (gi == 26) return 0xA0;  // OTA check
            if (gi == 32) return 0xB0;  // open Game Genie
            if (gi == 36) return 0xD0;  // open Game Shark
            if (gi == 38) return 0xC0;  // Web Debug toggle
            if (gi == 25 || gi == 33 || gi == 37) {
                _prevCat = _settingsCat;
                _settingsCat = (gi == 33) ? 7 : (gi == 37) ? 8 : 6;
                _catItemIdx = 0; _detailOffset = 0; _detailOpenedMs = millis();
                drawCategoryDetail(); return 0;
            }
            settingDec(gi); drawCategoryDetail();
        }
        if (btn & BTN_STA) {
            // START = подтвердить / открыть (то же что раньше делал BTN_A)
            int gi = globalSettingIdx(_settingsCat, _catItemIdx);
            if (gi == 12) return 0x40;
            if (gi == 24) return 0x80;
            if (gi == 26) return 0xA0;
            if (gi == 32) return 0xB0;  // open Game Genie
            if (gi == 36) return 0xD0;  // open Game Shark
            if (gi == 38) return 0xC0;  // Web Debug toggle
            if (gi == 25 || gi == 33 || gi == 37) {
                _prevCat = _settingsCat;
                _settingsCat = (gi == 33) ? 7 : (gi == 37) ? 8 : 6;
                _catItemIdx = 0; _detailOffset = 0; _detailOpenedMs = millis();
                drawCategoryDetail(); return 0;
            }
            settingInc(gi); drawCategoryDetail();
        }
        if (btn & BTN_SEL) { _settingsCat = -1; _gridSelected = -1; drawCategoryGrid(); }  // SELECT = назад
    }
    return 0;
}

uint8_t btnMapNavBtn(uint8_t btn) {
    if (btn & BTN_UP)   { if(_rmIdx>0){_rmIdx--;btnMapDraw();} }
    if (btn & BTN_DOWN) { if(_rmIdx<BTN_MAP_COUNT-1){_rmIdx++;btnMapDraw();} }
    if (btn & BTN_RIGHT) {
        int fi=(nesFuncIndex(settings.btnMap[_rmIdx])+1)%_nesFuncCount;
        settings.btnMap[_rmIdx]=_nesFunc[fi].mask; btnMapDraw();
    }
    if (btn & BTN_LEFT) {
        int fi=(nesFuncIndex(settings.btnMap[_rmIdx])-1+_nesFuncCount)%_nesFuncCount;
        settings.btnMap[_rmIdx]=_nesFunc[fi].mask; btnMapDraw();
    }
    if (btn & BTN_SEL) return BTN_B;  // SELECT = назад
    return 0;
}

// ══════════════════════════════════════════════════════════════
// POPUP (generic)
// ══════════════════════════════════════════════════════════════

void popupShow(const char *title, const char *msg, uint32_t timeoutMs) {
    const Theme565 &t = getTheme();
    int pw = 260, ph = 100;
    int px = (SCREEN_W - pw) / 2, py = (SCREEN_H - ph) / 2;
    lcd.fillRoundRect(px+3, py+3, pw, ph, 10, 0x0841);
    lcd.fillRoundRect(px, py, pw, ph, 10, t.header);
    lcd.drawRoundRect(px, py, pw, ph, 10, t.accent);
    lcd.drawFastHLine(px+8, py+30, pw-16, 0x2945);
    fmd(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(t.accent);
    lcd.drawString(title, SCREEN_W/2, py+15);
    fsm(); lcd.setTextColor(t.textPri);
    lcd.drawString(msg, SCREEN_W/2, py+54);
    lcd.setTextColor(t.textSec);
    lcd.drawString("Tap to close", SCREEN_W/2, py+78);
    uint32_t dl = millis() + timeoutMs;
    while (millis() < dl) { if (touch.isTouched()) break; delay(20); }
}

// ══════════════════════════════════════════════════════════════
// WIFI MANAGER SCREEN
// ══════════════════════════════════════════════════════════════

#define WIFI_ROW_H     28
#define WIFI_MAX_ROWS  5      // rows visible (matches the available area)
#define WIFI_AREA_TOP  HDR_H
#define WIFI_AREA_BOT  DPAD_Y

static int  _wifiSel    = 0;
static int  _wifiOffset = 0;
static bool _wifiScanned = false;

// Signal strength bars (0..4 bars)
static int rssiToBars(int rssi) {
    if (rssi >= -50) return 4;
    if (rssi >= -65) return 3;
    if (rssi >= -75) return 2;
    if (rssi >= -85) return 1;
    return 0;
}

static void drawSignalBars(int cx, int cy, int bars, uint16_t c) {
    // 4 bars, each slightly taller
    int heights[4] = {4, 7, 10, 13};
    int bw = 3, gap = 2;
    int totalW = 4*bw + 3*gap;
    int bx = cx - totalW/2;
    for (int i = 0; i < 4; i++) {
        uint16_t col = (i < bars) ? c : (uint16_t)0x2945;
        int h = heights[i];
        lcd.fillRect(bx + i*(bw+gap), cy - h + 4, bw, h, col);
    }
}

void wifiManagerDraw() {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);

    // Header
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    flg(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor((uint16_t)COL_GOLD);
    lcd.drawString("WiFi", SCREEN_W/2, HDR_H/2);
    lcd.drawFastHLine(0, HDR_H-1, SCREEN_W, t.accent);

    // Status chip (top-right)
    fsm(); lcd.setTextDatum(MR_DATUM);
    if (wifiMgr.isConnected()) {
        lcd.setTextColor(t.ok);
        String s = wifiMgr.getSSID();
        if (s.length() > 14) s = s.substring(0, 12) + "..";
        lcd.drawString(s.c_str(), SCREEN_W-6, HDR_H/2);
    } else {
        lcd.setTextColor(t.danger);
        lcd.drawString("Not connected", SCREEN_W-6, HDR_H/2);
    }

    // Network list
    int count = wifiMgr.getScanCount();
    if (!_wifiScanned) {
        fmd(); lcd.setTextDatum(MC_DATUM);
        lcd.setTextColor(t.textSec);
        lcd.drawString("Press Scan to find networks", SCREEN_W/2, (HDR_H + DPAD_Y)/2 - 10);
        fsm(); lcd.setTextColor(t.textSec);
        lcd.drawString("(tap SCAN button below)", SCREEN_W/2, (HDR_H + DPAD_Y)/2 + 14);
    } else if (count == 0) {
        fmd(); lcd.setTextDatum(MC_DATUM);
        lcd.setTextColor(t.textSec);
        lcd.drawString("No networks found", SCREEN_W/2, (HDR_H + DPAD_Y)/2);
    } else {
        // Clamp scroll
        if (_wifiSel < _wifiOffset) _wifiOffset = _wifiSel;
        if (_wifiSel >= _wifiOffset + WIFI_MAX_ROWS) _wifiOffset = _wifiSel - WIFI_MAX_ROWS + 1;
        if (_wifiOffset < 0) _wifiOffset = 0;

        int end = min(_wifiOffset + WIFI_MAX_ROWS, count);
        for (int i = _wifiOffset; i < end; i++) {
            int row = i - _wifiOffset;
            int y   = WIFI_AREA_TOP + row * WIFI_ROW_H;
            bool sel = (i == _wifiSel);

            uint16_t bg = sel ? t.selected : (row%2 ? t.rowOdd : t.rowEven);
            lcd.fillRect(0, y, SCREEN_W, WIFI_ROW_H, bg);
            if (sel) lcd.fillRect(0, y, 3, WIFI_ROW_H, t.accent);

            // Lock icon
            uint16_t txtCol = sel ? t.bg : t.textPri;
            if (wifiMgr.getScanEncrypted(i)) {
                fsm(); lcd.setTextColor(sel ? t.bg : t.textSec);
                lcd.setTextDatum(ML_DATUM);
                lcd.drawString("*", 8, y + WIFI_ROW_H/2);
            }
            // SSID
            fsm(); lcd.setTextColor(txtCol); lcd.setTextDatum(ML_DATUM);
            String ssid = wifiMgr.getScanSSID(i);
            if (ssid.length() > 28) ssid = ssid.substring(0, 26) + "..";
            lcd.drawString(ssid.c_str(), 18, y + WIFI_ROW_H/2);
            // Signal bars
            int bars = rssiToBars(wifiMgr.getScanRSSI(i));
            drawSignalBars(SCREEN_W - 24, y + WIFI_ROW_H/2, bars, sel ? t.bg : t.accent);
        }

        // Scroll indicators
        if (_wifiOffset > 0) {
            int ax = SCREEN_W-14, ay = WIFI_AREA_TOP + 6;
            lcd.fillTriangle(ax, ay+6, ax-5, ay+12, ax+5, ay+12, t.accent);
        }
        if (_wifiOffset + WIFI_MAX_ROWS < count) {
            int ax = SCREEN_W-14, ay = WIFI_AREA_BOT - 14;
            lcd.fillTriangle(ax, ay+6, ax-5, ay, ax+5, ay, t.accent);
        }
    }

    // Bottom bar: [← Back] [SCAN] [CONNECT]
    int by = DPAD_Y;
    lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.header);
    lcd.drawFastHLine(0, by, SCREEN_W, (uint16_t)COL_TOPBAR);
    // BACK (0..106)
    lcd.fillRoundRect(4, by+5, 102, 34, 8, t.rowOdd);
    int ax = 16, ty = by + BTNBAR_H/2;
    lcd.fillTriangle(ax, ty, ax+8, ty-6, ax+8, ty+6, (uint16_t)COL_GOLD);
    fsm(); lcd.setTextColor((uint16_t)COL_WHITE); lcd.setTextDatum(ML_DATUM);
    lcd.drawString(cyrStr(S().back), ax+12, ty);
    // SCAN (107..213)
    lcd.drawFastVLine(107, by+6, BTNBAR_H-12, 0x3186);
    fmd(); lcd.setTextColor(t.textSec); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("SCAN", 160, ty);
    // CONNECT (214..319)
    lcd.drawFastVLine(214, by+6, BTNBAR_H-12, 0x3186);
    bool hasNet = (_wifiScanned && wifiMgr.getScanCount() > 0);
    lcd.setTextColor(hasNet ? (uint16_t)COL_GOLD : t.textSec);
    lcd.drawString("CONNECT", 267, ty);
}

uint8_t wifiManagerHandleTouch(int x, int y) {
    if (y >= DPAD_Y) {
        if (x < 107) return BTN_B;   // Back
        if (x >= 107 && x < 214) {
            // SCAN
            const Theme565 &t = getTheme();
            // Show "Scanning..." message
            lcd.fillRect(0, HDR_H, SCREEN_W, DPAD_Y - HDR_H, t.bg);
            fmd(); lcd.setTextDatum(MC_DATUM);
            lcd.setTextColor(t.textSec);
            lcd.drawString("Scanning...", SCREEN_W/2, (HDR_H + DPAD_Y)/2);
            _wifiScanned = true;
            _wifiSel = 0; _wifiOffset = 0;
            wifiMgr.scan();
            wifiManagerDraw();
        } else if (x >= 214) {
            // CONNECT
            if (_wifiScanned && wifiMgr.getScanCount() > 0)
                return BTN_A;   // signal to main: open keyboard
        }
        return 0;
    }
    if (y < HDR_H) return 0;

    // List tap
    int count = wifiMgr.getScanCount();
    if (_wifiScanned && count > 0) {
        int row = (y - WIFI_AREA_TOP) / WIFI_ROW_H;
        int idx = _wifiOffset + row;
        if (idx >= 0 && idx < count) {
            if (idx != _wifiSel) {
                _wifiSel = idx;
                wifiManagerDraw();
            } else {
                return BTN_A;   // double tap same row → connect
            }
        }
    }
    return 0;
}

uint8_t wifiManagerNavBtn(uint8_t btn) {
    int count = wifiMgr.getScanCount();
    if (btn & BTN_UP)   { if(_wifiSel>0){_wifiSel--;wifiManagerDraw();} }
    if (btn & BTN_DOWN) { if(_wifiSel<count-1){_wifiSel++;wifiManagerDraw();} }
    if (btn & BTN_STA)  return BTN_A;  // START = подключить
    if (btn & BTN_SEL)  return BTN_B;  // SELECT = назад
    return 0;
}

// Get selected SSID from the scan list
const char *wifiManagerSelectedSSID() {
    static String _s;
    _s = wifiMgr.getScanSSID(_wifiSel);
    return _s.c_str();
}

// ══════════════════════════════════════════════════════════════
// WIFI PASSWORD KEYBOARD
// ══════════════════════════════════════════════════════════════

static char  _kbPassword[65] = {};
static int   _kbLen      = 0;
static int   _kbCursorPos = 0;   // позиция курсора внутри строки (0.._kbLen)
static bool  _kbCaps     = false;
static bool  _kbSymMode  = false;
static char  _kbSSID[64] = {};
static char  _kbLabel[48] = {};   // override top label (empty = use "Network:")
static bool  _kbMask     = true;  // true = show dots (password), false = show chars

static const char *_kbAlpha[3] = {
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM"
};
static const char *_kbSym[3] = {
    "1234567890",
    "!@#$%^&*()",
    "-_=+[];',"
};

static const int KB_HDR_H  = 40;
static const int KB_INP_Y  = 40;
static const int KB_INP_H  = 30;
static const int KB_ROW_Y  = 74;   // keyboard starts here
static const int KB_ROW_H  = 34;
static const int KB_KEY_W  = 32;

// ── Physical-button cursor for keyboard ───────────────────────────────────────
// Row 0: 10 alpha/num keys  (cols 0..9)
// Row 1:  9 alpha/num keys  (cols 0..8)
// Row 2:  9 positions       col 0=Shift, 1-7=chars, 8=Backspace
// Row 3:  3 positions       col 0=SYM/123, 1=SPACE, 2=OK
static int _kbCurRow = 0;
static int _kbCurCol = 0;

static int kbRowMaxCol(int row) {
    switch (row) {
        case 0: return 9;
        case 1: return 8;
        case 2: return 8;
        case 3: return 2;
        default: return 0;
    }
}

static void kbClampCol() {
    int mx = kbRowMaxCol(_kbCurRow);
    if (_kbCurCol > mx) _kbCurCol = mx;
    if (_kbCurCol < 0)  _kbCurCol = 0;
}

// Activate the key currently under the cursor (called on BTN_A press)
// Returns BTN_A if the "OK" key was activated, 0 otherwise.
static uint8_t kbActivateCurrent() {
    const char **rows = _kbSymMode ? _kbSym : _kbAlpha;
    auto addChar = [&](char c) {
        if (_kbLen < 63) { _kbPassword[_kbLen++] = c; _kbPassword[_kbLen] = '\0'; }
    };
    if (_kbCurRow == 0) {
        char c = rows[0][_kbCurCol];
        if (!_kbSymMode) c = _kbCaps ? toupper(c) : tolower(c);
        addChar(c);
    } else if (_kbCurRow == 1) {
        char c = rows[1][_kbCurCol];
        if (!_kbSymMode) c = _kbCaps ? toupper(c) : tolower(c);
        addChar(c);
    } else if (_kbCurRow == 2) {
        if (_kbCurCol == 0) {
            if (_kbSymMode) _kbSymMode = false;
            else            _kbCaps = !_kbCaps;
        } else if (_kbCurCol == 8) {
            if (_kbLen > 0) { _kbPassword[--_kbLen] = '\0'; }
        } else {
            int ki = _kbCurCol - 1;  // maps col 1-7 → chars index 0-6
            char c = rows[2][ki];
            if (!_kbSymMode) c = _kbCaps ? toupper(c) : tolower(c);
            addChar(c);
        }
    } else if (_kbCurRow == 3) {
        if (_kbCurCol == 0) {
            _kbSymMode = !_kbSymMode;
            _kbCaps = false;
        } else if (_kbCurCol == 1) {
            addChar(' ');
        } else if (_kbCurCol == 2) {
            return BTN_A;  // OK button
        }
    }
    return 0;
}

// Draw a single key
static void drawKey(int x, int y, int w, int h, const char *label, uint16_t bg, uint16_t fg) {
    lcd.fillRoundRect(x+1, y+1, w-2, h-2, 4, bg);
    lcd.drawRoundRect(x, y, w, h, 4, 0x2945);
    fsm(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(fg);
    lcd.drawString(label, x + w/2, y + h/2);
}

void wifiKeyboardDraw(const char *ssid) {
    strncpy(_kbSSID, ssid, 63); _kbSSID[63] = '\0';

    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);

    // Header
    lcd.fillRect(0, 0, SCREEN_W, KB_HDR_H, t.header);
    fsm(); lcd.setTextDatum(ML_DATUM);
    lcd.setTextColor(t.textSec);
    // Use custom label if set, otherwise default "Network:"
    const char *lbl1 = (_kbLabel[0] != '\0') ? _kbLabel : "Network:";
    lcd.drawString(lbl1, 6, KB_HDR_H/2 - 8);
    fmd(); lcd.setTextColor((uint16_t)COL_GOLD);
    String sn = String(ssid); if(sn.length()>22) sn=sn.substring(0,20)+"..";
    lcd.drawString(sn.c_str(), 6, KB_HDR_H/2 + 8);

    // Input field
    lcd.fillRoundRect(4, KB_INP_Y+2, SCREEN_W-8, KB_INP_H-4, 5, t.rowOdd);
    lcd.drawRoundRect(4, KB_INP_Y+2, SCREEN_W-8, KB_INP_H-4, 5, t.accent);
    // Show text: masked (dots) for passwords, plain for rename
    String dots = "";
    if (_kbMask) {
        for (int i = 0; i < _kbLen; i++) dots += (i < _kbLen-1) ? '*' : _kbPassword[i];
        dots += "|";  // cursor always at end in mask mode
    } else {
        for (int i = 0; i < _kbLen; i++) {
            if (i == _kbCursorPos) dots += '|';
            dots += _kbPassword[i];
        }
        if (_kbCursorPos >= _kbLen) dots += '|';
    }
    fsm(); lcd.setTextDatum(ML_DATUM);
    lcd.setTextColor(t.textPri);
    lcd.drawString(dots.c_str(), 10, KB_INP_Y + KB_INP_H/2);

    // Keyboard rows — cursor position (_kbCurRow/_kbCurCol) is highlighted with accent bg
    const char **rows = _kbSymMode ? _kbSym : _kbAlpha;

    // Helper: returns accent bg if this is the cursor key, else normal bg
    auto keybg = [&](int row, int col, uint16_t normal) -> uint16_t {
        return (_kbCurRow == row && _kbCurCol == col) ? t.accent : normal;
    };

    // Row 0 — 10 alpha/num keys (10 × 32 px)
    for (int i = 0; i < 10; i++) {
        char ch[3] = {0};
        ch[0] = _kbSymMode ? rows[0][i]
                           : (_kbCaps ? toupper(rows[0][i]) : tolower(rows[0][i]));
        drawKey(i * KB_KEY_W, KB_ROW_Y, KB_KEY_W, KB_ROW_H,
                ch, keybg(0, i, t.header), (uint16_t)COL_WHITE);
    }

    // Row 1 — 9 keys, centred (offset 16 px)
    int r2off = 16;
    for (int i = 0; i < 9; i++) {
        char ch[3] = {0};
        ch[0] = _kbSymMode ? rows[1][i]
                           : (_kbCaps ? toupper(rows[1][i]) : tolower(rows[1][i]));
        drawKey(r2off + i * KB_KEY_W, KB_ROW_Y + KB_ROW_H, KB_KEY_W, KB_ROW_H,
                ch, keybg(1, i, t.header), (uint16_t)COL_WHITE);
    }

    // Row 2 — [Shift/ABC](44px) + 7 keys + [⌫](44px)
    {
        int shiftW = 44, backW = 44;
        int r3keysW = SCREEN_W - shiftW - backW;
        int r3keyW  = r3keysW / 7;
        int r3start = shiftW;
        // Shift key: col 0
        uint16_t shiftBg = keybg(2, 0, _kbCaps ? t.accent : t.rowOdd);
        drawKey(0, KB_ROW_Y + KB_ROW_H*2, shiftW, KB_ROW_H,
                _kbSymMode ? "ABC" : (_kbCaps ? "abc" : "ABC"),
                shiftBg, (uint16_t)COL_WHITE);
        // 7 character keys: cols 1-7
        for (int i = 0; i < 7; i++) {
            char ch[3] = {0};
            ch[0] = _kbSymMode ? rows[2][i]
                               : (_kbCaps ? toupper(rows[2][i]) : tolower(rows[2][i]));
            drawKey(r3start + i * r3keyW, KB_ROW_Y + KB_ROW_H*2, r3keyW, KB_ROW_H,
                    ch, keybg(2, i + 1, t.header), (uint16_t)COL_WHITE);
        }
        // Backspace: col 8
        drawKey(SCREEN_W - backW, KB_ROW_Y + KB_ROW_H*2, backW, KB_ROW_H,
                "<", keybg(2, 8, t.rowOdd), (uint16_t)COL_GOLD);
    }

    // Row 3 — [SYM/123](64px) [SPACE](190px) [OK](64px)
    {
        int r4y = KB_ROW_Y + KB_ROW_H * 3;
        drawKey(0,   r4y, 64,  KB_ROW_H,
                _kbSymMode ? "ABC" : "123", keybg(3, 0, t.rowOdd), (uint16_t)COL_WHITE);
        drawKey(65,  r4y, 190, KB_ROW_H,
                "SPACE", keybg(3, 1, t.header), (uint16_t)COL_WHITE);
        drawKey(256, r4y, 64,  KB_ROW_H,
                "OK", keybg(3, 2, t.accent), (uint16_t)COL_WHITE);
    }
}

uint8_t wifiKeyboardHandleTouch(int x, int y) {
    // Tap on input field — move text cursor (only in non-mask mode)
    if (y >= KB_INP_Y && y < KB_INP_Y + KB_INP_H && !_kbMask) {
        fmd();
        int tapX = x - 10;
        String built = "";
        int bestPos = _kbLen;
        for (int i = 0; i < _kbLen; i++) {
            int w = lcd.textWidth(built.c_str());
            if (tapX <= w) { bestPos = i; break; }
            built += _kbPassword[i];
        }
        _kbCursorPos = constrain(bestPos, 0, _kbLen);
        wifiKeyboardDraw(_kbSSID);
        return 0;
    }

    auto addChar = [&](char c) {
        if (_kbLen < 63) {
            memmove(_kbPassword + _kbCursorPos + 1,
                    _kbPassword + _kbCursorPos,
                    _kbLen - _kbCursorPos + 1);
            _kbPassword[_kbCursorPos] = c;
            _kbLen++;
            _kbCursorPos++;
        }
        wifiKeyboardDraw(_kbSSID);
    };
    auto delChar = [&]() {
        if (_kbCursorPos > 0) {
            memmove(_kbPassword + _kbCursorPos - 1,
                    _kbPassword + _kbCursorPos,
                    _kbLen - _kbCursorPos + 1);
            _kbLen--;
            _kbCursorPos--;
        }
        wifiKeyboardDraw(_kbSSID);
    };

    const char **rows = _kbSymMode ? _kbSym : _kbAlpha;

    if (y >= KB_ROW_Y && y < KB_ROW_Y + KB_ROW_H) {
        // Row 1 — 10 keys
        int ki = x / KB_KEY_W;
        if (ki >= 0 && ki < 10) {
            char c = rows[0][ki];
            if (!_kbSymMode) c = _kbCaps ? toupper(c) : tolower(c);
            addChar(c);
        }
    } else if (y >= KB_ROW_Y + KB_ROW_H && y < KB_ROW_Y + KB_ROW_H*2) {
        // Row 2 — 9 keys, offset 16px
        int ki = (x - 16) / KB_KEY_W;
        if (ki >= 0 && ki < 9) {
            char c = rows[1][ki];
            if (!_kbSymMode) c = _kbCaps ? toupper(c) : tolower(c);
            addChar(c);
        }
    } else if (y >= KB_ROW_Y + KB_ROW_H*2 && y < KB_ROW_Y + KB_ROW_H*3) {
        // Row 3 — Shift | 7 keys | Backspace
        int shiftW = 44, backW = 44;
        int r3keysW = SCREEN_W - shiftW - backW;
        int r3keyW  = r3keysW / 7;
        if (x < shiftW) {
            // Toggle caps / sym-mode if sym active toggle back
            if (_kbSymMode) { _kbSymMode = false; }
            else             { _kbCaps = !_kbCaps; }
            wifiKeyboardDraw(_kbSSID);
        } else if (x >= SCREEN_W - backW) {
            delChar();
        } else {
            int ki = (x - shiftW) / r3keyW;
            if (ki >= 0 && ki < 7) {
                char c = rows[2][ki];
                if (!_kbSymMode) c = _kbCaps ? toupper(c) : tolower(c);
                addChar(c);
            }
        }
    } else if (y >= KB_ROW_Y + KB_ROW_H*3 && y < KB_ROW_Y + KB_ROW_H*4) {
        // Row 4 — 123/ABC | SPACE | OK
        if (x < 64) {
            _kbSymMode = !_kbSymMode;
            _kbCaps = false;
            wifiKeyboardDraw(_kbSSID);
        } else if (x < 256) {
            addChar(' ');
        } else {
            return BTN_A;   // OK
        }
    }
    return 0;
}

uint8_t wifiKeyboardNavBtn(uint8_t btn) {
    // D-pad: move cursor
    if (btn & BTN_UP) {
        if (_kbCurRow > 0) { _kbCurRow--; kbClampCol(); }
        wifiKeyboardDraw(_kbSSID); return 0;
    }
    if (btn & BTN_DOWN) {
        if (_kbCurRow < 3) { _kbCurRow++; kbClampCol(); }
        wifiKeyboardDraw(_kbSSID); return 0;
    }
    if (btn & BTN_LEFT) {
        int mx = kbRowMaxCol(_kbCurRow);
        _kbCurCol = (_kbCurCol > 0) ? _kbCurCol - 1 : mx;  // wrap left→end
        wifiKeyboardDraw(_kbSSID); return 0;
    }
    if (btn & BTN_RIGHT) {
        int mx = kbRowMaxCol(_kbCurRow);
        _kbCurCol = (_kbCurCol < mx) ? _kbCurCol + 1 : 0;  // wrap end→left
        wifiKeyboardDraw(_kbSSID); return 0;
    }
    // A / STA: activate current key
    if (btn & (BTN_A | BTN_STA)) {
        uint8_t r = kbActivateCurrent();
        wifiKeyboardDraw(_kbSSID);
        return r;  // BTN_A if OK was activated, else 0
    }
    // B: backspace if text exists, cancel if empty
    if (btn & BTN_B) {
        if (_kbLen > 0) {
            _kbPassword[--_kbLen] = '\0';
            wifiKeyboardDraw(_kbSSID);
            return 0;
        }
        return BTN_B;  // nothing to delete → cancel
    }
    // SEL: cancel
    if (btn & BTN_SEL) return BTN_B;
    return 0;
}

const char *wifiKeyboardGetPassword() {
    return _kbPassword;
}

// Clear keyboard state (call before opening keyboard)
void wifiKeyboardReset() {
    _kbPassword[0] = '\0'; _kbLen = 0; _kbCursorPos = 0;
    _kbCaps = false; _kbSymMode = false;
    _kbLabel[0] = '\0';   // reset to default "Network:"
    _kbMask = true;       // reset to password mode
    _kbCurRow = 0;        // cursor top-left
    _kbCurCol = 0;
}

// Override the top label line (e.g. "Rename:" for file manager)
void wifiKeyboardSetLabel(const char *label) {
    strncpy(_kbLabel, label, sizeof(_kbLabel) - 1);
    _kbLabel[sizeof(_kbLabel) - 1] = '\0';
}

// Pre-fill input buffer (e.g. current ROM name for rename)
void wifiKeyboardSetInitial(const char *text) {
    strncpy(_kbPassword, text, sizeof(_kbPassword) - 1);
    _kbPassword[sizeof(_kbPassword) - 1] = '\0';
    _kbLen = (int)strlen(_kbPassword);
    _kbCursorPos = _kbLen;  // курсор в конец при предзаполнении
}

// Control whether input is masked (true=dots, false=plain)
void wifiKeyboardSetMask(bool mask) {
    _kbMask = mask;
}

// ── Кнопка "← НАЗАД" для экрана поиска (рисуется под клавиатурой) ───────────
// Клавиатура занимает y=0..209; свободная полоса y=210..239 (30px)
// Тач-зона: x=0..119, y=210..239
void menuDrawSearchKbBack() {
    const Theme565& t = getTheme();
    const int BY = KB_ROW_Y + KB_ROW_H * 4;  // 74 + 136 = 210
    const int BH = SCREEN_H - BY;             // 30px
    lcd.fillRect(0, BY, SCREEN_W, BH, t.bg);
    lcd.fillRoundRect(4, BY + 3, 112, BH - 6, 5, t.rowOdd);
    lcd.drawRoundRect(4, BY + 3, 112, BH - 6, 5, t.accent);
    fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textPri);
    lcd.drawString("<- BACK", 60, BY + BH / 2);
}

// ══════════════════════════════════════════════════════════════
// OTA SCREEN
// ══════════════════════════════════════════════════════════════

// ── Вспомогательная: рисует шапку экрана обновления ─────────────────────────
static void _otaDrawHeader(const char *title) {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    flg(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor((uint16_t)COL_GOLD);
    lcd.drawString(title, SCREEN_W/2, HDR_H/2);
    lcd.drawFastHLine(0, HDR_H-1, SCREEN_W, t.accent);
}

// ── Вспомогательная: рисует кнопки Cancel(Skip)/Action ──────────────────────
// Возвращает true если нажали Action (правая кнопка), false — Cancel/таймаут
static bool _otaAskUser(const char *cancelLabel, const char *actionLabel,
                        uint32_t timeoutMs = 20000) {
    const Theme565 &t = getTheme();
    int by = DPAD_Y;
    lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.header);
    lcd.drawFastHLine(0, by, SCREEN_W, (uint16_t)COL_TOPBAR);
    int ty = by + BTNBAR_H/2;
    lcd.fillRoundRect(4, by+5, 148, 34, 8, t.rowOdd);
    fmd(); lcd.setTextColor(t.textSec); lcd.setTextDatum(MC_DATUM);
    lcd.drawString(cancelLabel, 80, ty);
    lcd.fillRoundRect(156, by+5, 160, 34, 8, t.accent);
    lcd.setTextColor(t.bg);
    lcd.drawString(actionLabel, 237, ty);

    uint32_t deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        if (!touch.isTouched()) { delay(20); continue; }
        int tx, ty2; touch.getXY(tx, ty2);
        if (ty2 >= by) return (tx >= 156);
        delay(20);
    }
    return false;   // timeout → cancel
}

// ══════════════════════════════════════════════════════════════
// SD-OTA: обновление прошивки из файла на SD карте
// ══════════════════════════════════════════════════════════════

// Сканируем /update/*.bin → макс 12 файлов
struct _SdFwFile { char name[72]; size_t size; };
static _SdFwFile _sdFwList[12];
static int       _sdFwCount = 0;

static void _sdFwScan() {
    _sdFwCount = 0;
    File dir = SD.open("/update");
    if (!dir || !dir.isDirectory()) return;
    while (_sdFwCount < 12) {
        File f = dir.openNextFile();
        if (!f) break;
        if (f.isDirectory()) { f.close(); continue; }
        const char *n = f.name();
        int len = strlen(n);
        if (len < 4) { f.close(); continue; }
        // Проверяем расширение .bin
        if (n[len-4]!='.' || (n[len-3]!='b'&&n[len-3]!='B')) { f.close(); continue; }
        // basename
        const char *sl = strrchr(n, '/');
        const char *base = sl ? sl + 1 : n;
        strncpy(_sdFwList[_sdFwCount].name, base, 71);
        _sdFwList[_sdFwCount].name[71] = '\0';
        _sdFwList[_sdFwCount].size = f.size();
        _sdFwCount++;
        f.close();
    }
    dir.close();
}

// Цифровая клавиатура — тач.  Возвращает true если введён правильный PIN.
// Рисует поверх текущего экрана своё окно.
static bool _sdNumpad(const char *correctPin) {
    const Theme565 &t = getTheme();

    // Полноэкранное окно с небольшим отступом
    const int WX = 5, WY = 4, WW = 310, WH = 232;
    lcd.fillRoundRect(WX, WY, WW, WH, 8, t.bg);
    lcd.drawRoundRect(WX, WY, WW, WH, 8, t.accent);

    lcd.setTextDatum(MC_DATUM);
    fmd(); lcd.setTextColor((uint16_t)COL_GOLD);
    lcd.drawString("Enter Password", WX + WW/2, WY + 16);

    // 4 ячейки для ввода
    char entered[5] = {};
    int  elen = 0;
    const int CY = WY + 38;
    const int CW = 54, CH = 24, CGap = 6;
    const int cx0 = WX + WW/2 - (CW*4 + CGap*3)/2;

    auto drawCells = [&]() {
        for (int i = 0; i < 4; i++) {
            int bx = cx0 + i*(CW+CGap);
            lcd.fillRoundRect(bx, CY, CW, CH, 4, t.rowEven);
            lcd.drawRoundRect(bx, CY, CW, CH, 4, i < elen ? t.accent : t.rowOdd);
            lcd.setTextDatum(MC_DATUM);
            fmd(); lcd.setTextColor(t.textPri);
            if (i < elen) lcd.drawString("*", bx + CW/2, CY + CH/2);
        }
    };
    drawCells();

    // Кнопки 1-9, 0, ←, OK — 4 строки × 3 кол. (умещаются в 240px)
    const char *keys[] = { "1","2","3","4","5","6","7","8","9","←","0","OK" };
    const int KW = 93, KH = 27, KGX = 5, KGY = 4;
    const int KX0 = WX + 10, KY0 = WY + 76;

    auto drawKeys = [&]() {
        for (int i = 0; i < 12; i++) {
            int col = i % 3, row = i / 3;
            int bx = KX0 + col*(KW+KGX);
            int by = KY0 + row*(KH+KGY);
            bool isOk  = (i == 11);
            bool isDel = (i == 9);
            uint16_t bg = isOk ? t.accent : (isDel ? t.danger : t.rowEven);
            uint16_t fg = isOk ? t.bg : (isDel ? (uint16_t)0xFFFF : t.textPri);
            lcd.fillRoundRect(bx, by, KW, KH, 6, bg);
            lcd.drawRoundRect(bx, by, KW, KH, 6, t.rowOdd);
            lcd.setTextDatum(MC_DATUM);
            fmd(); lcd.setTextColor(fg);
            lcd.drawString(keys[i], bx + KW/2, by + KH/2);
        }
    };
    drawKeys();

    // Цикл ввода
    bool lastTouched = false;
    while (true) {
        bool nowTouched = touch.isTouched();
        if (nowTouched && !lastTouched) {
            int tx, ty; touch.getXY(tx, ty);
            for (int i = 0; i < 12; i++) {
                int col = i % 3, row = i / 3;
                int bx = KX0 + col*(KW+KGX);
                int by = KY0 + row*(KH+KGY);
                if (tx >= bx && tx < bx+KW && ty >= by && ty < by+KH) {
                    if (i == 9) { // ←
                        if (elen > 0) { elen--; entered[elen] = '\0'; }
                    } else if (i == 11) { // OK
                        if (elen == (int)strlen(correctPin)) {
                            return strncmp(entered, correctPin, 5) == 0;
                        }
                    } else {
                        int digit = (i == 10) ? 0 : (i + 1);
                        if (i >= 1 && i <= 8) digit = i + 1;
                        if (i == 0) digit = 1;
                        // Recalculate: keys[0]="1"..keys[8]="9", keys[10]="0"
                        if (i <= 8) digit = i + 1;
                        else digit = 0;
                        if (elen < 4) { entered[elen] = '0' + digit; elen++; entered[elen] = '\0'; }
                    }
                    drawCells();
                    break;
                }
            }
        }
        lastTouched = nowTouched;
        delay(16);

        // Физическая кнопка B = отмена
        buttons.update();
        if (buttons.readCurrent() & BTN_B) return false;
    }
}

// Прошивка ESP32 из файла на SD карте
static void _sdFlashFirmware(const char *filename) {
    const Theme565 &t = getTheme();
    char path[80];
    snprintf(path, sizeof(path), "/update/%s", filename);

    File f = SD.open(path);
    if (!f) { popupShow("SD Update", "Cannot open file!", 3000); return; }
    size_t fsize = f.size();
    if (fsize < 1024) { f.close(); popupShow("SD Update", "File too small!", 3000); return; }

    _otaDrawHeader("SD Update");
    fmd(); lcd.setTextColor(t.textPri); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("Flashing firmware...", SCREEN_W/2, 80);
    fsm(); lcd.setTextColor(t.textSec);
    lcd.drawString("Do NOT power off!", SCREEN_W/2, 100);
    char buf[64]; snprintf(buf, sizeof(buf), "%s (%.0f KB)", filename, fsize/1024.0f);
    lcd.drawString(buf, SCREEN_W/2, 116);

    int barX = 20, barY = 140, barW = SCREEN_W-40, barH = 20;
    lcd.drawRoundRect(barX, barY, barW, barH, 4, t.accent);

    if (!Update.begin(fsize, U_FLASH)) {
        f.close();
        popupShow("SD Update", "Not enough OTA space!", 4000);
        return;
    }

    uint8_t chunkBuf[512];
    size_t written = 0;
    while (written < fsize) {
        int toRead = min((size_t)512, fsize - written);
        int n = f.read(chunkBuf, toRead);
        if (n <= 0) break;
        Update.write(chunkBuf, n);
        written += n;
        int pct = (int)(written * 100 / fsize);
        int filled = (barW - 2) * pct / 100;
        lcd.fillRoundRect(barX+1, barY+1, filled > 1 ? filled : 1, barH-2, 3, t.accent);
        char pctStr[8]; snprintf(pctStr, sizeof(pctStr), "%d%%", pct);
        lcd.fillRect(barX, barY+barH+4, barW, 16, t.bg);
        fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textSec);
        lcd.drawString(pctStr, SCREEN_W/2, barY+barH+12);
    }
    f.close();

    if (Update.end(true) && !Update.hasError()) {
        fmd(); lcd.setTextColor(t.ok); lcd.setTextDatum(MC_DATUM);
        lcd.drawString("Done! Rebooting...", SCREEN_W/2, barY+barH+34);
        delay(2000);
        ESP.restart();
    } else {
        char errBuf[48]; snprintf(errBuf, sizeof(errBuf), "Error: %d", Update.getError());
        popupShow("SD Update", errBuf, 5000);
    }
}

// ── SD Update экран: выбор прошивки из /update/ ──────────────────────────────
void sdOtaScreen() {
    const Theme565 &t = getTheme();
    _sdFwScan();

    _otaDrawHeader("SD Update");
    fsm(); lcd.setTextDatum(MC_DATUM);

    if (_sdFwCount == 0) {
        lcd.setTextColor(t.textSec);
        lcd.drawString("No .bin files in /update/", SCREEN_W/2, 110);
        fsm(); lcd.setTextColor(t.textSec);
        lcd.drawString("Upload via Web Manager first", SCREEN_W/2, 130);
        // footer
        int by = DPAD_Y;
        lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.header);
        lcd.drawFastHLine(0, by, SCREEN_W, (uint16_t)COL_TOPBAR);
        lcd.setTextColor(t.textSec); lcd.setTextDatum(MC_DATUM);
        lcd.drawString("Reset = Back", SCREEN_W/2, by + BTNBAR_H/2);
        // ждём Reset (BTN_B)
        bool prev = false;
        while (true) {
            buttons.update();
            bool now = (buttons.readCurrent() & BTN_B) != 0;
            if (now && !prev) return;
            prev = now; delay(20);
        }
    }

    // Рисуем список файлов
    const int FW_ROW_H = 28;
    const int FW_ROWS  = (DPAD_Y - HDR_H) / FW_ROW_H;
    int sel = 0, off = 0;

    auto drawList = [&]() {
        lcd.fillRect(0, HDR_H, SCREEN_W, DPAD_Y - HDR_H, t.bg);
        for (int i = 0; i < FW_ROWS && (off + i) < _sdFwCount; i++) {
            int idx = off + i;
            int ry  = HDR_H + i * FW_ROW_H;
            bool s  = (idx == sel);
            lcd.fillRect(0, ry, SCREEN_W, FW_ROW_H - 1, s ? t.hilite : (i%2 ? t.rowOdd : t.rowEven));
            if (s) lcd.drawFastHLine(0, ry, SCREEN_W, t.accent);
            // имя
            lcd.setTextDatum(ML_DATUM);
            fsm(); lcd.setTextColor(s ? t.selText : t.textPri);
            lcd.drawString(_sdFwList[idx].name, 8, ry + FW_ROW_H/2);
            // размер
            char sz[16]; snprintf(sz, sizeof(sz), "%.0f KB", _sdFwList[idx].size/1024.0f);
            lcd.setTextDatum(MR_DATUM);
            fsm(); lcd.setTextColor(s ? t.selText : t.textSec);
            lcd.drawString(sz, SCREEN_W - 6, ry + FW_ROW_H/2);
        }
        // footer
        int by = DPAD_Y;
        lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.header);
        lcd.drawFastHLine(0, by, SCREEN_W, (uint16_t)COL_TOPBAR);
        lcd.setTextDatum(ML_DATUM); fsm(); lcd.setTextColor(t.textSec);
        lcd.drawString("Reset=Back", 8, by + BTNBAR_H/2);
        lcd.setTextDatum(MR_DATUM);
        lcd.drawString("A=Flash", SCREEN_W - 8, by + BTNBAR_H/2);
    };
    drawList();

    uint8_t prevPad = 0xFF;
    while (true) {
        buttons.update();
        uint8_t cur    = buttons.readCurrent();
        uint8_t newBtn = cur & ~prevPad;
        prevPad        = cur;

        if (newBtn & BTN_B) return;
        if (newBtn & BTN_UP) {
            if (sel > 0) { sel--; if (sel < off) off = sel; drawList(); }
        }
        if (newBtn & BTN_DOWN) {
            if (sel < _sdFwCount - 1) { sel++; if (sel >= off + FW_ROWS) off = sel - FW_ROWS + 1; drawList(); }
        }
        if (newBtn & BTN_A) {
            // Подтверждение
            _otaDrawHeader("SD Update");
            fmd(); lcd.setTextColor((uint16_t)COL_GOLD); lcd.setTextDatum(MC_DATUM);
            lcd.drawString("Flash firmware?", SCREEN_W/2, 78);
            fsm(); lcd.setTextColor(t.textSec);
            lcd.drawString(_sdFwList[sel].name, SCREEN_W/2, 100);
            fmd(); lcd.setTextColor(t.danger);
            lcd.drawString("ESP32 will reboot!", SCREEN_W/2, 125);
            if (!_otaAskUser("Cancel", "Flash", 20000)) { drawList(); continue; }

            // Пароль
            _otaDrawHeader("SD Update");
            if (!_sdNumpad("1234")) {
                popupShow("SD Update", "Wrong password!", 2000);
                _otaDrawHeader("SD Update");
                drawList();
                continue;
            }

            // Прошиваем
            _sdFlashFirmware(_sdFwList[sel].name);
            // Если сюда дошли — ошибка; перерисовываем список
            _otaDrawHeader("SD Update");
            drawList();
        }
        delay(16);
    }
}

// ── Выбор источника обновления: GitHub (WEB) или SD карта (MANAGER) ──────────
static bool _otaSourceChoice() {
    const Theme565 &t = getTheme();
    _otaDrawHeader("Check Update");

    fmd(); lcd.setTextColor(t.textPri); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("Update source:", SCREEN_W/2, 72);

    // Кнопка WEB (левая)
    lcd.fillRoundRect(16, 95, 130, 60, 10, t.rowEven);
    lcd.drawRoundRect(16, 95, 130, 60, 10, t.accent);
    fmd(); lcd.setTextColor(t.accent); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("WEB", 81, 118);
    fsm(); lcd.setTextColor(t.textSec);
    lcd.drawString("GitHub", 81, 138);

    // Кнопка MANAGER (правая)
    lcd.fillRoundRect(174, 95, 130, 60, 10, t.rowEven);
    lcd.drawRoundRect(174, 95, 130, 60, 10, t.accent);
    fmd(); lcd.setTextColor(t.accent); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("MANAGER", 239, 118);
    fsm(); lcd.setTextColor(t.textSec);
    lcd.drawString("SD card /update/", 239, 138);

    // footer
    int by = DPAD_Y;
    lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.header);
    lcd.drawFastHLine(0, by, SCREEN_W, (uint16_t)COL_TOPBAR);
    fsm(); lcd.setTextColor(t.textSec); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("Reset = Cancel", SCREEN_W/2, by + BTNBAR_H/2);

    // Текущий выбор кнопками: 0=WEB, 1=MANAGER
    int kbSel = -1;  // -1 = ничего не выбрано кнопками
    auto highlightKb = [&](int s) {
        // Перерисовываем рамки кнопок
        lcd.drawRoundRect(16,  95, 130, 60, 10, s==0 ? (uint16_t)0xFFFF : t.accent);
        lcd.drawRoundRect(17,  96, 128, 58, 9,  s==0 ? (uint16_t)0xFFFF : t.rowEven);
        lcd.drawRoundRect(174, 95, 130, 60, 10, s==1 ? (uint16_t)0xFFFF : t.accent);
        lcd.drawRoundRect(175, 96, 128, 58, 9,  s==1 ? (uint16_t)0xFFFF : t.rowEven);
    };

    bool lastTouch = false;
    uint8_t prevBtn = 0xFF;
    while (true) {
        buttons.update();
        uint8_t cur    = buttons.readCurrent();
        uint8_t newBtn = cur & ~prevBtn;
        prevBtn        = cur;

        // Reset = отмена
        if (newBtn & BTN_B) return false;

        // LEFT/RIGHT = выбор кнопками
        if (newBtn & BTN_LEFT)  { kbSel = 0; highlightKb(0); }
        if (newBtn & BTN_RIGHT) { kbSel = 1; highlightKb(1); }

        // A = подтвердить текущий выбор кнопкой
        if ((newBtn & BTN_A) && kbSel >= 0) {
            if (kbSel == 0) return true;
            sdOtaScreen();
            return false;
        }

        bool nowTouch = touch.isTouched();
        if (nowTouch && !lastTouch) {
            int tx, ty; touch.getXY(tx, ty);
            if (ty >= 95 && ty <= 155) {
                if (tx >= 16  && tx <= 146) return true;        // WEB
                if (tx >= 174 && tx <= 304) {                    // MANAGER
                    sdOtaScreen();
                    return false;
                }
            }
        }
        lastTouch = nowTouch;
        delay(16);
    }
}

void otaScreen() {
    const Theme565 &t = getTheme();

    // ── Выбор: WEB или MANAGER ───────────────────────────────────────────────
    // false = отмена или пользователь выбрал MANAGER (sdOtaScreen уже отработал)
    // true  = пользователь выбрал WEB → продолжаем GitHub OTA ниже
    if (!_otaSourceChoice()) return;

    // Авто-подключение WiFi если выключен (WiFi выключается после NTP на старте)
    bool _otaConnectedHere = false;
    if (!wifiMgr.isConnected()) {
        if (!settings.wifiEnabled || settings.wifiSSID[0] == '\0') {
            popupShow("Update", "No WiFi configured!", 3000);
            return;
        }
        _otaDrawHeader("Update");
        fmd(); lcd.setTextColor(getTheme().textSec); lcd.setTextDatum(MC_DATUM);
        lcd.drawString("Connecting to WiFi...", SCREEN_W/2, 120);
        WiFi.mode(WIFI_STA);
        if (!wifiMgr.connect(settings.wifiSSID, settings.wifiPass, 12000)) {
            popupShow("Update", "WiFi connection failed!", 3000);
            WiFi.mode(WIFI_OFF);
            return;
        }
        _otaConnectedHere = true;
    }

    // ── Проверяем обновления ──────────────────────────────────────────────────
    _otaDrawHeader("Update");
    fmd(); lcd.setTextColor(t.textSec); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("Checking for update...", SCREEN_W/2, 110);
    lcd.fillCircle(SCREEN_W/2, 150, 16, t.accent);
    fsm(); lcd.setTextColor(t.bg);
    lcd.drawString("...", SCREEN_W/2, 150);

    OTAInfo info;
    if (!otaCheckUpdate(info)) {
        popupShow("Update", "Check failed. No internet?", 4000);
        return;
    }

    bool anyAction = false;

    // ════════════════════════════════════════════════════════════════════════
    // ШАГ 1: Обновление ESP32 (если доступно)
    // ════════════════════════════════════════════════════════════════════════
    if (info.available) {
        anyAction = true;
        _otaDrawHeader("Update");

        fmd(); lcd.setTextColor((uint16_t)COL_GOLD); lcd.setTextDatum(MC_DATUM);
        lcd.drawString("ESP32 update available!", SCREEN_W/2, 72);
        fsm(); lcd.setTextColor(t.textSec);
        lcd.drawString(String("Current: ") + FIRMWARE_VERSION, SCREEN_W/2, 95);
        lcd.drawString(String("Latest:  v") + info.latestVersion, SCREEN_W/2, 110);
        fmd(); lcd.setTextColor(t.textPri);
        lcd.drawString("Install ESP32 firmware?", SCREEN_W/2, 140);
        fsm(); lcd.setTextColor(t.textSec);
        lcd.drawString("ESP32 reboots after install", SCREEN_W/2, 158);
        if (info.picoAvailable) {
            lcd.setTextColor(t.ok);
            lcd.drawString("Pico firmware also in this release", SCREEN_W/2, 175);
        }

        if (_otaAskUser("Skip", "Install", 30000)) {
            // Показываем прогресс
            lcd.fillRect(0, HDR_H, SCREEN_W, DPAD_Y - HDR_H, t.bg);
            fmd(); lcd.setTextColor(t.textPri); lcd.setTextDatum(MC_DATUM);
            lcd.drawString("Downloading ESP32...", SCREEN_W/2, 90);
            int barX = 20, barY = 120, barW = SCREEN_W-40, barH = 18;
            lcd.drawRoundRect(barX, barY, barW, barH, 4, t.accent);

            auto progressCb = [](int pct) {
                const Theme565 &t2 = getTheme();
                int bx=20, by2=120, bw=SCREEN_W-40, bh=18;
                int filled = (bw-2) * pct / 100;
                lcd.fillRoundRect(bx+1, by2+1, filled > 1 ? filled : 1, bh-2, 3, t2.accent);
                char s[8]; snprintf(s, sizeof(s), "%d%%", pct);
                lcd.fillRect(bx, by2+bh+4, bw, 16, t2.bg);
                fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t2.textSec);
                lcd.drawString(s, SCREEN_W/2, by2+bh+12);
            };

            otaDownloadAndFlash(info.downloadUrl.c_str(), progressCb);
            // При успехе ESP32 перезагрузится — сюда не дойдёт
            popupShow("Update", "ESP32 flash failed!", 5000);
            return;
        }
        // Пользователь нажал Skip → переходим к Pico (если есть)
    }

    // ════════════════════════════════════════════════════════════════════════
    // ШАГ 2: Обновление Pico (если доступно)
    // ════════════════════════════════════════════════════════════════════════
    if (info.picoAvailable) {
        anyAction = true;
        _otaDrawHeader("Update");

        fmd(); lcd.setTextColor((uint16_t)COL_GOLD); lcd.setTextDatum(MC_DATUM);
        lcd.drawString("Pico firmware available!", SCREEN_W/2, 72);
        fsm(); lcd.setTextColor(t.textSec);
        int picoVer = getCachedPicoVer();
        if (picoVer > 0) {
            char buf[40];
            snprintf(buf, sizeof(buf), "Current Pico: v%d.%d",
                     picoVer/100, picoVer%100);
            lcd.drawString(buf, SCREEN_W/2, 95);
        } else {
            lcd.drawString("Current Pico: unknown", SCREEN_W/2, 95);
        }
        lcd.drawString("New firmware found in this release", SCREEN_W/2, 112);
        fmd(); lcd.setTextColor(t.textPri);
        lcd.drawString("Flash Pico controller?", SCREEN_W/2, 145);
        fsm(); lcd.setTextColor(t.textSec);
        lcd.drawString("Buttons off ~30 sec during flash", SCREEN_W/2, 163);

        if (_otaAskUser("Skip", "Flash Pico", 20000)) {
            picoOtaScreen(info.picoUrl.c_str());
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // Ничего не было доступно
    // ════════════════════════════════════════════════════════════════════════
    if (!anyAction) {
        _otaDrawHeader("Update");
        fmd(); lcd.setTextColor(t.ok); lcd.setTextDatum(MC_DATUM);
        lcd.drawString("Everything is up to date!", SCREEN_W/2, 110);
        fsm(); lcd.setTextColor(t.textSec);
        lcd.drawString(FIRMWARE_VERSION, SCREEN_W/2, 135);
        lcd.drawString("No Pico firmware in this release", SCREEN_W/2, 153);
        delay(3000);
    }

    // Выключаем WiFi если мы его включили здесь (OTA закончен, WiFi больше не нужен)
    if (_otaConnectedHere) {
        WiFi.mode(WIFI_OFF);
        Serial.println("[WiFi] Off after OTA check.");
    }
}

// ── Pico OTA screen ───────────────────────────────────────────────────────────
// Прошивает Pico из pico_firmware.bin в GitHub релизе (picoUrl).
// Версии ESP32 и Pico независимы — сравнивать их между собой не нужно.
// Единственный сигнал "есть обновление для Pico" = наличие pico_firmware.bin
// в данном релизе (проверяется в otaCheckUpdate → info.picoAvailable).
void picoOtaScreen(const char *picoUrl) {
    const Theme565 &t = getTheme();

    if (!wifiMgr.isConnected()) {
        popupShow("Pico Update", "Connect WiFi first!", 3000);
        return;
    }

    // ── Читаємо поточну версію Pico (тільки для відображення) ────────────────
    lcd.fillScreen(t.bg);
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    flg(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor((uint16_t)COL_GOLD);
    lcd.drawString("Pico Update", SCREEN_W/2, HDR_H/2);
    lcd.drawFastHLine(0, HDR_H-1, SCREEN_W, t.accent);
    fmd(); lcd.setTextColor(t.textSec); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("Reading Pico version...", SCREEN_W/2, 110);

    // Запрашиваем версию Pico — только для отображения, не для сравнения
    int picoVer = picoOtaGetVersion();

    // ── Экран подтверждения ───────────────────────────────────────────────────
    lcd.fillRect(0, HDR_H, SCREEN_W, DPAD_Y - HDR_H, t.bg);

    fmd(); lcd.setTextColor((uint16_t)COL_GOLD); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("Pico firmware update", SCREEN_W/2, 78);

    fsm(); lcd.setTextColor(t.textSec);
    if (picoVer > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Pico current version: v%d.%d",
                 picoVer / 100, picoVer % 100);
        lcd.drawString(buf, SCREEN_W/2, 102);
    } else {
        lcd.setTextColor(t.danger);
        lcd.drawString("Pico not responding (old fw?)", SCREEN_W/2, 102);
        lcd.setTextColor(t.textSec);
    }
    lcd.drawString("New firmware found in this release.", SCREEN_W/2, 120);

    fmd(); lcd.setTextColor(t.textPri);
    lcd.drawString("Flash Pico now?", SCREEN_W/2, 150);
    fsm(); lcd.setTextColor(t.textSec);
    lcd.drawString("Buttons off ~30 sec during flash", SCREEN_W/2, 168);

    // Кнопки Cancel / Flash — та же механика что и _otaAskUser, без дублирования
    if (!_otaAskUser("Cancel", "Flash Pico", 20000)) return;

    // ── Прогрес бар ──────────────────────────────────────────────────────────
    lcd.fillRect(0, HDR_H, SCREEN_W, DPAD_Y - HDR_H, t.bg);
    fmd(); lcd.setTextColor(t.textPri); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("Flashing Pico...", SCREEN_W/2, 85);
    fsm(); lcd.setTextColor(t.textSec);
    lcd.drawString("Do not power off!", SCREEN_W/2, 108);
    int barX = 20, barY = 128, barW = SCREEN_W-40, barH = 18;
    lcd.drawRoundRect(barX, barY, barW, barH, 4, t.accent);

    auto progressCb = [](int pct) {
        const Theme565 &t2 = getTheme();
        int bx = 20, by2 = 128, bw = SCREEN_W-40, bh = 18;
        int filled = (bw - 2) * pct / 100;
        lcd.fillRoundRect(bx+1, by2+1, filled > 1 ? filled : 1, bh-2, 3, t2.accent);
        char s[8]; snprintf(s, sizeof(s), "%d%%", pct);
        lcd.fillRect(bx, by2+bh+4, bw, 16, t2.bg);
        fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t2.textSec);
        lcd.drawString(s, SCREEN_W/2, by2+bh+12);
    };

    // picoUrl — прямой CDN-адрес из GitHub API (info.picoUrl), не нужно строить вручную
    bool ok = picoOtaUpdate(picoUrl, progressCb);

    if (ok) {
        // Обновляем кеш версии Pico после успешной прошивки
        delay(500);
        _cachedPicoVer  = picoOtaGetVersion();
        _picoVerCacheMs = millis();
        popupShow("Pico Update", "Done! Pico rebooted.", 4000);
    } else {
        popupShow("Pico Update", "Failed! Check Serial log.", 5000);
    }
}

// ══════════════════════════════════════════════════════════════
// FILE MANAGER
// ══════════════════════════════════════════════════════════════
// Access via BTN_LEFT in main menu.
// Shows ROM list with rename and delete actions.
// ══════════════════════════════════════════════════════════════

#define FM_ROW_H   32
#define FM_ROWS    ((DPAD_Y - HDR_H) / FM_ROW_H)   // 4 visible rows

static int _fmSel    = 0;
static int _fmOffset = 0;

// ── Bottom bar: [← Back] [✏ RENAME] [🗑 DELETE] ──────────────
static void drawFileMgrBar() {
    const Theme565 &t = getTheme();
    int by = DPAD_Y;
    lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.header);
    lcd.drawFastHLine(0, by, SCREEN_W, (uint16_t)COL_TOPBAR);
    int ty = by + BTNBAR_H/2;

    // Zone 1: BACK (0..106)
    lcd.fillRoundRect(4, by+5, 102, 34, 8, t.rowOdd);
    int ax = 16;
    lcd.fillTriangle(ax, ty, ax+8, ty-6, ax+8, ty+6, (uint16_t)COL_GOLD);
    fsm(); lcd.setTextColor((uint16_t)COL_WHITE); lcd.setTextDatum(ML_DATUM);
    lcd.drawString(cyrStr(S().back), ax+12, ty);

    // Dividers
    lcd.drawFastVLine(107, by+6, BTNBAR_H-12, 0x3186);
    lcd.drawFastVLine(214, by+6, BTNBAR_H-12, 0x3186);

    bool hasRoms = (sdMgr.count() > 0);

    // Zone 2: RENAME (107..213)
    fmd(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(hasRoms ? (uint16_t)COL_GOLD : t.textSec);
    lcd.drawString("RENAME", 160, ty);

    // Zone 3: DELETE (214..319)
    lcd.setTextColor(hasRoms ? t.danger : t.textSec);
    lcd.drawString("DELETE", 267, ty);
}

// ── One ROM row ─────────────────────────────────────────────────
static void drawFileMgrRow(int romIdx, int slot) {
    const Theme565 &t = getTheme();
    int y   = HDR_H + slot * FM_ROW_H;
    bool sel = (romIdx == _fmSel);

    uint16_t bg = sel ? t.selected : (slot % 2 ? t.rowOdd : t.rowEven);
    lcd.fillRect(0, y, SCREEN_W, FM_ROW_H, bg);
    if (sel) lcd.fillRect(0, y, 3, FM_ROW_H, t.accent);

    if (slot > 0) lcd.drawFastHLine(6, y, SCREEN_W-12, (uint16_t)COL_SEP);

    const ROMInfo &rom = sdMgr.get(romIdx);
    // Name (left, truncated)
    fmd(); lcd.setTextDatum(ML_DATUM);
    lcd.setTextColor(sel ? (uint16_t)COL_WHITE : t.textPri);
    String name = rom.name;
    if (name.length() > 22) name = name.substring(0, 20) + "..";
    lcd.drawString(name.c_str(), 10, y + FM_ROW_H/2);

    // Size (right)
    fsm(); lcd.setTextDatum(MR_DATUM);
    lcd.setTextColor(sel ? (uint16_t)COL_GOLD : t.textSec);
    String sz = (rom.size < 1024) ? String(rom.size) + "B"
                                  : String(rom.size / 1024) + "KB";
    lcd.drawString(sz.c_str(), SCREEN_W - 8, y + FM_ROW_H/2);
}

// ── Scroll indicators ───────────────────────────────────────────
static void drawFileMgrScrollIndicators() {
    const Theme565 &t = getTheme();
    int total = sdMgr.count();
    if (_fmOffset > 0) {
        int ax = SCREEN_W - 14, ay = HDR_H + 6;
        lcd.fillTriangle(ax, ay+6, ax-5, ay+12, ax+5, ay+12, t.accent);
    }
    if (_fmOffset + FM_ROWS < total) {
        int ax = SCREEN_W - 14, ay = DPAD_Y - 14;
        lcd.fillTriangle(ax, ay+6, ax-5, ay, ax+5, ay, t.accent);
    }
}

void fileMgrDraw() {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);

    // Header
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    flg(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor((uint16_t)COL_GOLD);
    lcd.drawString("FILE MANAGER", SCREEN_W/2, HDR_H/2);
    lcd.drawFastHLine(0, HDR_H-1, SCREEN_W, t.accent);

    int total = sdMgr.count();
    if (total == 0) {
        fmd(); lcd.setTextColor(t.textSec); lcd.setTextDatum(MC_DATUM);
        int cy = HDR_H + (DPAD_Y - HDR_H)/2;
        lcd.drawString("No ROMs found", SCREEN_W/2, cy - 10);
        fsm(); lcd.drawString("Copy .nes files to SD card", SCREEN_W/2, cy + 14);
    } else {
        // Clamp scroll
        if (_fmSel < _fmOffset) _fmOffset = _fmSel;
        if (_fmSel >= _fmOffset + FM_ROWS) _fmOffset = _fmSel - FM_ROWS + 1;
        if (_fmOffset < 0) _fmOffset = 0;

        int end = min(_fmOffset + FM_ROWS, total);
        for (int i = _fmOffset; i < end; i++)
            drawFileMgrRow(i, i - _fmOffset);
        drawFileMgrScrollIndicators();

        // ROM count badge
        char badge[16];
        snprintf(badge, sizeof(badge), "%d files", total);
        fsm(); lcd.setTextColor(t.textSec); lcd.setTextDatum(MR_DATUM);
        lcd.drawString(badge, SCREEN_W - 8, HDR_H/2);
    }

    drawFileMgrBar();
}

// ── Delete confirm popup ────────────────────────────────────────
// Returns true if user confirmed, false if cancelled.
static bool fileMgrConfirmDelete(const String &name) {
    const Theme565 &t = getTheme();
    int pw = 270, ph = 110;
    int px = (SCREEN_W - pw)/2, py = (SCREEN_H - ph)/2;
    lcd.fillRoundRect(px+3, py+3, pw, ph, 10, 0x0841);
    lcd.fillRoundRect(px, py, pw, ph, 10, t.header);
    lcd.drawRoundRect(px, py, pw, ph, 10, t.danger);
    lcd.drawFastHLine(px+8, py+32, pw-16, 0x2945);

    fmd(); lcd.setTextColor(t.danger); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("Delete?", SCREEN_W/2, py+16);
    fsm(); lcd.setTextColor(t.textPri);
    String n = name; if (n.length() > 28) n = n.substring(0, 26) + "..";
    lcd.drawString(n.c_str(), SCREEN_W/2, py+52);

    // Buttons: [Cancel] [Delete]
    int bby = py + ph - 38;
    lcd.fillRoundRect(px+8,   bby, 110, 28, 6, t.rowOdd);
    lcd.fillRoundRect(px+152, bby, 110, 28, 6, t.danger);
    fsm(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(t.textSec);
    lcd.drawString("Cancel", px + 63, bby + 14);
    lcd.setTextColor(t.bg);
    lcd.drawString("DELETE", px + 207, bby + 14);

    delay(300);
    uint32_t until = millis() + 8000;
    while (millis() < until) {
        if (touch.isTouched()) {
            int tx, ty; touch.getXY(tx, ty);
            // Delete button zone (right half of popup)
            if (ty >= bby && ty < bby + 28) {
                return (tx >= px + 152);
            }
        }
        delay(20);
    }
    return false;  // timeout → cancel
}

int fileMgrSelected() { return _fmSel; }

uint8_t fileMgrHandleTouch(int x, int y) {
    if (y >= DPAD_Y) {
        // Bottom bar
        if (x < 107) return BTN_B;   // Back

        if (sdMgr.count() == 0) return 0;

        if (x >= 107 && x < 214) {
            // RENAME — signal to main to open keyboard
            return BTN_A;
        }
        if (x >= 214) {
            // DELETE — confirm then remove
            const String &name = sdMgr.get(_fmSel).name;
            if (fileMgrConfirmDelete(name)) {
                sdMgr.removeROM(_fmSel);
                // Clamp selection after deletion
                if (_fmSel >= sdMgr.count() && _fmSel > 0) _fmSel--;
                if (_fmOffset > _fmSel) _fmOffset = _fmSel;
                fileMgrDraw();
                return 0xFF;  // 0xFF = list changed, stay in file manager
            }
            // Cancelled — just redraw
            fileMgrDraw();
        }
        return 0;
    }

    if (y < HDR_H || sdMgr.count() == 0) return 0;

    // Row tap
    int row = (y - HDR_H) / FM_ROW_H;
    int idx  = _fmOffset + row;
    if (idx >= 0 && idx < sdMgr.count()) {
        _fmSel = idx;
        fileMgrDraw();
    }
    return 0;
}

uint8_t fileMgrNavBtn(uint8_t btn) {
    int total = sdMgr.count();
    if (btn & BTN_UP)   { if (_fmSel > 0)        { _fmSel--; fileMgrDraw(); } }
    if (btn & BTN_DOWN) { if (_fmSel < total - 1) { _fmSel++; fileMgrDraw(); } }
    if (btn & BTN_SEL)  return BTN_B;   // SELECT = back to menu
    if (btn & BTN_STA && total > 0)  return BTN_A;   // START = rename
    if (btn & BTN_RIGHT && total > 0) {
        // RIGHT = delete (with confirm)
        const String &name = sdMgr.get(_fmSel).name;
        if (fileMgrConfirmDelete(name)) {
            sdMgr.removeROM(_fmSel);
            if (_fmSel >= sdMgr.count() && _fmSel > 0) _fmSel--;
            if (_fmOffset > _fmSel) _fmOffset = _fmSel;
            fileMgrDraw();
            return 0xFF;  // stay in file manager
        }
        fileMgrDraw();
    }
    return 0;
}

// ══════════════════════════════════════════════════════════════════════════════
// GAME GENIE CODES SCREEN
// ══════════════════════════════════════════════════════════════════════════════
// Layout (320×240):
//   Header   y=0..39    (40px)  — заголовок + B кнопка
//   Toggle   y=40..63   (24px)  — "Enabled: ON/OFF"
//   Slots    y=64..215  (19px × 8 = 152px)  — 8 слотов кодов
//   Hint     y=216..239 (24px)  — подсказки кнопок
//
// Навигация:
//   UP/DOWN  — переключение выбранного слота (0=toggle, 1-8=коды)
//   RIGHT/A  — редактировать выбранный слот (возвращает 0xC0+слот)
//   LEFT/B   — очистить выбранный слот / вернуться (если toggle — возврат)
//   SELECT   — вернуться в настройки
//   START    — переключить ggEnabled

#define GG_HDR_H    40
#define GG_TOG_Y    40
#define GG_TOG_H    24
#define GG_SLOT_Y   64
#define GG_SLOT_H   19
#define GG_HINT_Y   216

static int _ggSel = 0;   // 0=toggle, 1-8=слот кода

static void _ggDrawRow(int idx, bool selected) {
    const Theme565 &t = getTheme();
    int y = (idx == 0) ? GG_TOG_Y : GG_SLOT_Y + (idx - 1) * GG_SLOT_H;
    int h = (idx == 0) ? GG_TOG_H : GG_SLOT_H;

    uint16_t bg  = selected ? t.selected : (idx % 2 ? t.rowOdd : t.rowEven);
    uint16_t fg  = selected ? t.selText  : t.textPri;
    lcd.fillRect(0, y, SCREEN_W, h, bg);

    lcd.setTextDatum(ML_DATUM);
    lcd.setFont(&lgfx::fonts::DejaVu9);
    lcd.setTextColor(fg);

    char buf[64];
    if (idx == 0) {
        // Строка включения
        snprintf(buf, sizeof(buf), "Enabled: %s", settings.ggEnabled ? "ON" : "OFF");
        lcd.drawString(buf, 8, y + h / 2);
        // Цветной индикатор справа
        uint16_t dotCol = settings.ggEnabled ? t.ok : t.danger;
        lcd.fillCircle(SCREEN_W - 12, y + h / 2, 5, dotCol);
    } else {
        int slot = idx - 1;
        const char *code = settings.ggCodes[slot];
        if (code[0]) {
            snprintf(buf, sizeof(buf), "%d: %s", idx, code);
            lcd.setTextColor(fg);
        } else {
            snprintf(buf, sizeof(buf), "%d: ---", idx);
            lcd.setTextColor(selected ? t.selText : t.textSec);
        }
        lcd.drawString(buf, 8, y + h / 2);
    }
}

void ggScreenDraw() {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);

    // ── Заголовок ──────────────────────────────────────────────────────────
    lcd.fillRect(0, 0, SCREEN_W, GG_HDR_H, t.header);
    lcd.setTextDatum(ML_DATUM);
    lcd.setFont(&lgfx::fonts::DejaVu12);
    lcd.setTextColor(t.textPri);
    lcd.drawString("Game Genie Codes", 10, GG_HDR_H / 2);
    // B = назад
    lcd.setTextDatum(MR_DATUM);
    lcd.setTextColor(t.accent);
    lcd.drawString("[B:Back]", SCREEN_W - 6, GG_HDR_H / 2);

    // ── Toggle + слоты ────────────────────────────────────────────────────
    for (int i = 0; i <= 8; i++) _ggDrawRow(i, i == _ggSel);

    // ── Подсказка ──────────────────────────────────────────────────────────
    lcd.fillRect(0, GG_HINT_Y, SCREEN_W, SCREEN_H - GG_HINT_Y, t.header);
    lcd.setFont(&lgfx::fonts::DejaVu9);
    lcd.setTextColor(t.textSec);
    lcd.setTextDatum(MC_DATUM);
    lcd.drawString("A:Edit  B:Clear  START:Enable/Disable", SCREEN_W / 2, GG_HINT_Y + 12);
}

// Возвращаемые коды:
//   BTN_B     — вернуться в настройки
//   0xC1..C8  — открыть клавиатуру для слота (0xC0 + slot_number)
//   0          — просто перерисовано
uint8_t ggScreenNavBtn(uint8_t btn) {
    if (btn & BTN_UP) {
        if (_ggSel > 0) { _ggSel--; ggScreenDraw(); }
        return 0;
    }
    if (btn & BTN_DOWN) {
        if (_ggSel < 8) { _ggSel++; ggScreenDraw(); }
        return 0;
    }
    if (btn & BTN_SEL) return BTN_B;  // SELECT = назад

    if (btn & BTN_STA) {
        // START = переключить ggEnabled
        settings.ggEnabled = !settings.ggEnabled;
        _ggDrawRow(0, _ggSel == 0);
        return 0;
    }

    if (btn & (BTN_A | BTN_RIGHT)) {
        if (_ggSel == 0) {
            // На toggle — переключить enabled
            settings.ggEnabled = !settings.ggEnabled;
            _ggDrawRow(0, true);
            return 0;
        }
        // Открыть клавиатуру для слота _ggSel (1..8)
        return (uint8_t)(0xC0 + _ggSel);
    }

    if (btn & BTN_B) {
        if (_ggSel == 0) return BTN_B;  // на toggle → выход
        // Очистить выбранный слот
        settings.ggCodes[_ggSel - 1][0] = '\0';
        _ggDrawRow(_ggSel, true);
        return 0;
    }

    return 0;
}

uint8_t ggScreenHandleTouch(int x, int y) {
    // Тап по заголовку — назад
    if (y < GG_HDR_H) return BTN_B;

    // Тап по строке toggle
    if (y >= GG_TOG_Y && y < GG_TOG_Y + GG_TOG_H) {
        _ggSel = 0;
        settings.ggEnabled = !settings.ggEnabled;
        ggScreenDraw();
        return 0;
    }

    // Тап по слоту
    if (y >= GG_SLOT_Y && y < GG_SLOT_Y + GG_SLOT_H * 8) {
        int slot = (y - GG_SLOT_Y) / GG_SLOT_H + 1;  // 1..8
        if (slot != _ggSel) {
            _ggSel = slot;
            ggScreenDraw();
            return 0;  // первый тап = выбор
        }
        // Второй тап = редактировать
        return (uint8_t)(0xC0 + slot);
    }

    return 0;
}

void ggScreenSetSlot(int slot) {
    if (slot >= 1 && slot <= 8) _ggSel = slot;
}

int ggScreenGetSlot() { return _ggSel; }

// Сохраняет код в текущий выбранный слот (_ggSel, 1..8).
// Вызывается из main.cpp после валидации через gg_decode().
void ggCodeSaveToSlot(const char *code) {
    if (_ggSel < 1 || _ggSel > 8) return;
    strncpy(settings.ggCodes[_ggSel - 1], code, 7);
    settings.ggCodes[_ggSel - 1][7] = '\0';
}

// ══════════════════════════════════════════════════════════════════════════════
// GAME SHARK CODES SCREEN
// ══════════════════════════════════════════════════════════════════════════════
// Коды пишутся как 6-символьный hex: "AAAAVV" (адрес 4 + значение 2).
// Экран идентичен GG по структуре, но хранит settings.gsCodes / gsEnabled.
// Отображаются как AAAA:VV для читаемости.

#define GS_HDR_H    GG_HDR_H
#define GS_TOG_Y    GG_TOG_Y
#define GS_TOG_H    GG_TOG_H
#define GS_SLOT_Y   GG_SLOT_Y
#define GS_SLOT_H   GG_SLOT_H
#define GS_HINT_Y   GG_HINT_Y

static int _gsSel = 0;

static void _gsDrawRow(int idx, bool selected) {
    const Theme565 &t = getTheme();
    int y = (idx == 0) ? GS_TOG_Y : GS_SLOT_Y + (idx - 1) * GS_SLOT_H;
    int h = (idx == 0) ? GS_TOG_H : GS_SLOT_H;

    uint16_t bg = selected ? t.selected : (idx % 2 ? t.rowOdd : t.rowEven);
    uint16_t fg = selected ? t.selText  : t.textPri;
    lcd.fillRect(0, y, SCREEN_W, h, bg);

    lcd.setTextDatum(ML_DATUM);
    lcd.setFont(&lgfx::fonts::DejaVu9);
    lcd.setTextColor(fg);

    char buf[64];
    if (idx == 0) {
        snprintf(buf, sizeof(buf), "Enabled: %s", settings.gsEnabled ? "ON" : "OFF");
        lcd.drawString(buf, 8, y + h / 2);
        lcd.fillCircle(SCREEN_W - 12, y + h / 2, 5,
                       settings.gsEnabled ? t.ok : t.danger);
    } else {
        int slot = idx - 1;
        const char *code = settings.gsCodes[slot];
        if (code[0]) {
            // Показываем как AAAA:VV
            snprintf(buf, sizeof(buf), "%d: %c%c%c%c:%c%c",
                     idx, code[0], code[1], code[2], code[3], code[4], code[5]);
            lcd.setTextColor(fg);
        } else {
            snprintf(buf, sizeof(buf), "%d: ---", idx);
            lcd.setTextColor(selected ? t.selText : t.textSec);
        }
        lcd.drawString(buf, 8, y + h / 2);
    }
}

void gsScreenDraw() {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);

    lcd.fillRect(0, 0, SCREEN_W, GS_HDR_H, t.header);
    lcd.setTextDatum(ML_DATUM);
    lcd.setFont(&lgfx::fonts::DejaVu12);
    lcd.setTextColor(t.textPri);
    lcd.drawString("Game Shark Codes", 10, GS_HDR_H / 2);
    lcd.setTextDatum(MR_DATUM);
    lcd.setTextColor(t.accent);
    lcd.drawString("[B:Back]", SCREEN_W - 6, GS_HDR_H / 2);

    for (int i = 0; i <= 8; i++) _gsDrawRow(i, i == _gsSel);

    lcd.fillRect(0, GS_HINT_Y, SCREEN_W, SCREEN_H - GS_HINT_Y, t.header);
    lcd.setFont(&lgfx::fonts::DejaVu9);
    lcd.setTextColor(t.textSec);
    lcd.setTextDatum(MC_DATUM);
    lcd.drawString("A:Edit  B:Clear  START:Enable/Disable", SCREEN_W / 2, GS_HINT_Y + 12);
}

uint8_t gsScreenNavBtn(uint8_t btn) {
    if (btn & BTN_UP)   { if (_gsSel > 0) { _gsSel--; gsScreenDraw(); } return 0; }
    if (btn & BTN_DOWN) { if (_gsSel < 8) { _gsSel++; gsScreenDraw(); } return 0; }
    if (btn & BTN_SEL)  return BTN_B;
    if (btn & BTN_STA) {
        settings.gsEnabled = !settings.gsEnabled;
        _gsDrawRow(0, _gsSel == 0);
        return 0;
    }
    if (btn & (BTN_A | BTN_RIGHT)) {
        if (_gsSel == 0) { settings.gsEnabled = !settings.gsEnabled; _gsDrawRow(0, true); return 0; }
        return (uint8_t)(0xC0 + _gsSel);
    }
    if (btn & BTN_B) {
        if (_gsSel == 0) return BTN_B;
        settings.gsCodes[_gsSel - 1][0] = '\0';
        _gsDrawRow(_gsSel, true);
        return 0;
    }
    return 0;
}

uint8_t gsScreenHandleTouch(int x, int y) {
    if (y < GS_HDR_H) return BTN_B;
    if (y >= GS_TOG_Y && y < GS_TOG_Y + GS_TOG_H) {
        _gsSel = 0;
        settings.gsEnabled = !settings.gsEnabled;
        gsScreenDraw();
        return 0;
    }
    if (y >= GS_SLOT_Y && y < GS_SLOT_Y + GS_SLOT_H * 8) {
        int slot = (y - GS_SLOT_Y) / GS_SLOT_H + 1;
        if (slot != _gsSel) { _gsSel = slot; gsScreenDraw(); return 0; }
        return (uint8_t)(0xC0 + slot);
    }
    return 0;
}

void gsScreenSetSlot(int slot) { if (slot >= 1 && slot <= 8) _gsSel = slot; }
int  gsScreenGetSlot() { return _gsSel; }

void gsCodeSaveToSlot(const char *code) {
    if (_gsSel < 1 || _gsSel > 8) return;
    strncpy(settings.gsCodes[_gsSel - 1], code, 6);
    settings.gsCodes[_gsSel - 1][6] = '\0';
}

// ══════════════════════════════════════════════════════════════
// ГАЛЕРЕЯ СКРИНШОТОВ
// ══════════════════════════════════════════════════════════════
// Уровни:  0 = список игр   1 = список скриншотов   2 = полный экран
//
// Файловая схема:
//   /Screenshots/<name>_NNN.raw  — нумерованные снимки
//   /Screenshots/<name>.raw      — обложка (может отсутствовать)
// ──────────────────────────────────────────────────────────────

#define GAL_MAX_GAMES 50
#define GAL_MAX_SHOTS 30
#define GAL_ROWS      7    // строк на экране
#define GAL_ROW_H     M_ROW_H

static int   _galLevel    = 0;

// Уровень 0 — список игр
static char  _galGames[GAL_MAX_GAMES][48];
static int   _galGameCnt  = 0;
static int   _galGameSel  = 0;
static int   _galGameOff  = 0;

// Уровень 1 — список скриншотов выбранной игры
static char  _galShots[GAL_MAX_SHOTS][96];
static int   _galShotCnt  = 0;
static int   _galShotSel  = 0;
static int   _galShotOff  = 0;

// ── Сканирование ──────────────────────────────────────────────────────────────

// Сканирует /Screenshots/ и строит список имён игр, у которых есть <name>_NNN.bmp
static void _galScanGames() {
    _galGameCnt = 0;
    File dir = SD.open("/Screenshots");
    if (!dir) return;
    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            String nm = String(f.name());
            int len = nm.length();
            // Проверяем паттерн: <name>_NNN.bmp  (NNN = ровно 3 цифры)
            if (len >= 9 && nm.endsWith(".bmp")) {
                // Позиция суффикса _NNN.raw: len-8
                if (nm[len-8] == '_' &&
                    isdigit(nm[len-7]) && isdigit(nm[len-6]) && isdigit(nm[len-5])) {
                    String game = nm.substring(0, len - 8);
                    // Дедупликация
                    bool found = false;
                    for (int i = 0; i < _galGameCnt; i++) {
                        if (strcmp(_galGames[i], game.c_str()) == 0) { found = true; break; }
                    }
                    if (!found && _galGameCnt < GAL_MAX_GAMES) {
                        strncpy(_galGames[_galGameCnt], game.c_str(), 47);
                        _galGames[_galGameCnt][47] = '\0';  // guarantee null-termination
                        _galGameCnt++;
                    }
                }
            }
        }
        f = dir.openNextFile();
    }
    dir.close();
    // Сортировка (пузырёк, список маленький)
    for (int i = 0; i < _galGameCnt - 1; i++)
        for (int j = i + 1; j < _galGameCnt; j++)
            if (strcmp(_galGames[i], _galGames[j]) > 0) {
                char tmp[48]; strcpy(tmp, _galGames[i]);
                strcpy(_galGames[i], _galGames[j]); strcpy(_galGames[j], tmp);
            }
}

// Сканирует скриншоты конкретной игры
static void _galScanShots(const char *gameName) {
    _galShotCnt = 0;
    File dir = SD.open("/Screenshots");
    if (!dir) return;
    char prefix[56];
    snprintf(prefix, sizeof(prefix), "%s_", gameName);
    int plen = strlen(prefix);
    File f = dir.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            String nm = String(f.name());
            int len = nm.length();
            if (len >= plen + 7 && nm.endsWith(".bmp")) {
                if (strncmp(nm.c_str(), prefix, plen) == 0) {
                    const char *numPart = nm.c_str() + plen;
                    int numLen = len - plen - 4;  // без ".bmp"
                    if (numLen == 3 && isdigit(numPart[0]) && isdigit(numPart[1]) && isdigit(numPart[2])) {
                        if (_galShotCnt < GAL_MAX_SHOTS) {
                            snprintf(_galShots[_galShotCnt++], 95,
                                     "/Screenshots/%s", nm.c_str());
                        }
                    }
                }
            }
        }
        f = dir.openNextFile();
    }
    dir.close();
    // Сортировка по имени файла = по номеру
    for (int i = 0; i < _galShotCnt - 1; i++)
        for (int j = i + 1; j < _galShotCnt; j++)
            if (strcmp(_galShots[i], _galShots[j]) > 0) {
                char tmp[96]; strcpy(tmp, _galShots[i]);
                strcpy(_galShots[i], _galShots[j]); strcpy(_galShots[j], tmp);
            }
}

// ── Конвертация BMP скриншота в RAW обложку ──────────────────────────────────
// Читает .bmp (24-bit), конвертирует BGR888 → RGB565, пишет .raw (наш формат).
static bool _galSetAsCover(const char *shotPath, const char *gameName) {
    char coverPath[96];
    snprintf(coverPath, sizeof(coverPath), "/Screenshots/%s.raw", gameName);

    File src = SD.open(shotPath, FILE_READ);
    if (!src) return false;
    uint8_t hdr[54] = {};
    if (src.read(hdr, 54) != 54 || hdr[0] != 'B' || hdr[1] != 'M') { src.close(); return false; }
    uint32_t dataOff = (uint32_t)hdr[10] | ((uint32_t)hdr[11]<<8) | ((uint32_t)hdr[12]<<16) | ((uint32_t)hdr[13]<<24);
    uint16_t srcW   = (uint16_t)hdr[18] | ((uint16_t)hdr[19] << 8);
    int32_t  srcHs  = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23]<<8) | ((uint32_t)hdr[24]<<16) | ((uint32_t)hdr[25]<<24));
    uint16_t srcH   = (uint16_t)(srcHs < 0 ? -srcHs : srcHs);
    uint32_t rowBytes = ((uint32_t)srcW * 3 + 3) & ~3u;

    SD.remove(coverPath);
    File dst = SD.open(coverPath, FILE_WRITE);
    if (!dst) { src.close(); return false; }

    uint8_t  ohdr[4] = { (uint8_t)(srcW & 0xFF), (uint8_t)(srcW >> 8),
                         (uint8_t)(srcH & 0xFF), (uint8_t)(srcH >> 8) };
    dst.write(ohdr, 4);

    uint8_t  *rowBuf = (uint8_t  *)malloc(rowBytes);
    uint16_t *pixBuf = (uint16_t *)malloc((size_t)srcW * 2);
    bool ok = (rowBuf && pixBuf);
    if (ok) {
        src.seek(dataOff);
        for (int y = 0; y < (int)srcH; y++) {
            src.read(rowBuf, rowBytes);
            for (int x = 0; x < (int)srcW; x++) {
                uint8_t b = rowBuf[x*3+0], g = rowBuf[x*3+1], r = rowBuf[x*3+2];
                pixBuf[x] = ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
            }
            dst.write((uint8_t *)pixBuf, (size_t)srcW * 2);
        }
    }
    free(rowBuf); free(pixBuf);
    dst.flush(); src.close(); dst.close();
    return ok;
}

// ── Диалог «Установить как обложку?» ─────────────────────────────────────────
static bool _galConfirmCover(const char *gameName) {
    const Theme565& t = getTheme();
    const int PX = 10, PY = 65, PW = 300, PH = 100;
    const int BW = 130, BH = 26, BY = PY + 62;
    lcd.fillRoundRect(PX, PY, PW, PH, 8, t.header);
    lcd.drawRoundRect(PX, PY, PW, PH, 8, t.accent);
    fmd(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.accent);
    lcd.drawString("Set as cover art?", SCREEN_W / 2, PY + 18);
    String gn = String(gameName); if (gn.length() > 26) gn = gn.substring(0, 24) + "..";
    fsm(); lcd.setTextColor(t.textPri);
    lcd.drawString(gn.c_str(), SCREEN_W / 2, PY + 38);
    // sel: 0=YES выбран, 1=NO выбран
    int sel = 0;
    auto drawBtns = [&]() {
        // YES — подсветка если sel==0
        uint16_t yBg = (sel == 0) ? t.accent : t.ok;
        lcd.fillRoundRect(PX + 8,           BY, BW, BH, 6, yBg);
        // NO — подсветка если sel==1
        uint16_t nBg = (sel == 1) ? t.accent : t.selected;
        lcd.fillRoundRect(PX + PW - 8 - BW, BY, BW, BH, 6, nBg);
        fsm(); lcd.setTextColor((uint16_t)COL_WHITE); lcd.setTextDatum(MC_DATUM);
        lcd.drawString("YES - Set cover", PX + 8 + BW / 2,       BY + BH / 2);
        lcd.setTextColor(sel == 1 ? (uint16_t)COL_WHITE : t.textSec);
        lcd.drawString("NO - Cancel",     PX + PW - 8 - BW / 2,  BY + BH / 2);
        // Рамка вокруг активной кнопки
        if (sel == 0) lcd.drawRoundRect(PX + 8,           BY, BW, BH, 6, (uint16_t)COL_WHITE);
        else          lcd.drawRoundRect(PX + PW - 8 - BW, BY, BW, BH, 6, (uint16_t)COL_WHITE);
    };
    drawBtns();
    delay(300);
    while (touch.isTouched()) delay(10);  // ждём отпускания START
    uint32_t until = millis() + 8000;
    while (millis() < until) {
        if (touch.isTouched()) {
            delay(40); int tx = 0, ty = 0; touch.getXY(tx, ty);
            if (ty >= BY && ty < BY + BH) {
                if (tx >= PX + 8 && tx < PX + 8 + BW) return true;
                return false;
            }
            return false;
        }
        buttons.update();
        uint8_t btn = buttons.applyBtnMap(buttons.readNew());
        if (btn & (BTN_LEFT | BTN_UP))  { sel = 0; drawBtns(); until = millis() + 8000; }
        if (btn & (BTN_RIGHT | BTN_DOWN)){ sel = 1; drawBtns(); until = millis() + 8000; }
        if (btn & (BTN_A | BTN_STA))    return (sel == 0);
        if (btn & BTN_B)                return false;
        if (btn & BTN_SEL)              return false;
        delay(20);
    }
    return false;
}

// ── Отрисовка ─────────────────────────────────────────────────────────────────

static void _galDrawHeader(const char *title, int count, bool backArrow) {
    const Theme565& t = getTheme();
    lcd.fillRect(0, 0, SCREEN_W, M_HDR_H, t.header);
    lcd.drawFastHLine(0, M_HDR_H - 1, SCREEN_W, t.accent);
    fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textPri);
    if (backArrow) {
        lcd.setTextDatum(ML_DATUM);
        lcd.drawString("<", 6, M_HDR_H / 2);
        lcd.setTextDatum(MC_DATUM);
    }
    lcd.drawString(title, SCREEN_W / 2, M_HDR_H / 2);
    if (count >= 0) {
        char badge[8]; snprintf(badge, sizeof(badge), "%d", count);
        fsm(); lcd.setTextDatum(MR_DATUM); lcd.setTextColor(t.accent);
        lcd.drawString(badge, SCREEN_W - 8, M_HDR_H / 2);
    }
}

static void _galDrawFooter(const char *leftHint, const char *rightHint) {
    const Theme565& t = getTheme();
    int by = M_DPAD_Y;
    lcd.fillRect(0, by, SCREEN_W, M_BAR_H, t.header);
    lcd.drawFastHLine(0, by, SCREEN_W, t.accent);
    fsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(t.textSec);
    lcd.drawString(leftHint,  8,            by + M_BAR_H / 2);
    lcd.setTextDatum(MR_DATUM);
    lcd.drawString(rightHint, SCREEN_W - 8, by + M_BAR_H / 2);
}

// Рисует одну строку в галерейном списке
static void _galDrawRow(int slot, bool sel, const char *label, const char *badge) {
    const Theme565& t = getTheme();
    int y  = M_HDR_H + slot * GAL_ROW_H;
    int cy = y + GAL_ROW_H / 2;
    uint16_t bg = sel ? t.selected : ((slot % 2) ? t.rowOdd : t.rowEven);
    lcd.fillRect(0, y, M_LIST_W - 1, GAL_ROW_H, bg);
    if (slot < GAL_ROWS - 1)
        lcd.drawFastHLine(0, y + GAL_ROW_H - 1, M_LIST_W - 1, t.header);
    uint16_t tc = sel ? (t.selText ? t.selText : (uint16_t)COL_WHITE) : t.textPri;
    fsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(tc);
    // Метка обрезается; если есть счётчик — оставляем место
    int maxLen = badge ? 14 : 17;
    String lbl_s = String(label);
    if ((int)lbl_s.length() > maxLen) lbl_s = lbl_s.substring(0, maxLen - 2) + "..";
    lcd.drawString(lbl_s.c_str(), 8, cy);
    if (badge) {
        lcd.setTextDatum(MR_DATUM); lcd.setTextColor(sel ? tc : t.textSec);
        lcd.drawString(badge, M_LIST_W - 6, cy);
    }
}

// ── Уровень 0: список игр ────────────────────────────────────────────────────

static void _galDrawGamesLevel() {
    const Theme565& t = getTheme();
    lcd.fillScreen(t.bg);
    _galDrawHeader("GALLERY", _galGameCnt, false);
    // Правая панель — превью выбранной игры
    lcd.fillRect(M_PANEL_X, M_HDR_H, M_PANEL_W, M_DPAD_Y - M_HDR_H, t.bg);
    lcd.drawFastVLine(M_PANEL_X - 1, M_HDR_H, M_DPAD_Y - M_HDR_H, t.accent);
    if (_galGameCnt > 0) {
        char covPath[96];
        snprintf(covPath, sizeof(covPath), "/Screenshots/%s.raw", _galGames[_galGameSel]);
        _drawRawFile(covPath, M_PANEL_X + 2, M_HDR_H + 2, M_PANEL_W - 4, M_COVER_H - 4, false);
        fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textSec);
        lcd.drawString(_galGames[_galGameSel], M_PANEL_X + M_PANEL_W / 2, M_INFO_Y + 12);
    }
    // Список игр
    int visible = min(_galGameCnt - _galGameOff, GAL_ROWS);
    for (int s = 0; s < GAL_ROWS; s++) {
        int idx = _galGameOff + s;
        if (idx >= _galGameCnt) {
            lcd.fillRect(0, M_HDR_H + s * GAL_ROW_H, M_LIST_W - 1, GAL_ROW_H,
                         (s % 2) ? t.rowOdd : t.rowEven);
            continue;
        }
        // Подсчёт снимков для этой игры (кэш не нужен — список маленький)
        char badge[6] = "";
        // Не сканируем здесь — слишком медленно; показываем просто имя
        _galDrawRow(s, idx == _galGameSel, _galGames[idx], nullptr);
    }
    (void)visible;
    _galDrawFooter("B: Back", "A: Open");
}

// ── Уровень 1: список скриншотов ─────────────────────────────────────────────

static void _galDrawShotsLevel() {
    const Theme565& t = getTheme();
    lcd.fillScreen(t.bg);
    // Заголовок с именем игры
    char hdr[52];
    snprintf(hdr, sizeof(hdr), "< %s", _galGames[_galGameSel]);
    _galDrawHeader(hdr, _galShotCnt, true);
    // Разделитель и правая панель
    lcd.drawFastVLine(M_PANEL_X - 1, M_HDR_H, M_DPAD_Y - M_HDR_H, t.accent);
    lcd.fillRect(M_PANEL_X, M_HDR_H, M_PANEL_W, M_DPAD_Y - M_HDR_H, t.bg);
    // Превью выбранного скриншота
    if (_galShotCnt > 0)
        _drawBmpFile(_galShots[_galShotSel],
                     M_PANEL_X + 2, M_HDR_H + 2, M_PANEL_W - 4, M_COVER_H - 4, false);
    // Список скриншотов
    for (int s = 0; s < GAL_ROWS; s++) {
        int idx = _galShotOff + s;
        if (idx >= _galShotCnt) {
            lcd.fillRect(0, M_HDR_H + s * GAL_ROW_H, M_LIST_W - 1, GAL_ROW_H,
                         (s % 2) ? t.rowOdd : t.rowEven);
            continue;
        }
        // Имя из пути: "/Screenshots/Name_001.bmp" → "Shot 001"
        const char *fn = strrchr(_galShots[idx], '/');
        fn = fn ? fn + 1 : _galShots[idx];
        // Берём последние 7 символов файла (NNN.bmp) → номер
        int fnLen = strlen(fn);
        char label[24] = "Shot ???";
        if (fnLen >= 7) snprintf(label, sizeof(label), "Shot %c%c%c", fn[fnLen-7], fn[fnLen-6], fn[fnLen-5]);
        _galDrawRow(s, idx == _galShotSel, label, nullptr);
    }
    _galDrawFooter("B: Back", "A: Fullscreen");
}

// ── Уровень 2: полный экран ───────────────────────────────────────────────────

static void _galDrawFullscreen() {
    if (_galShotCnt == 0) return;
    // Рисуем скриншот на весь экран
    _drawBmpFile(_galShots[_galShotSel], 0, 0, SCREEN_W, SCREEN_H, false);
    // Нижняя панель-подсказка
    const Theme565& t = getTheme();
    lcd.fillRect(0, SCREEN_H - 28, SCREEN_W, 28, 0x0000u);
    lcd.fillRect(0, SCREEN_H - 28, SCREEN_W, 1, t.accent);
    fsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor((uint16_t)COL_GOLD);
    lcd.drawString("STA: Set cover", 8, SCREEN_H - 14);
    lcd.setTextDatum(MR_DATUM); lcd.setTextColor(t.textSec);
    lcd.drawString("B: Back", SCREEN_W - 8, SCREEN_H - 14);
}

// ── Частичное обновление правой панели (уровни 0 и 1) ───────────────────────

static void _galUpdatePanel() {
    const Theme565& t = getTheme();
    lcd.fillRect(M_PANEL_X, M_HDR_H, M_PANEL_W, M_DPAD_Y - M_HDR_H, t.bg);
    if (_galLevel == 0 && _galGameCnt > 0) {
        char covPath[96];
        snprintf(covPath, sizeof(covPath), "/Screenshots/%s.raw", _galGames[_galGameSel]);
        _drawRawFile(covPath, M_PANEL_X + 2, M_HDR_H + 2, M_PANEL_W - 4, M_COVER_H - 4, false);
        fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textSec);
        lcd.drawString(_galGames[_galGameSel], M_PANEL_X + M_PANEL_W / 2, M_INFO_Y + 12);
    } else if (_galLevel == 1 && _galShotCnt > 0) {
        _drawBmpFile(_galShots[_galShotSel],
                     M_PANEL_X + 2, M_HDR_H + 2, M_PANEL_W - 4, M_COVER_H - 4, false);
    }
}

// ── Переключение строки списка с частичной перерисовкой ──────────────────────

static void _galMoveSel(int delta) {
    if (_galLevel == 0) {
        if (_galGameCnt == 0) return;
        int oldSel = _galGameSel;
        _galGameSel = constrain(_galGameSel + delta, 0, _galGameCnt - 1);
        if (_galGameSel == oldSel) return;
        // Прокрутка окна
        if (_galGameSel < _galGameOff) _galGameOff = _galGameSel;
        if (_galGameSel >= _galGameOff + GAL_ROWS) _galGameOff = _galGameSel - GAL_ROWS + 1;
        // Перерисовываем затронутые строки
        int olds = oldSel - _galGameOff, news = _galGameSel - _galGameOff;
        if (olds >= 0 && olds < GAL_ROWS) {
            const char *lbl = _galGames[oldSel];
            _galDrawRow(olds, false, lbl, nullptr);
        }
        if (news >= 0 && news < GAL_ROWS) {
            _galDrawRow(news, true, _galGames[_galGameSel], nullptr);
        }
        _galUpdatePanel();
    } else if (_galLevel == 1) {
        if (_galShotCnt == 0) return;
        int oldSel = _galShotSel;
        _galShotSel = constrain(_galShotSel + delta, 0, _galShotCnt - 1);
        if (_galShotSel == oldSel) return;
        if (_galShotSel < _galShotOff) _galShotOff = _galShotSel;
        if (_galShotSel >= _galShotOff + GAL_ROWS) _galShotOff = _galShotSel - GAL_ROWS + 1;
        int olds = oldSel - _galShotOff, news = _galShotSel - _galShotOff;
        // Имя из пути для старой и новой строки
        auto shotLabel = [](int idx, char *buf, int bufsz) {
            const char *fn = strrchr(_galShots[idx], '/');
            fn = fn ? fn + 1 : _galShots[idx];
            int fnl = strlen(fn);
            if (fnl >= 7) snprintf(buf, bufsz, "Shot %c%c%c", fn[fnl-7], fn[fnl-6], fn[fnl-5]);
            else          snprintf(buf, bufsz, "Shot ???");
        };
        char lbl[24];
        if (olds >= 0 && olds < GAL_ROWS) { shotLabel(oldSel, lbl, sizeof(lbl)); _galDrawRow(olds, false, lbl, nullptr); }
        if (news >= 0 && news < GAL_ROWS) { shotLabel(_galShotSel, lbl, sizeof(lbl)); _galDrawRow(news, true, lbl, nullptr); }
        _galUpdatePanel();
    }
}

// ══════════════════════════════════════════════════════════════
// MUSIC PLAYER
// ══════════════════════════════════════════════════════════════
// MUSIC PLAYER — воспроизведение WAV/MP3 из /Music на SD
// Функции: авто-переход, перемешивание, пред/след, строка "сейчас играет"
// ══════════════════════════════════════════════════════════════
#include "input/audio.h"

#define MP_MAX_FILES  32
#define MP_HDR_H      30
#define MP_NP_H       36    // строка "сейчас играет"
#define MP_BAR_H      40    // подвал
#define MP_ROW_H      26
#define MP_LIST_Y     (MP_HDR_H + MP_NP_H)
#define MP_ROWS       ((SCREEN_H - MP_LIST_Y - MP_BAR_H) / MP_ROW_H)

static char _mpFiles[MP_MAX_FILES][48];
static int  _mpCount   = 0;
static int  _mpSel     = 0;
static int  _mpOffset  = 0;
static int  _mpPlaying = -1;
static bool _mpShuffle = false;

static void _mpScan() {
    _mpCount = 0;
    File dir = SD.open("/Music");
    if (!dir || !dir.isDirectory()) return;
    while (_mpCount < MP_MAX_FILES) {
        File f = dir.openNextFile();
        if (!f) break;
        if (!f.isDirectory()) {
            const char *raw = f.name();
            const char *slash = strrchr(raw, '/');
            const char *name = slash ? slash + 1 : raw;
            int len = strlen(name);
            if (len > 4) {
                char ext[5];
                for (int i = 0; i < 4; i++) ext[i] = tolower((unsigned char)name[len - 4 + i]);
                ext[4] = '\0';
                if (strcmp(ext, ".wav") == 0 || strcmp(ext, ".mp3") == 0) {
                    strncpy(_mpFiles[_mpCount], name, 47);
                    _mpFiles[_mpCount][47] = '\0';
                    _mpCount++;
                }
            }
        }
        f.close();
    }
    dir.close();
}

static void _mpPlaySelected() {
    if (_mpSel < 0 || _mpSel >= _mpCount) return;
    if (audioIsBusy()) soundStop();
    char path[64];
    snprintf(path, sizeof(path), "/Music/%s", _mpFiles[_mpSel]);
    soundPlayPath(path);
    _mpPlaying = _mpSel;
}

// Убираем расширение из имени файла для отображения
static void _mpDispName(const char *src, char *dst, int dstSize) {
    strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
    int len = (int)strlen(dst);
    for (int i = len - 1; i > 0 && i > len - 6; i--) {
        if (dst[i] == '.') { dst[i] = '\0'; break; }
    }
}

static void _mpDrawNowPlaying() {
    const Theme565& t = getTheme();
    bool ru = (settings.language == LANG_RU);
    bool playing = (_mpPlaying >= 0) && audioIsBusy();

    uint16_t barBg = playing ? t.selected : t.rowOdd;
    lcd.fillRect(0, MP_HDR_H, SCREEN_W, MP_NP_H, barBg);
    lcd.drawFastHLine(0, MP_HDR_H, SCREEN_W, t.accent);
    lcd.drawFastHLine(0, MP_HDR_H + MP_NP_H - 1, SCREEN_W, (uint16_t)COL_SEP);

    int cy = MP_HDR_H + MP_NP_H / 2;
    uint16_t fg = playing ? (t.selText ? t.selText : (uint16_t)COL_WHITE) : t.textSec;

    if (_mpCount == 0) {
        fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textSec);
        lcd.drawString(ru ? cyrStr("Нет файлов в /Music") : "No audio files in /Music",
                       SCREEN_W / 2, cy);
        return;
    }

    // Иконка: ▶ при воспроизведении, • при остановке
    if (playing) {
        lcd.fillTriangle(7, cy - 6, 7, cy + 6, 17, cy, t.accent);
    } else {
        lcd.fillCircle(12, cy, 4, t.textSec);
    }

    // Имя трека без расширения
    int showIdx = playing ? _mpPlaying : _mpSel;
    char disp[48];
    _mpDispName(_mpFiles[showIdx], disp, sizeof(disp));
    fsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(fg);
    lcd.drawString(disp, 26, cy);

    // Счётчик X/N справа
    char num[8]; snprintf(num, sizeof(num), "%d/%d", showIdx + 1, _mpCount);
    lcd.setTextDatum(MR_DATUM); lcd.setTextColor(playing ? fg : t.textSec);
    lcd.drawString(num, SCREEN_W - 8, cy);
}

static void _mpDrawList() {
    const Theme565& t = getTheme();
    bool ru = (settings.language == LANG_RU);
    int listH = SCREEN_H - MP_LIST_Y - MP_BAR_H;
    lcd.fillRect(0, MP_LIST_Y, SCREEN_W, listH, t.bg);

    if (_mpCount == 0) return;

    for (int r = 0; r < MP_ROWS; r++) {
        int idx = _mpOffset + r;
        if (idx >= _mpCount) break;
        int ry = MP_LIST_Y + r * MP_ROW_H;
        bool isSel     = (idx == _mpSel);
        bool isPlaying = (idx == _mpPlaying) && audioIsBusy();
        uint16_t bg = isSel ? t.selected : (r % 2 ? t.rowOdd : t.rowEven);
        uint16_t fg = isSel ? (t.selText ? t.selText : (uint16_t)COL_WHITE)
                            : (isPlaying ? t.accent : t.textPri);
        lcd.fillRect(0, ry, SCREEN_W, MP_ROW_H, bg);
        lcd.drawFastHLine(0, ry + MP_ROW_H - 1, SCREEN_W, (uint16_t)COL_SEP);
        if (isPlaying)
            lcd.fillTriangle(7, ry + 7, 7, ry + MP_ROW_H - 7, 15, ry + MP_ROW_H / 2, fg);
        char numBuf[4]; snprintf(numBuf, sizeof(numBuf), "%d", idx + 1);
        fsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(t.textSec);
        lcd.drawString(numBuf, isPlaying ? 22 : 8, ry + MP_ROW_H / 2);
        lcd.setTextColor(fg);
        lcd.drawString(_mpFiles[idx], isPlaying ? 36 : 28, ry + MP_ROW_H / 2);
    }
}

static void _mpDrawFull() {
    const Theme565& t = getTheme();
    bool ru = (settings.language == LANG_RU);
    lcd.fillScreen(t.bg);

    // Шапка
    lcd.fillRect(0, 0, SCREEN_W, MP_HDR_H, t.header);
    lcd.drawFastHLine(0, MP_HDR_H - 1, SCREEN_W, t.accent);
    // Иконка нотки
    int mx = 18, my = MP_HDR_H / 2;
    lcd.fillRect(mx, my - 5, 2, 9, t.accent);
    lcd.fillRect(mx + 5, my - 7, 2, 8, t.accent);
    lcd.fillRect(mx, my - 5, 7, 2, t.accent);
    lcd.fillCircle(mx - 1, my + 4, 3, t.accent);
    lcd.fillCircle(mx + 4, my + 1, 3, t.accent);
    // Заголовок
    if (ru) {
        fmd(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor((uint16_t)COL_GOLD);
        const char *s = cyrStr("МУЗЫКА");
        lcd.drawString(s, SCREEN_W / 2, MP_HDR_H / 2);
        lcd.drawString(s, SCREEN_W / 2 + 1, MP_HDR_H / 2);
    } else {
        flg(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor((uint16_t)COL_GOLD);
        lcd.drawString("MUSIC", SCREEN_W / 2, MP_HDR_H / 2);
    }
    // Перемешивание / счётчик справа
    fsm(); lcd.setTextDatum(MR_DATUM);
    if (_mpShuffle) {
        lcd.setTextColor(t.accent);
        lcd.drawString("RND", SCREEN_W - 8, MP_HDR_H / 2);
    } else if (_mpCount > 0) {
        lcd.setTextColor(t.textSec);
        char cnt[8]; snprintf(cnt, sizeof(cnt), "%d", _mpCount);
        lcd.drawString(cnt, SCREEN_W - 8, MP_HDR_H / 2);
    }

    _mpDrawNowPlaying();
    _mpDrawList();

    // Подвал — 4 кнопки: НАЗАД | << | ИГРАТЬ/СТОП | >>
    int fy = SCREEN_H - MP_BAR_H;
    lcd.fillRect(0, fy, SCREEN_W, MP_BAR_H, t.header);
    lcd.drawFastHLine(0, fy, SCREEN_W, t.accent);
    int bw = SCREEN_W / 4;  // 80px на кнопку
    lcd.drawFastVLine(bw,     fy + 4, MP_BAR_H - 8, t.accent);
    lcd.drawFastVLine(bw * 2, fy + 4, MP_BAR_H - 8, t.accent);
    lcd.drawFastVLine(bw * 3, fy + 4, MP_BAR_H - 8, t.accent);
    int cy = fy + MP_BAR_H / 2;
    fsm(); lcd.setTextDatum(MC_DATUM);
    // НАЗАД
    lcd.setTextColor(t.textPri);
    lcd.drawString(ru ? cyrStr("НАЗАД") : "BACK", bw / 2, cy);
    // <<
    lcd.setTextColor(_mpCount > 0 ? t.textPri : t.textSec);
    lcd.drawString("<<", bw + bw / 2, cy);
    // ИГРАТЬ / СТОП
    if (audioIsBusy()) {
        lcd.setTextColor(t.danger);
        lcd.drawString(ru ? cyrStr("СТОП") : "STOP", bw * 2 + bw / 2, cy);
    } else {
        lcd.setTextColor(t.accent);
        lcd.drawString(ru ? cyrStr("ИГРАТЬ") : "PLAY", bw * 2 + bw / 2, cy);
    }
    // >>
    lcd.setTextColor(_mpCount > 0 ? t.textPri : t.textSec);
    lcd.drawString(">>", bw * 3 + bw / 2, cy);
}

void musicPlayerOpen() {
    _mpCount = 0; _mpSel = 0; _mpOffset = 0; _mpPlaying = -1; _mpShuffle = false;
    _mpScan();
    _mpDrawFull();
}

void musicPlayerDraw() {
    _mpDrawFull();
}

void musicPlayerTick() {
    // Трек закончился — авто-переход к следующему
    if (_mpPlaying >= 0 && !audioIsBusy()) {
        if (_mpCount > 0) {
            int next;
            if (_mpShuffle && _mpCount > 1) {
                do { next = (int)random(_mpCount); } while (next == _mpPlaying);
            } else {
                next = (_mpPlaying + 1) % _mpCount;
            }
            _mpSel = next;
            if (_mpSel < _mpOffset) _mpOffset = _mpSel;
            if (_mpSel >= _mpOffset + MP_ROWS) _mpOffset = _mpSel - MP_ROWS + 1;
            _mpPlaySelected();
            _mpDrawFull();
        } else {
            _mpPlaying = -1;
        }
    }
}

uint8_t musicPlayerHandleTouch(int x, int y) {
    int fy = SCREEN_H - MP_BAR_H;
    int bw = SCREEN_W / 4;

    // Подвал
    if (y >= fy) {
        if (x < bw) {
            soundStop(); _mpPlaying = -1;
            return BTN_B;
        } else if (x < bw * 2) {
            // << ПРЕД
            if (_mpCount > 0) {
                if (_mpShuffle && _mpCount > 1) {
                    int next; do { next = (int)random(_mpCount); } while (next == _mpSel);
                    _mpSel = next;
                } else {
                    _mpSel = (_mpSel - 1 + _mpCount) % _mpCount;
                }
                if (_mpSel < _mpOffset) _mpOffset = _mpSel;
                _mpPlaySelected();
                _mpDrawFull();
            }
        } else if (x < bw * 3) {
            // ИГРАТЬ / СТОП
            if (audioIsBusy()) {
                soundStop(); _mpPlaying = -1;
            } else {
                _mpPlaySelected();
            }
            _mpDrawFull();
        } else {
            // >> СЛЕД
            if (_mpCount > 0) {
                if (_mpShuffle && _mpCount > 1) {
                    int next; do { next = (int)random(_mpCount); } while (next == _mpSel);
                    _mpSel = next;
                } else {
                    _mpSel = (_mpSel + 1) % _mpCount;
                }
                if (_mpSel >= _mpOffset + MP_ROWS) _mpOffset = _mpSel - MP_ROWS + 1;
                _mpPlaySelected();
                _mpDrawFull();
            }
        }
        return 0;
    }

    // Строка "сейчас играет" — тап переключает режим перемешивания
    if (y >= MP_HDR_H && y < MP_LIST_Y) {
        _mpShuffle = !_mpShuffle;
        _mpDrawFull();
        return 0;
    }

    // Список треков
    if (y >= MP_LIST_Y && y < fy) {
        int row = (y - MP_LIST_Y) / MP_ROW_H;
        int idx = _mpOffset + row;
        if (idx >= 0 && idx < _mpCount) {
            _mpSel = idx;
            _mpPlaySelected();
            _mpDrawFull();
        }
    }
    return 0;
}

uint8_t musicPlayerNavBtn(uint8_t btn) {
    if (btn & BTN_B) {
        soundStop(); _mpPlaying = -1;
        return BTN_B;
    }
    if (btn & BTN_SEL) {
        _mpShuffle = !_mpShuffle;
        _mpDrawFull();
        return 0;
    }
    if (btn & BTN_LEFT) {
        if (_mpCount > 0) {
            if (_mpShuffle && _mpCount > 1) {
                int next; do { next = (int)random(_mpCount); } while (next == _mpSel);
                _mpSel = next;
            } else {
                _mpSel = (_mpSel - 1 + _mpCount) % _mpCount;
            }
            if (_mpSel < _mpOffset) _mpOffset = _mpSel;
            _mpPlaySelected(); _mpDrawFull();
        }
        return 0;
    }
    if (btn & BTN_RIGHT) {
        if (_mpCount > 0) {
            if (_mpShuffle && _mpCount > 1) {
                int next; do { next = (int)random(_mpCount); } while (next == _mpSel);
                _mpSel = next;
            } else {
                _mpSel = (_mpSel + 1) % _mpCount;
            }
            if (_mpSel >= _mpOffset + MP_ROWS) _mpOffset = _mpSel - MP_ROWS + 1;
            _mpPlaySelected(); _mpDrawFull();
        }
        return 0;
    }
    if (btn & BTN_UP) {
        if (_mpSel > 0) {
            _mpSel--;
            if (_mpSel < _mpOffset) _mpOffset = _mpSel;
            _mpDrawNowPlaying();
            _mpDrawList();
        }
        return 0;
    }
    if (btn & BTN_DOWN) {
        if (_mpSel < _mpCount - 1) {
            _mpSel++;
            if (_mpSel >= _mpOffset + MP_ROWS) _mpOffset = _mpSel - MP_ROWS + 1;
            _mpDrawNowPlaying();
            _mpDrawList();
        }
        return 0;
    }
    if (btn & BTN_A) {
        if (audioIsBusy()) {
            soundStop(); _mpPlaying = -1; _mpDrawFull();
        } else {
            _mpPlaySelected(); _mpDrawFull();
        }
        return 0;
    }
    return 0;
}

// ══════════════════════════════════════════════════════════════
// PUBLIC GALLERY API
// ══════════════════════════════════════════════════════════════

void galleryOpen() {
    _galLevel = 0;
    _galGameSel = 0; _galGameOff = 0;
    _galShotSel = 0; _galShotOff = 0;
    _galScanGames();
    _galDrawGamesLevel();
}

void galleryDraw() {
    if      (_galLevel == 0) _galDrawGamesLevel();
    else if (_galLevel == 1) _galDrawShotsLevel();
    else if (_galLevel == 2) _galDrawFullscreen();
}

// Возвращает BTN_B когда пользователь вышел из галереи
uint8_t galleryNavBtn(uint8_t btn) {
    if (_galLevel == 2) {
        // Полный экран: STA = установить обложку, B/SEL = назад
        if (btn & BTN_STA) {
            if (_galConfirmCover(_galGames[_galGameSel])) {
                _galSetAsCover(_galShots[_galShotSel], _galGames[_galGameSel]);
                _coverLastRom = -2;  // сбрасываем кэш обложки в меню
            }
            _galDrawFullscreen();
            return 0;
        }
        if (btn & (BTN_B | BTN_SEL)) {
            _galLevel = 1; _galDrawShotsLevel(); return 0;
        }
        return 0;
    }
    if (_galLevel == 1) {
        if (btn & BTN_UP)   { _galMoveSel(-1); return 0; }
        if (btn & BTN_DOWN) { _galMoveSel(+1); return 0; }
        if (btn & (BTN_A | BTN_STA)) {
            _galLevel = 2; _galDrawFullscreen(); return 0;
        }
        if (btn & (BTN_B | BTN_SEL)) {
            _galLevel = 0; _galDrawGamesLevel(); return 0;
        }
        return 0;
    }
    // Уровень 0
    if (btn & BTN_UP)   { _galMoveSel(-1); return 0; }
    if (btn & BTN_DOWN) { _galMoveSel(+1); return 0; }
    if (btn & (BTN_A | BTN_STA)) {
        if (_galGameCnt == 0) return 0;
        _galShotSel = 0; _galShotOff = 0;
        _galScanShots(_galGames[_galGameSel]);
        _galLevel = 1; _galDrawShotsLevel(); return 0;
    }
    if (btn & (BTN_B | BTN_SEL)) return BTN_B;  // выход из галереи
    return 0;
}

uint8_t galleryHandleTouch(int x, int y) {
    if (_galLevel == 2) {
        // Нижняя панель (y >= SCREEN_H-28):
        //   левая половина → Set cover  |  правая половина → Back
        if (y >= SCREEN_H - 28) {
            if (x < SCREEN_W / 2) {
                // Установить обложку
                if (_galConfirmCover(_galGames[_galGameSel])) {
                    _galSetAsCover(_galShots[_galShotSel], _galGames[_galGameSel]);
                    _coverLastRom = -2;
                }
                _galDrawFullscreen();
            } else {
                // Назад
                _galLevel = 1; _galDrawShotsLevel();
            }
            return 0;
        }
        // Тап по картинке → назад
        _galLevel = 1; _galDrawShotsLevel(); return 0;
    }
    if (_galLevel == 1) {
        if (y < M_HDR_H) {
            // Тап по заголовку (стрелка назад)
            if (x < 30) { _galLevel = 0; _galDrawGamesLevel(); return 0; }
            return 0;
        }
        if (y >= M_DPAD_Y) return 0;
        if (x >= M_PANEL_X) {
            // Тап по правой панели → полный экран
            if (_galShotCnt > 0) { _galLevel = 2; _galDrawFullscreen(); } return 0;
        }
        int slot = (y - M_HDR_H) / GAL_ROW_H;
        int idx  = _galShotOff + slot;
        if (idx >= _galShotCnt) return 0;
        if (idx == _galShotSel) {
            _galLevel = 2; _galDrawFullscreen(); return 0;
        }
        int oldSel = _galShotSel; _galShotSel = idx;
        if (_galShotSel < _galShotOff) _galShotOff = _galShotSel;
        if (_galShotSel >= _galShotOff + GAL_ROWS) _galShotOff = _galShotSel - GAL_ROWS + 1;
        char lbl[24];
        auto shotLabel = [](int i, char *b, int bs) {
            const char *fn = strrchr(_galShots[i], '/'); fn = fn ? fn + 1 : _galShots[i];
            int fnl = strlen(fn);
            if (fnl >= 7) snprintf(b, bs, "Shot %c%c%c", fn[fnl-7], fn[fnl-6], fn[fnl-5]);
            else          snprintf(b, bs, "Shot ???");
        };
        int olds = oldSel - _galShotOff;
        int news = _galShotSel - _galShotOff;
        if (olds >= 0 && olds < GAL_ROWS) { shotLabel(oldSel, lbl, sizeof(lbl)); _galDrawRow(olds, false, lbl, nullptr); }
        if (news >= 0 && news < GAL_ROWS) { shotLabel(_galShotSel, lbl, sizeof(lbl)); _galDrawRow(news, true, lbl, nullptr); }
        _galUpdatePanel();
        return 0;
    }
    // Уровень 0
    if (y < M_HDR_H || y >= M_DPAD_Y) return 0;
    if (x >= M_PANEL_X) return 0;
    int slot = (y - M_HDR_H) / GAL_ROW_H;
    int idx  = _galGameOff + slot;
    if (idx >= _galGameCnt) return 0;
    if (idx == _galGameSel) {
        // Двойной тап = открыть список скриншотов
        _galShotSel = 0; _galShotOff = 0;
        _galScanShots(_galGames[_galGameSel]);
        _galLevel = 1; _galDrawShotsLevel(); return 0;
    }
    int oldSel = _galGameSel; _galGameSel = idx;
    if (_galGameSel < _galGameOff) _galGameOff = _galGameSel;
    if (_galGameSel >= _galGameOff + GAL_ROWS) _galGameOff = _galGameSel - GAL_ROWS + 1;
    int olds = oldSel - _galGameOff, news = _galGameSel - _galGameOff;
    if (olds >= 0 && olds < GAL_ROWS) _galDrawRow(olds, false, _galGames[oldSel], nullptr);
    if (news >= 0 && news < GAL_ROWS) _galDrawRow(news, true,  _galGames[_galGameSel], nullptr);
    _galUpdatePanel();
    return 0;
}
