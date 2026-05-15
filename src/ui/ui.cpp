// ui.cpp — RetroESP UI — редизайн меню в стиле AleksOS v2
#include "ui/ui.h"

// ── Forward declarations для иконок (используются до определения) ─────────
static void iconTag(int cx,int cy,uint16_t c);
static void iconChip(int cx,int cy,uint16_t c);
static void iconSave(int cx,int cy,uint16_t c);
static void iconClock2(int cx,int cy,uint16_t c);
// ─────────────────────────────────────────────────────────────────────────
#include "display/display_manager.h"
#include "storage/sd_manager.h"
#include "system/time_manager.h"
#include "settings.h"
#include "config.h"
#include "lang.h"
#include "input/button_handler.h"
#include <SD.h>
#include <Arduino.h>

// ── Шрифты ───────────────────────────────────────────────────
#define FSM  (&lgfx::fonts::DejaVu9)
#define FMD  (&lgfx::fonts::DejaVu12)
#define FLG  (&lgfx::fonts::DejaVu18)
#define FXL  (&lgfx::fonts::DejaVu40)

static void fsm() { lcd.setFont(FSM); }
static void fmd() { lcd.setFont(FMD); }
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
    lcd.drawRect(cx-9, cy-6, 18, 12, c);
    lcd.fillRect(cx-7, cy-4, 14, 8, c);
    lcd.fillRect(cx-5, cy-2, 10, 4, 0x0863);
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
    lcd.fillCircle(cx, cy, 5, c);
    lcd.fillCircle(cx, cy, 2, 0x0863);
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
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    lcd.drawFastHLine(0, HDR_H-1, SCREEN_W, t.accent);
    lcd.drawRect(4, 7, HDR_BACK_W-8, HDR_H-14, 0x2945);
    fsm(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(t.accent);
    lcd.drawString(S().back, HDR_BACK_W/2, HDR_H/2);
    fmd(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(t.textPri);
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
    uint16_t bg = sel ? t.selected : (alt ? t.rowOdd : t.rowEven);
    lcd.fillRect(0, y, SCREEN_W-4, rowH, bg);
    if (sel) lcd.fillRect(0, y, 3, rowH, t.accent);
    fsm();
    if (left) {
        lcd.setTextColor(sel ? t.bg : t.textPri);
        lcd.setTextDatum(ML_DATUM);
        lcd.drawString(left, 10, y + rowH/2);
    }
    if (right) {
        lcd.setTextColor(sel ? t.bg : t.accent);
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
    lcd.drawString(S().loading, SCREEN_W/2, 198);
}

void drawSDError() {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);
    lcd.fillCircle(SCREEN_W/2, 80, 44, t.danger);
    fxl(); lcd.setTextColor(t.bg); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("!", SCREEN_W/2, 80);
    fmd(); lcd.setTextColor(t.textPri);
    lcd.drawString(S().noSD, SCREEN_W/2, 148);
    fsm(); lcd.setTextColor(t.textSec);
    lcd.drawString(S().noSDHint, SCREEN_W/2, 168);
}

// ══════════════════════════════════════════════════════════════
// ══ НОВОЕ ГЛАВНОЕ МЕНЮ ════════════════════════════════════════
// ══════════════════════════════════════════════════════════════

static int _menuSel    = 0;
static int _menuOffset = 0;

// ── Нижняя панель меню: [▶ PLAY] [HH:MM] [⚙ SETUP] ───────────
static void drawMenuBar() {
    const Theme565 &t = getTheme();
    int by = DPAD_Y;
    lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.header);
    lcd.drawFastHLine(0, by, SCREEN_W, COL_TOPBAR);

    // ── PLAY button (зона 1) ──────────────────────────────────
    lcd.fillRoundRect(4, by+5, MBAR_PLAY_W-8, 34, 8, t.selected);
    // Треугольник ▶
    int tx = 18, ty = by + BTNBAR_H/2;
    lcd.fillTriangle(tx, ty-7, tx, ty+7, tx+10, ty, (uint16_t)COL_GOLD);
    fmd(); lcd.setTextColor((uint16_t)COL_WHITE); lcd.setTextDatum(ML_DATUM);
    lcd.drawString(S().play, tx+14, ty);

    // ── Вертикальный разделитель ──────────────────────────────
    lcd.drawFastVLine(MBAR_TIME_X, by+6, BTNBAR_H-12, 0x3186);
    lcd.drawFastVLine(MBAR_SET_X,  by+6, BTNBAR_H-12, 0x3186);

    // ── Время (зона 2) ────────────────────────────────────────
    flg(); lcd.setTextColor((uint16_t)COL_CYAN); lcd.setTextDatum(MC_DATUM);
    lcd.drawString(timeGetString().c_str(), (MBAR_TIME_X + MBAR_SET_X)/2, by + BTNBAR_H/2);

    // ── SETUP (зона 3) ────────────────────────────────────────
    int scx = MBAR_SET_X + 16;
    iconSystem(scx, by + BTNBAR_H/2, t.textSec);
    fmd(); lcd.setTextColor(t.textSec); lcd.setTextDatum(ML_DATUM);
    lcd.drawString(S().setup, scx + 14, by + BTNBAR_H/2);
}

// ── Одна строка ROM ────────────────────────────────────────────
static void drawRomRow(int romIdx, int slot) {
    const Theme565 &t = getTheme();
    int y   = HDR_H + slot * ROW_H;
    bool sel = (romIdx == _menuSel);

    if (sel) {
        // Синий фон выбранной строки
        lcd.fillRoundRect(3, y+2, SCREEN_W-6, ROW_H-4, 6, t.selected);
        // Золотой ▶
        int tx = 14, ty = y + ROW_H/2;
        lcd.fillTriangle(tx, ty-6, tx, ty+6, tx+9, ty, (uint16_t)COL_GOLD);
        // Текст белый
        fmd(); lcd.setTextColor((uint16_t)COL_WHITE); lcd.setTextDatum(ML_DATUM);
        String name = sdMgr.get(romIdx).name;
        if (name.length() > 26) name = name.substring(0, 24) + "..";
        lcd.drawString(name.c_str(), 28, y + ROW_H/2);
    } else {
        // Тонкий разделитель сверху каждой строки
        if (slot > 0)
            lcd.drawFastHLine(10, y, SCREEN_W-20, (uint16_t)COL_SEP);
        // Текст
        fmd(); lcd.setTextColor(t.textPri); lcd.setTextDatum(ML_DATUM);
        String name = sdMgr.get(romIdx).name;
        if (name.length() > 28) name = name.substring(0, 26) + "..";
        lcd.drawString(name.c_str(), 12, y + ROW_H/2);
    }
}

// ── Индикаторы прокрутки (▲ вверху, ▼ внизу) ──────────────────
static void drawScrollIndicators(int total) {
    const Theme565 &t = getTheme();
    if (_menuOffset > 0) {
        // ▲ в правом верхнем углу списка
        int ax = SCREEN_W - 14, ay = HDR_H + 8;
        lcd.fillTriangle(ax, ay+6, ax-5, ay+12, ax+5, ay+12, t.accent);
    }
    if (_menuOffset + ROWS_VISIBLE < total) {
        // ▼ в правом нижнем углу списка
        int ax = SCREEN_W - 14, ay = DPAD_Y - 14;
        lcd.fillTriangle(ax, ay+6, ax-5, ay, ax+5, ay, t.accent);
    }
}

// ── Главная функция отрисовки меню ─────────────────────────────
void menuDraw() {
    const Theme565 &t = getTheme();

    // Фон
    lcd.fillScreen(t.bg);

    // ── Шапка ────────────────────────────────────────────────
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    // Заголовок
    flg(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor((uint16_t)COL_GOLD);
    lcd.drawString("NES GAMES", SCREEN_W/2, HDR_H/2);
    // Акцентная линия внизу шапки
    lcd.drawFastHLine(0, HDR_H-1, SCREEN_W, t.accent);
    // Бейдж с количеством ROM-ов (справа в шапке)
    if (sdMgr.count() > 0) {
        char badge[12];
        snprintf(badge, sizeof(badge), "%d ROMs", sdMgr.count());
        fsm(); lcd.setTextColor(t.textSec); lcd.setTextDatum(MR_DATUM);
        lcd.drawString(badge, SCREEN_W - 8, HDR_H/2);
    }

    // ── Список ROM ────────────────────────────────────────────
    int total = sdMgr.count();
    if (total == 0) {
        fmd(); lcd.setTextColor(t.textSec); lcd.setTextDatum(MC_DATUM);
        int cy = HDR_H + (DPAD_Y - HDR_H)/2;
        lcd.drawString(S().noRoms, SCREEN_W/2, cy - 12);
        fsm(); lcd.drawString(S().noRomsHint, SCREEN_W/2, cy + 12);
    } else {
        int end = min(_menuOffset + ROWS_VISIBLE, total);
        for (int i = _menuOffset; i < end; i++) {
            drawRomRow(i, i - _menuOffset);
        }
        drawScrollIndicators(total);
    }

    // ── Нижняя панель ─────────────────────────────────────────
    drawMenuBar();
}

// ── Обновление только времени в нижней панели (без перерисовки всего) ──
static uint8_t _menuLastMin = 0xFF;
void menuTimeTick() {

    uint8_t m = timeGetM();
    if (m == _menuLastMin) return;
    _menuLastMin = m;
    const Theme565 &t = getTheme();
    // Перерисовываем только зону времени
    int bx = MBAR_TIME_X + 1, by = DPAD_Y + 1;
    int bw = MBAR_SET_X - MBAR_TIME_X - 2, bh = BTNBAR_H - 2;
    lcd.fillRect(bx, by, bw, bh, t.header);
    flg(); lcd.setTextColor((uint16_t)COL_CYAN); lcd.setTextDatum(MC_DATUM);
    lcd.drawString(timeGetString().c_str(), (MBAR_TIME_X + MBAR_SET_X)/2, DPAD_Y + BTNBAR_H/2);
}

void menuScrollUp() {
    if (_menuSel > 0) {
        _menuSel--;
        if (_menuSel < _menuOffset) _menuOffset = _menuSel;
        menuDraw();
    }
}

void menuScrollDown() {
    int total = sdMgr.count();
    if (_menuSel < total-1) {
        _menuSel++;
        if (_menuSel >= _menuOffset + ROWS_VISIBLE)
            _menuOffset = _menuSel - ROWS_VISIBLE + 1;
        menuDraw();
    }
}

uint8_t menuHandleTouch(int x, int y, int &romAction) {
    romAction = 0;

    // ── Нижняя панель ─────────────────────────────────────────
    if (y >= DPAD_Y) {
        if (x < MBAR_TIME_X) {
            // Зона PLAY
            romAction = 1;
            return BTN_A;
        } else if (x >= MBAR_SET_X) {
            // Зона SETUP
            return BTN_SEL;
        }
        // Зона времени — нет действия
        return 0;
    }

    // ── Заголовок ─────────────────────────────────────────────
    if (y < HDR_H) return 0;

    // ── Индикаторы прокрутки (правая полоса списка) ───────────
    if (x > SCREEN_W - 22) {
        if (y < HDR_H + ROW_H) { menuScrollUp();   return 0; }
        if (y > DPAD_Y - ROW_H){ menuScrollDown(); return 0; }
    }

    // ── Строки ROM ────────────────────────────────────────────
    if (y > HDR_H && y < DPAD_Y && sdMgr.count() > 0) {
        int row    = (y - HDR_H) / ROW_H;
        int tapped = _menuOffset + row;
        if (tapped < 0 || tapped >= sdMgr.count()) return 0;
        TapType tt = touch.checkDoubleTap(x, y, tapped);
        if (tt == TAP_DOUBLE) { romAction = 2; return BTN_A; }
        if (tapped != _menuSel) { _menuSel = tapped; menuDraw(); }
    }
    return 0;
}

int menuSelected() { return _menuSel; }

// ══════════════════════════════════════════════════════════════
// ROM INFO POPUP
// ══════════════════════════════════════════════════════════════

void showRomInfo(int idx) {
    if (idx < 0 || idx >= sdMgr.count()) return;
    const ROMInfo &rom = sdMgr.get(idx);
    const Theme565 &t = getTheme();

    int px = 16, py = 36, pw = SCREEN_W-32, ph = SCREEN_H-80;
    lcd.fillRoundRect(px+3, py+3, pw, ph, 10, 0x0841);
    lcd.fillRoundRect(px, py, pw, ph, 10, t.header);
    lcd.drawRoundRect(px, py, pw, ph, 10, t.accent);
    lcd.drawFastHLine(px+8, py+34, pw-16, 0x2945);
    fmd(); lcd.setTextColor(t.accent); lcd.setTextDatum(MC_DATUM);
    lcd.drawString(S().romInfo, SCREEN_W/2, py+17);

    fsm(); lcd.setTextDatum(ML_DATUM);
    int lx = px+12, rx = px+pw-12, ly = py+46;
    auto row = [&](const char *lbl, String val) {
        lcd.setTextColor(t.textSec); lcd.drawString(lbl, lx, ly);
        lcd.setTextColor(t.textPri); lcd.setTextDatum(MR_DATUM);
        lcd.drawString(val.c_str(), rx, ly);
        lcd.setTextDatum(ML_DATUM);
        ly += 22;
    };
    row(S().romMapper, "N/A");
    String sz = rom.size < 1024 ? String(rom.size)+"B" : String(rom.size/1024)+"KB";
    row(S().romSize, sz);
    String p = rom.path; if(p.length()>26) p=p.substring(0,24)+"..";
    row(S().romPath, p);
    String n = rom.name; if(n.length()>22) n=n.substring(0,20)+"..";
    row("Name:", n);

    int bw = pw-40, bx = px+20, by = ly+6;
    lcd.fillRoundRect(bx, by, bw, 28, 8, t.accent);
    fsm(); lcd.setTextColor(t.header); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("OK", px+pw/2, by+14);

    delay(300);
    extern TouchHandler touch;
    uint32_t until = millis()+8000;
    while (millis()<until) { if(touch.isTouched()) break; delay(20); }
}

// ══════════════════════════════════════════════════════════════
// НАСТРОЙКИ — данные
// ══════════════════════════════════════════════════════════════

static const char *_catName[] = {
    "Display", "Audio", "Appearance", "System", "Controls", "Info"
};

static const int _catItems[][5] = {
    { 0, 4, 7, -1, -1  },  // Display
    { 1, 14, 8, 9, -1  },  // Audio
    { 2, 3, -1, -1, -1 },  // Appearance
    { 5, 6, 17, 18, 19 },  // System: FPS, AutoSave, Hour, Minute, AutoScroll
    { 12, 13, 15, 16, -1}, // Controls
    { 10, 11, -1, -1, -1}, // Info
};
#define CAT_COUNT 6

static int _settingsCat  = -1;
static int _catItemIdx   =  0;
static int _gridSelected = -1;
static int _detailRowH   = 32;  // реальная высота строки в текущем detail
static int _detailListTop= 44;  // реальный Y начала списка
static uint32_t _detailOpenedMs = 0;  // момент открытия — debounce тача  // тайл подсвечен (ожидает второго тапа)

static int catItemCount(int cat) {
    int n = 0;
    while (n < 5 && _catItems[cat][n] >= 0) n++;
    return n;
}

static int globalSettingIdx(int cat, int itemInCat) {
    return _catItems[cat][itemInCat];
}

static String settingValue(int gi) {
    const char *themes[] = {"Dark","Light","Green","Amber"};
    const char *langs[]  = {"RU","EN"};
    const char *scales[] = {"Fit","4:3","1:1"};
    const char *snds[]   = {"Beep","Click","Chime"};
    switch(gi) {
        case 0:  return String(settings.brightness)+"%";
        case 1:  return String(settings.volume)+"%";
        case 2:  return themes[(int)settings.theme%4];
        case 3:  return langs[(int)settings.language%2];
        case 4:  return scales[(int)settings.scale%3];
        case 5:  return settings.showFPS ? "On" : "Off";
        case 6:  return settings.autoSave ? "On" : "Off";
        case 7:  return settings.autoBrightness ? "On" : "Off";
        case 8:  return settings.soundEnabled ? "On" : "Off";
        case 9:  return snds[(int)settings.soundType%3];
        case 10: return FIRMWARE_VERSION;
        case 11: return String(ESP.getFreeHeap()/1024)+"KB";
        case 12: return "Edit >";
        case 13: return buttons.isConnected() ? "OK" : "---";
        case 14: return String(settings.emuVolume)+"%";
        case 15: return settings.vibroEnabled ? "On" : "Off";
        case 16: return String(settings.vibroStrength)+"%";
        case 17: { char b[4]; snprintf(b,4,"%02d", timeGetH()); return String(b)+"h"; }
        case 18: { char b[4]; snprintf(b,4,"%02d", timeGetM()); return String(b)+"m"; }
        case 19: return settings.autoScroll ? "On" : "Off";
    }
    return "";
}

static void settingInc(int gi) {
    switch(gi) {
        case 0: settings.brightness=min(100,(int)settings.brightness+10); setBrightness(settings.brightness); break;
        case 1: settings.volume=min(100,(int)settings.volume+10); break;
        case 2: settings.theme=(Theme)(((int)settings.theme+1)%THEME_COUNT); break;
        case 3: settings.language=(Language)(((int)settings.language+1)%LANG_COUNT); break;
        case 4: settings.scale=(Scale)(((int)settings.scale+1)%SCALE_COUNT); break;
        case 5: settings.showFPS=!settings.showFPS; break;
        case 6: settings.autoSave=!settings.autoSave; break;
        case 7: settings.autoBrightness=!settings.autoBrightness; break;
        case 8: settings.soundEnabled=!settings.soundEnabled; break;
        case 9: settings.soundType=(SoundType)(((int)settings.soundType+1)%SND_COUNT); break;
        case 14: settings.emuVolume=min(100,(int)settings.emuVolume+10); break;
        case 15: settings.vibroEnabled=!settings.vibroEnabled; break;
        case 16: settings.vibroStrength=min(100,(int)settings.vibroStrength+10); break;
        case 17: timeSet((timeGetH()+1)%24, timeGetM()); break;
        case 18: timeSet(timeGetH(), (timeGetM()+1)%60); break;
        case 19: settings.autoScroll = !settings.autoScroll; break;
    }
}

static void settingDec(int gi) {
    switch(gi) {
        case 0: settings.brightness=max(10,(int)settings.brightness-10); setBrightness(settings.brightness); break;
        case 1: settings.volume=(settings.volume<10)?0:settings.volume-10; break;
        case 2: settings.theme=(Theme)(((int)settings.theme-1+THEME_COUNT)%THEME_COUNT); break;
        case 3: settings.language=(Language)(((int)settings.language-1+LANG_COUNT)%LANG_COUNT); break;
        case 4: settings.scale=(Scale)(((int)settings.scale-1+SCALE_COUNT)%SCALE_COUNT); break;
        case 5: settings.showFPS=!settings.showFPS; break;
        case 6: settings.autoSave=!settings.autoSave; break;
        case 7: settings.autoBrightness=!settings.autoBrightness; break;
        case 8: settings.soundEnabled=!settings.soundEnabled; break;
        case 9: settings.soundType=(SoundType)(((int)settings.soundType-1+SND_COUNT)%SND_COUNT); break;
        case 14: settings.emuVolume=(settings.emuVolume<10)?0:settings.emuVolume-10; break;
        case 15: settings.vibroEnabled=!settings.vibroEnabled; break;
        case 16: settings.vibroStrength=(settings.vibroStrength<10)?10:settings.vibroStrength-10; break;
        case 17: timeSet((timeGetH()-1+24)%24, timeGetM()); break;
        case 18: timeSet(timeGetH(), (timeGetM()-1+60)%60); break;
        case 19: settings.autoScroll = !settings.autoScroll; break;
    }
}

static void drawCatIcon(int cat, int cx, int cy, uint16_t c) {
    switch(cat) {
        case 0: iconDisplay (cx, cy, c); break;
        case 1: iconAudio   (cx, cy, c); break;
        case 2: iconLook    (cx, cy, c); break;
        case 3: iconSystem  (cx, cy, c); break;
        case 4: iconControls(cx, cy, c); break;
        case 5: iconInfo    (cx, cy, c); break;
    }
}

// ── Нижняя панель настроек: [← Back] [time] [nav arrows] ───────
static void drawSettingsBar(bool showBack, int navMode) {
    // navMode: 0=none, 1=up/down/LR, 2=up/down only
    const Theme565 &t = getTheme();
    int by = DPAD_Y;
    lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.header);
    lcd.drawFastHLine(0, by, SCREEN_W, (uint16_t)COL_TOPBAR);
    int ty = by + BTNBAR_H/2;

    if (showBack) {
        // Blue BACK button (zone 0, x 0..79)
        lcd.fillRoundRect(4, by+5, 72, 34, 8, t.selected);
        // Left arrow ←
        int ax = 16;
        lcd.fillTriangle(ax, ty, ax+8, ty-6, ax+8, ty+6, (uint16_t)COL_GOLD);
        fmd(); lcd.setTextColor((uint16_t)COL_WHITE); lcd.setTextDatum(ML_DATUM);
        lcd.drawString(S().back, ax+12, ty);
    }
    // Vertical dividers
    lcd.drawFastVLine(80,  by+6, BTNBAR_H-12, 0x3186);
    lcd.drawFastVLine(240, by+6, BTNBAR_H-12, 0x3186);

    // Time (center)
    flg(); lcd.setTextColor((uint16_t)COL_CYAN); lcd.setTextDatum(MC_DATUM);
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
    lcd.fillScreen(t.bg);

    // ── Заголовок (золотой, крупный) ─────────────────────────
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    flg(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor((uint16_t)COL_GOLD);
    lcd.drawString("SETTINGS", SCREEN_W/2, HDR_H/2);
    // Золотая линия-разделитель
    lcd.drawFastHLine(8, HDR_H, SCREEN_W-16, (uint16_t)COL_GOLD);

    // ── Сетка тайлов ─────────────────────────────────────────
    const int COLS = 3, ROWS = 2, PAD = 6;
    int cw = (SCREEN_W - PAD*(COLS+1)) / COLS;
    int ch = (DPAD_Y - HDR_H - 3 - PAD*(ROWS+1)) / ROWS;

    for (int i = 0; i < CAT_COUNT; i++) {
        int col = i % COLS, row = i / COLS;
        int x = PAD + col*(cw+PAD);
        int y = HDR_H + 5 + PAD + row*(ch+PAD);
        bool sel = (i == _gridSelected);

        if (sel) {
            // ── ВЫБРАННЫЙ: инвертированные цвета ──────────────────
            // Яркий заливной фон
            lcd.fillRoundRect(x, y, cw, ch, 9, t.accent);
            // Золотой бордер
            lcd.drawRoundRect(x,   y,   cw,   ch,   9, (uint16_t)COL_GOLD);
            lcd.drawRoundRect(x+1, y+1, cw-2, ch-2, 8, (uint16_t)COL_GOLD);
            // Иконка тёмная (на светлом фоне)
            int icx = x + cw/2, icy = y + ch*2/5;
            drawCatIcon(i, icx, icy, t.header);
            // Текст тёмный + bold (два прохода)
            fmd(); lcd.setTextDatum(MC_DATUM);
            lcd.setTextColor(t.header);
            lcd.drawString(_catName[i], x+cw/2,   y+ch-14);
            lcd.drawString(_catName[i], x+cw/2+1, y+ch-14);  // bold
        } else {
            // ── ОБЫЧНЫЙ ───────────────────────────────────────────
            lcd.fillRoundRect(x, y, cw, ch, 9, t.header);
            uint16_t glowDim = (uint16_t)((t.accent >> 1) & 0x7BEF);
            lcd.drawRoundRect(x-1, y-1, cw+2, ch+2, 10, glowDim);
            lcd.drawRoundRect(x,   y,   cw,   ch,    9,  t.accent);
            int icx = x + cw/2, icy = y + ch*2/5;
            drawCatIcon(i, icx, icy, (uint16_t)COL_GOLD);
            fmd(); lcd.setTextColor((uint16_t)COL_GOLD); lcd.setTextDatum(MC_DATUM);
            lcd.drawString(_catName[i], x+cw/2, y+ch-14);
        }
    }

    // ── Нижняя панель ─────────────────────────────────────────
    drawSettingsBar(true, 0);
}

// ── Info screen ────────────────────────────────────────────────
struct InfoRow { const char *label; String value; uint16_t vc; bool isDiv; };
static InfoRow _infoRows[52];
static int     _infoCount  = 0;
static int     _infoScroll = 0;

static void buildInfoRows() {
    _infoCount = 0; _infoScroll = 0;
    auto d = [&](const char *t){ _infoRows[_infoCount++] = {t,"",0,true}; };
    auto r = [&](const char *l, String v, uint16_t c=0){ _infoRows[_infoCount++]={l,v,c,false}; };

    // ── Прошивка ──────────────────────────────────────────────
    d("FIRMWARE");
    r("Version:",      FIRMWARE_VERSION,                   0x4EF0);
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

static void drawInfoScreen() {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);

    // ── Заголовок (золотой, как в других категориях) ──────────
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    flg(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor((uint16_t)COL_GOLD);
    lcd.drawString("INFO", SCREEN_W/2, HDR_H/2);
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

    for (int i = 0; i < _infoCount && y < DPAD_Y-2; i++) {
        const InfoRow &row = _infoRows[i];
        int rh = row.isDiv ? INFO_DIV_H : INFO_ROW_H;
        if (skipped + rh <= _infoScroll) { skipped += rh; continue; }
        int yOff = (skipped < _infoScroll) ? (_infoScroll - skipped) : 0;
        skipped += rh;
        int ry = y;
        y += (rh - yOff);
        if (y > DPAD_Y - 2) break;

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
            lcd.drawString(row.label, LX + 20, ry + (rh / 2) + yOff / 2);
            lcd.drawString(row.label, LX + 21, ry + (rh / 2) + yOff / 2); // bold
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
            lcd.drawString(row.label, LX + 6, textY);

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
    drawInfoScreen();
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
        case 19:            iconClock2(cx,cy,c); break;  // auto-scroll
        default:            iconInfo(cx,cy,c); break;
    }
}

static const char* getLabelForGi(int gi) {
    if (gi >= 0 && gi < 12) return S().sLbl[gi];
    switch(gi) {
        case 12: return "Remap Buttons";
        case 13: return "Pico";
        case 14: return "Emu Volume";
        case 15: return "Vibration";
        case 16: return "Vibro Strength";
        case 17: return "Hour";
        case 18: return "Minute";
    }
    return "";
}
static void drawCategoryDetail() {
    if (_settingsCat == 5) { buildInfoRows(); drawInfoScreen(); return; }

    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);

    // ── Заголовок (золотой) ───────────────────────────────────
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    flg(); lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor((uint16_t)COL_GOLD);
    lcd.drawString(_catName[_settingsCat], SCREEN_W/2, HDR_H/2);
    // Синяя линия под заголовком (как в Image 2)
    lcd.drawFastHLine(0, HDR_H,   SCREEN_W, t.accent);
    lcd.drawFastHLine(0, HDR_H+1, SCREEN_W, t.accent);

    // ── Карточка со строками ──────────────────────────────────
    int n = catItemCount(_settingsCat);
    int listTop = HDR_H + 4;
    int listH   = DPAD_Y - listTop - 5;
    // Адаптивная высота строки (больше дышит при малом кол-ве)
    int rowH = min(52, max(30, listH / max(n, 1)));
    _detailRowH    = rowH;    // сохраняем для touch handler
    _detailListTop = listTop; // сохраняем для touch handler

    // Фон карточки
    lcd.fillRoundRect(4, listTop, SCREEN_W-8, n*rowH+4, 6, t.header);
    // Синяя линия над нижней панелью
    lcd.drawFastHLine(0, DPAD_Y-2, SCREEN_W, t.accent);
    lcd.drawFastHLine(0, DPAD_Y-1, SCREEN_W, t.accent);

    for (int i = 0; i < n; i++) {
        int gi  = globalSettingIdx(_settingsCat, i);
        int y   = listTop + 2 + i * rowH;
        bool sel = (i == _catItemIdx);

        // Подсветка выбранной строки
        if (sel) {
            lcd.fillRoundRect(5, y+1, SCREEN_W-10, rowH-2, 4, t.rowOdd);
            lcd.fillRect(5, y+2, 3, rowH-4, t.accent);  // левый акцент
        }
        // Разделитель между строками
        if (i > 0) lcd.drawFastHLine(12, y, SCREEN_W-24, 0x2104);

        // Иконка
        uint16_t icCol = sel ? t.accent : (uint16_t)COL_GOLD;
        drawSettingIcon(gi, 22, y + rowH/2, icCol);

        // Название (белое, bold при выборе)
        fmd(); lcd.setTextDatum(ML_DATUM);
        lcd.setTextColor((uint16_t)COL_WHITE);
        const char *lbl = getLabelForGi(gi);
        lcd.drawString(lbl, 40, y + rowH/2);
        if (sel) lcd.drawString(lbl, 41, y + rowH/2);  // bold

        // Значение (голубое/золотое, bold при выборе)
        uint16_t vCol = sel ? t.accent : (uint16_t)COL_GOLD;
        lcd.setTextColor(vCol);
        lcd.setTextDatum(MR_DATUM);
        String val = settingValue(gi);
        lcd.drawString(val.c_str(), SCREEN_W-12, y + rowH/2);
        if (sel) lcd.drawString(val.c_str(), SCREEN_W-11, y + rowH/2);
    }

    // Нижняя панель: только Back + time (чисто, как в Image 2)
    drawSettingsBar(true, 0);
}

void settingsDraw() {
    if (_settingsCat < 0) drawCategoryGrid();
    else                  drawCategoryDetail();
}

// ── Обработка тача настроек ────────────────────────────────────
static uint8_t handleCategoryGrid(int x, int y) {
    // Bottom bar: BACK (x < 80), time (80..239), no action (240+)
    if (y >= DPAD_Y) {
        if (x < 80) return BTN_B;   // BACK
        return 0;
    }
    if (y < HDR_H) return 0;

    const int COLS = 3, ROWS = 2, PAD = 6;
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
                _detailOpenedMs = millis();  // фиксируем момент открытия
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
        // Только BACK (x < 80). Остальные зоны не используем на тач —
        // значения меняются прямым тапом по строке.
        if (x < 80) {
            _settingsCat = -1;
            _gridSelected = -1;
            drawCategoryGrid();
            return 0;
        }
        // Info: скролл в правой половине
        if (_settingsCat == 5) {
            if (x < SCREEN_W/2) infoScrollBy(-INFO_ROW_H * 2);
            else                 infoScrollBy( INFO_ROW_H * 2);
        }
        return 0;
    }

    if (y < HDR_H) return 0;

    // ── Дебаунс: игнорируем тач 200 мс после открытия категории ──────────
    // Предотвращает «утечку» тапа с сетки в детальный экран
    if (millis() - _detailOpenedMs < 200) return 0;

    // Info — скролл тапом по содержимому
    if (_settingsCat == 5) {
        infoScrollBy(y < (HDR_H + DPAD_Y)/2 ? -INFO_ROW_H*2 : INFO_ROW_H*2);
        return 0;
    }

    // ── Строки настроек ───────────────────────────────────────
    // Используем АКТУАЛЬНУЮ геометрию из drawCategoryDetail()
    int n = catItemCount(_settingsCat);
    if (y >= _detailListTop && y < DPAD_Y - 2) {
        int row = (y - _detailListTop) / _detailRowH;
        if (row < 0 || row >= n) return 0;

        int gi = globalSettingIdx(_settingsCat, row);

        if (row != _catItemIdx) {
            // Первый тап — выбираем строку
            _catItemIdx = row;
            drawCategoryDetail();
            return 0;  // НЕ BTN_A — просто обновление экрана
        }
        // Второй тап по той же строке — меняем значение
        if (gi == 12) {
            // Remap Buttons — явный тап: сигнал открыть remap (0x40)
            return 0x40;
        }
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

void settingsScrollUp()   {}
void settingsScrollDown() {}

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

void btnMapApply() {}

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

        // BTN_A — открываем сразу (кнопки не требуют двойного тапа)
        if (btn & BTN_A) {
            _gridSelected   = -1;
            _settingsCat    = _gridCur;
            _catItemIdx     = 0;
            _detailOpenedMs = millis();
            drawCategoryDetail();
        }
        if (btn & BTN_B) { _gridSelected = -1; return BTN_B; }
    } else if (_settingsCat == 5) {
        if (btn & BTN_UP)   infoScrollBy(-INFO_ROW_H * 3);
        if (btn & BTN_DOWN) infoScrollBy( INFO_ROW_H * 3);
        if (btn & BTN_B) { _settingsCat = -1; _gridSelected = -1; drawCategoryGrid(); }
    } else {
        int n = catItemCount(_settingsCat);
        if (btn & BTN_UP)   { if(_catItemIdx>0){_catItemIdx--;drawCategoryDetail();} }
        if (btn & BTN_DOWN) { if(_catItemIdx<n-1){_catItemIdx++;drawCategoryDetail();} }
        if (btn & BTN_RIGHT) {
            int gi = globalSettingIdx(_settingsCat, _catItemIdx);
            if (gi == 12) return 0x40;  // open remap signal
            settingInc(gi); drawCategoryDetail();
        }
        if (btn & BTN_LEFT) {
            int gi = globalSettingIdx(_settingsCat, _catItemIdx);
            if (gi == 12) return 0x40;  // open remap signal
            settingDec(gi); drawCategoryDetail();
        }
        if (btn & BTN_B) { _settingsCat = -1; _gridSelected = -1; drawCategoryGrid(); }
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
    if (btn & BTN_B) return BTN_B;
    return 0;
}
