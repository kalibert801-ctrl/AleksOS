// theme_ios.cpp — iOS Light тема для AleksOS
//
// Элементы интерфейса и их действия:
//   Nav Bar (шапка):
//     Заголовок "NES GAMES" — нет действия
//     (в реальном iOS тут была бы кнопка "назад" и "Edit")
//   Tab Bar (нижняя панель):
//     🎮 Games  (активная, первая вкладка)  → нет доп. действия
//     🕐 HH:MM  (средняя вкладка)           → нет действия
//     ⚙ Settings (правая вкладка)            → открыть настройки
//   Строки ROM:
//     Одиночный тап → выбрать ROM
//     › disclosure indicator  → показать ROM Info
//     Двойной тап → ROM Info
// ════════════════════════════════════════════════════════════════════
#include "theme_helpers.h"

// ════════════════════════════════════════════════════════════════════
// NAV BAR (шапка) — светлый, жирный заголовок
// ════════════════════════════════════════════════════════════════════
static void ios_drawHeader(int romCount) {
    const Theme565& t = getTheme();
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    // Hairline снизу (iOS border)
    uint16_t hairline = t.shadow ? t.shadow : (uint16_t)0xC618;
    lcd.drawFastHLine(0, HDR_H - 1, SCREEN_W, hairline);

    // Жирный заголовок — двойной проход
    th_fmd(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textPri);
    lcd.drawString(cyrStr(S().listTitle), SCREEN_W / 2,     HDR_H / 2);
    lcd.drawString(cyrStr(S().listTitle), SCREEN_W / 2 + 1, HDR_H / 2);

    // Счётчик ROM-ов справа
    if (romCount > 0) {
        char badge[12]; snprintf(badge, sizeof(badge), "%d ROMs", romCount);
        th_fsm(); lcd.setTextDatum(MR_DATUM); lcd.setTextColor(t.textSec);
        lcd.drawString(badge, SCREEN_W - 8, HDR_H / 2);
    }
}

// ════════════════════════════════════════════════════════════════════
// TAB BAR (нижняя панель) — светлый, иконки + подписи
// ════════════════════════════════════════════════════════════════════
static void ios_drawMenuBar() {
    const Theme565& t = getTheme();
    int by = TH_DPAD_Y;
    uint16_t hairline = t.shadow ? t.shadow : (uint16_t)0xC618;

    lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.header);
    lcd.drawFastHLine(0, by, SCREEN_W, hairline);   // hairline сверху

    int zw = SCREEN_W / 3;
    for (int i = 0; i < 3; i++) {
        int cx  = i * zw + zw / 2;
        int icy = by + 8;
        bool active = (i == 0);
        uint16_t c = active ? t.accent : t.textSec;

        if (i == 0) {
            th_iconGamepad(cx, icy + 4, c);
        } else if (i == 1) {
            th_iconClock(cx, icy + 4, c);
        } else {
            th_iconSystem(cx, icy + 4, c);
        }

        th_fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(c);
        if (i == 0)      lcd.drawString(cyrStr(S().play),        cx, by + 31);
        else if (i == 1) lcd.drawString(timeGetString().c_str(), cx, by + 31);
        else             lcd.drawString(cyrStr(S().setup),       cx, by + 31);
    }
}

// ════════════════════════════════════════════════════════════════════
// СТРОКИ ROM — плоские, hairline-разделитель, › disclosure
// ════════════════════════════════════════════════════════════════════
static void ios_drawRomRow(int romIdx, int slot, bool sel) {
    const Theme565& t = getTheme();
    int y  = M_HDR_H + slot * M_ROW_H;
    int cy = y + M_ROW_H / 2;
    uint16_t hairline = t.shadow ? t.shadow : (uint16_t)0xC618;

    uint16_t bg = sel ? t.selected : t.rowEven;
    lcd.fillRect(0, y, M_LIST_W - 1, M_ROW_H, bg);

    // Hairline-разделитель (кроме первого слота)
    if (slot > 0) lcd.drawFastHLine(16, y, M_LIST_W - 30, hairline);

    uint16_t tc = sel ? (t.selText ? t.selText : (uint16_t)0x0001) : t.textPri;
    th_fsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(tc);
    lcd.drawString(th_trimName(sdMgr.get(romIdx).name, 15).c_str(), 10, cy);

    // Звезда (favourite) справа
    bool isFav = GameStats::favCheck(sdMgr.get(romIdx).path.c_str());
    uint16_t sc = isFav ? TH_GOLD : (uint16_t)0x2965u;
    int sx = M_LIST_W - 11;
    lcd.fillTriangle(sx, cy-4, sx-3, cy+2, sx+3, cy+2, sc);
    lcd.fillTriangle(sx, cy+3, sx-3, cy-2, sx+3, cy-2, sc);
}

// ════════════════════════════════════════════════════════════════════
// РЕАКЦИЯ НА НАЖАТИЯ
// ════════════════════════════════════════════════════════════════════

static ThemeAction ios_onBarTap(int x, int /*y*/) {
    int zw = SCREEN_W / 3;
    int zone = x / zw;
    if (zone == 0) return TA_NONE;       // Games tab — нет доп. действия
    if (zone == 1) return TA_NONE;       // Clock tab — нет действия
    return TA_SETTINGS;                  // Settings tab
}

static ThemeAction ios_onHeaderTap(int /*x*/, int /*y*/) {
    return TA_NONE;
}

static ThemeAction ios_onRowTap(int /*row*/, int taps) {
    if (taps >= 2) return TA_ROMINFO;   // двойной тап → ROM Info
    return TA_NONE;
}

// ════════════════════════════════════════════════════════════════════
// ЧАСТИЧНОЕ ОБНОВЛЕНИЕ ВРЕМЕНИ
// ════════════════════════════════════════════════════════════════════
static void ios_timeTick() {
    const Theme565& t = getTheme();
    int by  = TH_DPAD_Y;
    int zw  = SCREEN_W / 3;
    int cx  = SCREEN_W / 2;   // центр средней вкладки

    // Очищаем только среднюю вкладку
    lcd.fillRect(zw + 1, by + 2, zw - 2, BTNBAR_H - 4, t.header);
    uint16_t c = t.textSec;
    th_iconClock(cx, by + 12, c);
    th_fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(c);
    lcd.drawString(timeGetString().c_str(), cx, by + 31);  // время — ASCII, cyrStr не нужен
}

// ════════════════════════════════════════════════════════════════════
// РЕГИСТРАЦИЯ
// ════════════════════════════════════════════════════════════════════
static const ThemePlugin PLUGIN_IOS = {
    "iOS",
    // bg=white  header=#F2F2F7  sel=gray  accent=iOSblue
    // style: FLAT|TABBAR = 0x09
    { 0xFFFF, 0xF79E, 0xFFFF, 0xFFFF, 0xD69A,
      0x0000, 0x8C72, 0x03DF, 0xF986, 0x362B,
      0x0000, 0xC618, 0x0001, 0x09, 0 },
    ios_drawHeader, ios_drawMenuBar, ios_drawRomRow,
    ios_onBarTap, ios_onHeaderTap, ios_onRowTap, ios_timeTick
};
THEME_REGISTER(PLUGIN_IOS)
