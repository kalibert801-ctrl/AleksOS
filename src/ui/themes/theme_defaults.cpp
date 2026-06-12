// theme_defaults.cpp — базовые темы: Dark, Light, Green, Amber
// Все 4 темы используют одинаковые функции отрисовки.
// Различие только в цветовой палитре (поле colors).
//
// Чтобы удалить конкретную тему — закомментируйте её THEME_REGISTER().
// ════════════════════════════════════════════════════════════════════
#include "theme_helpers.h"

// ════════════════════════════════════════════════════════════════════
// ШАПКА (header)
// ════════════════════════════════════════════════════════════════════
static void def_drawHeader(int romCount) {
    const Theme565& t = getTheme();
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    // Акцент-линия внизу
    lcd.drawFastHLine(0, HDR_H - 1, SCREEN_W, t.accent);
    // Заголовок золотым цветом
    th_flg(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(TH_GOLD);
    lcd.drawString(cyrStr(S().listTitle), SCREEN_W / 2, HDR_H / 2);
    // Бейдж с количеством ROM-ов справа
    if (romCount > 0) {
        char badge[12]; snprintf(badge, sizeof(badge), "%d ROMs", romCount);
        th_fsm(); lcd.setTextDatum(MR_DATUM); lcd.setTextColor(t.textSec);
        lcd.drawString(badge, SCREEN_W - 8, HDR_H / 2);
    }
}

// ════════════════════════════════════════════════════════════════════
// НИЖНЯЯ ПАНЕЛЬ (menu bar)
// ════════════════════════════════════════════════════════════════════
static void def_drawMenuBar() {
    const Theme565& t = getTheme();
    int by = TH_DPAD_Y;
    int cy = TH_DPAD_CY;

    lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.header);
    lcd.drawFastHLine(0, by, SCREEN_W, TH_TOPBAR);

    // Кнопка PLAY (скруглённый прямоугольник)
    lcd.fillRoundRect(4, by + 5, TH_MBAR_PW - 8, 34, 8, t.selected);
    lcd.fillTriangle(18, cy - 7, 18, cy + 7, 28, cy, TH_GOLD);
    th_fmd(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(TH_WHITE);
    lcd.drawString(cyrStr(S().play), 32, cy);

    // Разделители
    lcd.drawFastVLine(TH_MBAR_TX, by + 6, BTNBAR_H - 12, 0x3186);
    lcd.drawFastVLine(TH_MBAR_SX, by + 6, BTNBAR_H - 12, 0x3186);

    // Время по центру
    th_flg(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(TH_CYAN);
    lcd.drawString(timeGetString().c_str(), (TH_MBAR_TX + TH_MBAR_SX) / 2, cy);

    // Иконка настроек + подпись справа
    int scx = TH_MBAR_SX + 16;
    th_iconSystem(scx, cy, t.textSec);
    th_fmd(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(t.textSec);
    lcd.drawString(cyrStr(S().setup), scx + 14, cy);
}

// ════════════════════════════════════════════════════════════════════
// СТРОКА ROM  (compact layout v14.72: M_ROW_H=24, M_LIST_W=196)
// ════════════════════════════════════════════════════════════════════
static void def_drawRomRow(int romIdx, int slot, bool sel) {
    const Theme565& t = getTheme();
    int y  = M_HDR_H + slot * M_ROW_H;
    int cy = y + M_ROW_H / 2;
    uint16_t stc = t.selText ? t.selText : TH_WHITE;

    bool isFav  = GameStats::favCheck(sdMgr.get(romIdx).path.c_str());
    uint16_t sc = isFav ? TH_GOLD : (uint16_t)0x2965u;  // золото : тёмно-серый

    // Максимальная длина имени: оставляем 18px для звезды
    int maxLen = sel ? 16 : 18;
    String name = th_trimName(sdMgr.get(romIdx).name, maxLen);

    if (sel) {
        lcd.fillRoundRect(1, y + 1, M_LIST_W - 18, M_ROW_H - 2, 4, t.selected);
        // Треугольник-плей
        lcd.fillTriangle(7, cy - 5, 7, cy + 5, 15, cy, TH_GOLD);
        th_fsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(stc);
        lcd.drawString(name.c_str(), 19, cy);
    } else {
        if (slot > 0) lcd.drawFastHLine(4, y, M_LIST_W - 22, TH_SEP);
        th_fsm(); lcd.setTextColor(t.textPri); lcd.setTextDatum(ML_DATUM);
        lcd.drawString(name.c_str(), 5, cy);
    }

    // Звёздочка (6-конечная: два треугольника) — правый край строки
    int sx = M_LIST_W - 11;
    // Верхний треугольник ▲
    lcd.fillTriangle(sx, cy - 4, sx - 3, cy + 2, sx + 3, cy + 2, sc);
    // Нижний треугольник ▼
    lcd.fillTriangle(sx, cy + 3, sx - 3, cy - 2, sx + 3, cy - 2, sc);
}

// ════════════════════════════════════════════════════════════════════
// РЕАКЦИЯ НА НАЖАТИЯ
// ════════════════════════════════════════════════════════════════════
static ThemeAction def_onBarTap(int x, int y) {
    if (x < TH_MBAR_TX)  return TA_PLAY;       // зона PLAY
    if (x >= TH_MBAR_SX) return TA_SETTINGS;   // зона SETUP
    return TA_NONE;                             // зона времени
}

static ThemeAction def_onHeaderTap(int /*x*/, int /*y*/) {
    return TA_NONE;
}

static ThemeAction def_onRowTap(int /*row*/, int taps) {
    if (taps >= 2) return TA_ROMINFO;  // двойной тап → ROM Info
    return TA_NONE;
}

// ════════════════════════════════════════════════════════════════════
// ОБНОВЛЕНИЕ ВРЕМЕНИ (раз в минуту)
// ════════════════════════════════════════════════════════════════════
static void def_timeTick() {
    const Theme565& t = getTheme();
    // Перерисовываем только зону времени
    int bx = TH_MBAR_TX + 1;
    int bw = TH_MBAR_SX - TH_MBAR_TX - 2;
    lcd.fillRect(bx, TH_DPAD_Y + 1, bw, BTNBAR_H - 2, t.header);
    th_flg(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(TH_CYAN);
    lcd.drawString(timeGetString().c_str(), (TH_MBAR_TX + TH_MBAR_SX) / 2, TH_DPAD_CY);
}

// ════════════════════════════════════════════════════════════════════
// РЕГИСТРАЦИЯ ПЛАГИНОВ
// Чтобы удалить тему — закомментируйте её THEME_REGISTER
// ════════════════════════════════════════════════════════════════════

// ── 0: Dark (RetroNight) ──────────────────────────────────────────
static const ThemePlugin THEME_DARK_PLUGIN = {
    "Dark",
    { 0x0863, 0x10C4, 0x08A4, 0x0884, 0x1A7B,
      0xE75E, 0x63B1, 0x3DFF, 0xFB8E, 0x4EF0,
      0x0000, 0x0000, 0xFFFF, 0x00, 0 },
    def_drawHeader, def_drawMenuBar, def_drawRomRow,
    def_onBarTap, def_onHeaderTap, def_onRowTap, def_timeTick
};
THEME_REGISTER(THEME_DARK_PLUGIN)

// ── 1: Light ──────────────────────────────────────────────────────
static const ThemePlugin THEME_LIGHT_PLUGIN = {
    "Light",
    { 0xFFFF, 0xC638, 0xFFFF, 0xDEDB, 0x435C,
      0x0000, 0x4208, 0x001F, 0xF800, 0x03E0,
      0x0000, 0x0000, 0xFFFF, 0x00, 0 },
    def_drawHeader, def_drawMenuBar, def_drawRomRow,
    def_onBarTap, def_onHeaderTap, def_onRowTap, def_timeTick
};
THEME_REGISTER(THEME_LIGHT_PLUGIN)

// ── 2: Green (Terminal) ───────────────────────────────────────────
static const ThemePlugin THEME_GREEN_PLUGIN = {
    "Green",
    { 0x0020, 0x0061, 0x0020, 0x0041, 0x02A0,
      0x07E0, 0x02E0, 0x07FF, 0xF800, 0x07E0,
      0x0000, 0x0000, 0xFFFF, 0x00, 0 },
    def_drawHeader, def_drawMenuBar, def_drawRomRow,
    def_onBarTap, def_onHeaderTap, def_onRowTap, def_timeTick
};
THEME_REGISTER(THEME_GREEN_PLUGIN)

// ── 3: Amber (Terminal) ───────────────────────────────────────────
static const ThemePlugin THEME_AMBER_PLUGIN = {
    "Amber",
    { 0x2000, 0x4820, 0x2000, 0x3000, 0xFD20,
      0xFDE0, 0x9240, 0xFD20, 0xF800, 0xFDE0,
      0x0000, 0x0000, 0xFFFF, 0x00, 0 },
    def_drawHeader, def_drawMenuBar, def_drawRomRow,
    def_onBarTap, def_onHeaderTap, def_onRowTap, def_timeTick
};
THEME_REGISTER(THEME_AMBER_PLUGIN)
