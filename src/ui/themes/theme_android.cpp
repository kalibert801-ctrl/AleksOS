// theme_android.cpp — Android Material Dark тема для AleksOS
//
// Элементы интерфейса и их действия:
//   App Bar (шапка):
//     ≡ hamburger (слева)     → открыть настройки (имитация navigation drawer)
//     Заголовок "NES GAMES"   → нет действия
//     ○ поиск (справа)        → показать ROM Info
//     ⋮ три точки (крайне справа) → открыть файловый менеджер
//   Bottom Navigation + FAB:
//     [🎮 PLAY] (иконка + подпись, первая зона) → нет доп. действия
//     [🕐 HH:MM] (средняя зона)                 → нет действия
//     [⚙ SETUP] (правая зона)                   → открыть настройки
//     FAB [▶] (кружок teal, правый край)        → запустить ROM
// ════════════════════════════════════════════════════════════════════
#include "theme_helpers.h"

// ════════════════════════════════════════════════════════════════════
// APP BAR (шапка)
// ════════════════════════════════════════════════════════════════════
static void and_drawHeader(int romCount) {
    const Theme565& t = getTheme();
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);

    // ≡ hamburger — 3 горизонтальные линии слева
    for (int li = 0; li < 3; li++)
        lcd.drawFastHLine(8, HDR_H / 2 - 5 + li * 5, 14, t.textPri);

    // Заголовок по центру
    th_fmd(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.textPri);
    lcd.drawString(cyrStr(S().listTitle), SCREEN_W / 2, HDR_H / 2);

    // ○ поиск: круг + маленькая рукоять
    lcd.drawCircle(SCREEN_W - 34, HDR_H / 2, 7, t.textPri);
    lcd.drawLine(SCREEN_W - 28, HDR_H / 2 + 5, SCREEN_W - 25, HDR_H / 2 + 8, t.textPri);

    // ⋮ три точки
    for (int di = 0; di < 3; di++)
        lcd.fillCircle(SCREEN_W - 10, HDR_H / 2 - 4 + di * 4, 1, t.textPri);

    // Elevation — тень внизу шапки
    lcd.drawFastHLine(0, HDR_H - 1, SCREEN_W, t.rowOdd);
}

// ════════════════════════════════════════════════════════════════════
// BOTTOM NAVIGATION + FAB
// ════════════════════════════════════════════════════════════════════
static void and_drawMenuBar() {
    const Theme565& t = getTheme();
    int by = TH_DPAD_Y;
    int cy = TH_DPAD_CY;

    lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.header);
    lcd.drawFastHLine(0, by, SCREEN_W, t.rowOdd);   // hairline сверху

    // FAB (Floating Action Button) — teal-кружок у правого края
    int fabX = SCREEN_W - 26, fabY = cy;
    lcd.fillCircle(fabX + 2, fabY + 2, 18, 0x0841);  // тень
    lcd.fillCircle(fabX,     fabY,     18, t.accent); // кнопка
    lcd.fillTriangle(fabX - 5, fabY - 6, fabX - 5, fabY + 6, fabX + 7, fabY, 0x0000); // ▶

    // 3 навигационных зоны (без зоны FAB справа)
    int zw = (SCREEN_W - 52) / 3;
    for (int i = 0; i < 3; i++) {
        int cx  = i * zw + zw / 2;
        int icy = by + 9;
        bool active = (i == 0);                   // первая зона активна (ROM list)
        uint16_t c  = active ? t.accent : t.textSec;

        if (i == 0) {
            th_iconGamepad(cx, icy + 4, c);
        } else if (i == 1) {
            th_iconClock(cx, icy + 4, c);
        } else {
            th_iconSystem(cx, icy + 4, c);
        }

        th_fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(c);
        if (i == 0)      lcd.drawString(cyrStr(S().play),            cx, by + 33);
        else if (i == 1) lcd.drawString(timeGetString().c_str(),    cx, by + 33);
        else             lcd.drawString(cyrStr(S().setup),          cx, by + 33);
    }
}

// ════════════════════════════════════════════════════════════════════
// СТРОКИ ROM — Material card-строки с акцент-баром
// ════════════════════════════════════════════════════════════════════
static void and_drawRomRow(int romIdx, int slot, bool sel) {
    const Theme565& t = getTheme();
    int y  = HDR_H + slot * ROW_H;
    int cy = y + ROW_H / 2;

    uint16_t bg = sel ? t.selected : ((slot % 2) ? t.rowOdd : t.rowEven);
    lcd.fillRect(4, y + 2, SCREEN_W - 8, ROW_H - 4, bg);

    if (sel) {
        // Вертикальный акцент-бар слева (teal)
        lcd.fillRect(4, y + 2, 3, ROW_H - 4, t.accent);
        // Тень внизу карточки
        lcd.drawFastHLine(6, y + ROW_H - 3, SCREEN_W - 12, t.shadow);
        // Иконка геймпада справа
        uint16_t stc = t.selText ? t.selText : TH_WHITE;
        th_iconControls(SCREEN_W - 24, cy, stc);
        th_fmd(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(stc);
        lcd.drawString(th_trimName(sdMgr.get(romIdx).name, 23).c_str(), 16, cy);
    } else {
        // Разделитель между карточками
        if (slot > 0) lcd.drawFastHLine(4, y + 2, SCREEN_W - 8, t.header);
        th_fmd(); lcd.setTextColor(t.textPri); lcd.setTextDatum(ML_DATUM);
        lcd.drawString(th_trimName(sdMgr.get(romIdx).name, 25).c_str(), 14, cy);
    }
}

// ════════════════════════════════════════════════════════════════════
// РЕАКЦИЯ НА НАЖАТИЯ
// ════════════════════════════════════════════════════════════════════

static ThemeAction and_onBarTap(int x, int /*y*/) {
    // FAB зона
    if (x >= SCREEN_W - 44) return TA_PLAY;

    // 3 навигационные зоны
    int zw = (SCREEN_W - 52) / 3;
    int zone = x / zw;
    if (zone == 0) return TA_NONE;       // PLAY zone — уже на экране игр
    if (zone == 1) return TA_NONE;       // clock zone — нет действия
    return TA_SETTINGS;                  // SETUP zone
}

static ThemeAction and_onHeaderTap(int x, int /*y*/) {
    if (x < 30)              return TA_SETTINGS;    // ≡ hamburger → настройки
    if (x > SCREEN_W - 45 &&
        x < SCREEN_W - 18)   return TA_ROMINFO;     // ○ поиск → ROM Info
    if (x >= SCREEN_W - 18)  return TA_FILEMANAGER; // ⋮ → файловый менеджер
    return TA_NONE;
}

static ThemeAction and_onRowTap(int /*row*/, int taps) {
    if (taps >= 2) return TA_ROMINFO;
    return TA_NONE;
}

// ════════════════════════════════════════════════════════════════════
// ЧАСТИЧНОЕ ОБНОВЛЕНИЕ ВРЕМЕНИ
// ════════════════════════════════════════════════════════════════════
static void and_timeTick() {
    const Theme565& t = getTheme();
    int by = TH_DPAD_Y;
    int zw = (SCREEN_W - 52) / 3;
    int cx = zw + zw / 2;   // центр второй (clock) зоны

    // Очищаем только среднюю иконку-зону
    lcd.fillRect(zw + 2, by + 2, zw - 4, BTNBAR_H - 4, t.header);
    uint16_t c = t.textSec;
    th_iconClock(cx, by + 13, c);
    th_fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(c);
    lcd.drawString(timeGetString().c_str(), cx, by + 33);
}

// ════════════════════════════════════════════════════════════════════
// РЕГИСТРАЦИЯ
// ════════════════════════════════════════════════════════════════════
static const ThemePlugin PLUGIN_ANDROID = {
    "Android",
    // bg=#121212  header=#1E1E1E  rowEven=#1E1E1E  rowOdd=#252525
    // sel=teal  selText≈black  style: CARD|APPBAR = 0x24
    { 0x1082, 0x18E3, 0x18E3, 0x2104, 0x06D8,
      0xFFFF, 0x8C51, 0x06D8, 0xF800, 0x4EF0,
      0x0000, 0x0000, 0x0001, 0x24, 0 },
    and_drawHeader, and_drawMenuBar, and_drawRomRow,
    and_onBarTap, and_onHeaderTap, and_onRowTap, and_timeTick
};
THEME_REGISTER(PLUGIN_ANDROID)
