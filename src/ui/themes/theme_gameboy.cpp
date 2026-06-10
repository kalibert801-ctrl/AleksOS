// theme_gameboy.cpp — Game Boy DMG тема для AleksOS
//
// Элементы интерфейса и их действия:
//   Шапка (header):
//     Двойной пиксельный бордер + заголовок
//     (без интерактивных элементов — только отображение)
//   Нижняя панель:
//     "A:PLAY"   (левая треть)  → запустить ROM
//     "HH:MM"    (центр)        → нет действия
//     "STA:MENU" (правая треть) → открыть настройки
//   Строки ROM:
//     Пиксельный ▶ курсор + двойные рамки
//     Двойной тап → ROM Info
// ════════════════════════════════════════════════════════════════════
#include "theme_helpers.h"

// ════════════════════════════════════════════════════════════════════
// ШАПКА — GameBoy пиксельный стиль, толстые бордеры
// ════════════════════════════════════════════════════════════════════
static void gb_drawHeader(int romCount) {
    const Theme565& t = getTheme();

    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);

    // Верхний толстый бордер (4px, тёмно-зелёный)
    lcd.fillRect(0, 0, SCREEN_W, 4, t.textPri);
    // Акцент-линии под бордером
    lcd.drawFastHLine(0, 4, SCREEN_W, t.accent);
    lcd.drawFastHLine(0, 5, SCREEN_W, t.accent);

    // Боковые бордеры (4px)
    lcd.fillRect(0,          0, 4, HDR_H, t.textPri);
    lcd.fillRect(SCREEN_W - 4, 0, 4, HDR_H, t.textPri);

    // Двойной нижний бордер
    lcd.drawFastHLine(0, HDR_H - 2, SCREEN_W, t.textPri);
    lcd.drawFastHLine(0, HDR_H - 1, SCREEN_W, t.accent);

    // Заголовок акцентным цветом (зелёный DMG)
    th_fmd(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.accent);
    lcd.drawString(cyrStr(S().listTitle), SCREEN_W / 2, HDR_H / 2);

    // Счётчик ROM-ов в пикселях (внутри рамок)
    if (romCount > 0) {
        char badge[6]; snprintf(badge, sizeof(badge), "%d", romCount);
        th_fsm(); lcd.setTextDatum(MR_DATUM); lcd.setTextColor(t.textPri);
        lcd.drawString(badge, SCREEN_W - 8, HDR_H / 2);
    }
}

// ════════════════════════════════════════════════════════════════════
// НИЖНЯЯ ПАНЕЛЬ — пиксельные хинты: A:PLAY | HH:MM | STA:MENU
// ════════════════════════════════════════════════════════════════════
static void gb_drawMenuBar() {
    const Theme565& t = getTheme();
    int by = TH_DPAD_Y;
    int cy = TH_DPAD_CY;

    // Тёмный (selected) фон панели
    lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.selected);

    // Двойная верхняя рамка
    lcd.drawFastHLine(0, by,     SCREEN_W, t.textPri);
    lcd.drawFastHLine(0, by + 1, SCREEN_W, t.textPri);
    lcd.drawFastHLine(0, by + 2, SCREEN_W, t.accent);

    // Двойные вертикальные разделители
    for (int xi = 1; xi <= 2; xi++) {
        int vx = SCREEN_W * xi / 3;
        lcd.drawFastVLine(vx,     by + 4, BTNBAR_H - 8, t.textPri);
        lcd.drawFastVLine(vx + 1, by + 4, BTNBAR_H - 8, t.accent);
    }

    // Текстовые подсказки в каждой трети
    th_fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.accent);
    lcd.drawString(String("A:")   + cyrStr(S().play),  SCREEN_W / 6,     cy);
    lcd.drawString(timeGetString().c_str(),             SCREEN_W / 2,     cy);
    lcd.drawString(String("STA:") + cyrStr(S().setup), SCREEN_W * 5 / 6, cy);
}

// ════════════════════════════════════════════════════════════════════
// СТРОКИ ROM — 4-colour palette, двойные рамки, пиксельный ▶ курсор
// ════════════════════════════════════════════════════════════════════
static void gb_drawRomRow(int romIdx, int slot, bool sel) {
    const Theme565& t = getTheme();
    int y  = HDR_H + slot * ROW_H;
    int cy = y + ROW_H / 2;

    // Фон строки чередующийся
    uint16_t bg = (slot % 2) ? t.rowOdd : t.rowEven;
    lcd.fillRect(0, y, SCREEN_W, ROW_H, bg);

    // Боковые двойные бордеры (4px + 5px)
    lcd.drawFastVLine(4, y, ROW_H, t.textPri);
    lcd.drawFastVLine(5, y, ROW_H, t.accent);
    lcd.drawFastVLine(SCREEN_W - 5, y, ROW_H, t.textPri);
    lcd.drawFastVLine(SCREEN_W - 6, y, ROW_H, t.accent);

    // Нижний двойной разделитель (кроме последней строки)
    if (slot < ROWS_VISIBLE - 1) {
        lcd.drawFastHLine(0, y + ROW_H - 2, SCREEN_W, t.textPri);
        lcd.drawFastHLine(0, y + ROW_H - 1, SCREEN_W, t.accent);
    }

    if (sel) {
        // Выбранная строка: тёмный фон внутри рамок
        lcd.fillRect(6, y + 1, SCREEN_W - 12, ROW_H - 3, t.selected);

        // Пиксельный ▶ курсор (горизонтальные линии разной длины)
        for (int pi = 0; pi < 5; pi++)
            lcd.drawFastHLine(8, cy - 2 + pi, 1 + pi / 2, t.accent);

        th_fmd(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(t.accent);
        lcd.drawString(th_trimName(sdMgr.get(romIdx).name, 22).c_str(), 19, cy);
    } else {
        th_fmd(); lcd.setTextColor(t.textPri); lcd.setTextDatum(ML_DATUM);
        lcd.drawString(th_trimName(sdMgr.get(romIdx).name, 24).c_str(), 12, cy);
    }
}

// ════════════════════════════════════════════════════════════════════
// РЕАКЦИЯ НА НАЖАТИЯ
// ════════════════════════════════════════════════════════════════════

static ThemeAction gb_onBarTap(int x, int /*y*/) {
    int zw = SCREEN_W / 3;
    if (x < zw)             return TA_PLAY;       // A:PLAY
    if (x >= 2 * zw)        return TA_SETTINGS;   // STA:MENU
    return TA_NONE;                               // время — нет действия
}

static ThemeAction gb_onHeaderTap(int /*x*/, int /*y*/) {
    return TA_NONE;
}

static ThemeAction gb_onRowTap(int /*row*/, int taps) {
    if (taps >= 2) return TA_ROMINFO;
    return TA_NONE;
}

// ════════════════════════════════════════════════════════════════════
// ЧАСТИЧНОЕ ОБНОВЛЕНИЕ ВРЕМЕНИ
// ════════════════════════════════════════════════════════════════════
static void gb_timeTick() {
    const Theme565& t = getTheme();
    int by  = TH_DPAD_Y;
    int zw  = SCREEN_W / 3;
    // Очищаем только центральную треть (между разделителями)
    lcd.fillRect(zw + 2, by + 4, zw - 4, BTNBAR_H - 8, t.selected);
    th_fsm(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(t.accent);
    lcd.drawString(timeGetString().c_str(), SCREEN_W / 2, TH_DPAD_CY);
}

// ════════════════════════════════════════════════════════════════════
// РЕГИСТРАЦИЯ
// ════════════════════════════════════════════════════════════════════
static const ThemePlugin PLUGIN_GAMEBOY = {
    "GameBoy",
    // 4-colour DMG palette: #9BBC0F #8BAC0F #306230 #0F380F
    // style: FLAT|TABBAR|PIXEL = 0x49
    { 0x9DE1, 0x3306, 0x9DE1, 0x8D21, 0x3306,
      0x09C1, 0x3306, 0x9DE1, 0x09C1, 0x3306,
      0x9DE1, 0x09C1, 0x9DE1, 0x49, 0 },
    gb_drawHeader, gb_drawMenuBar, gb_drawRomRow,
    gb_onBarTap, gb_onHeaderTap, gb_onRowTap, gb_timeTick
};
THEME_REGISTER(PLUGIN_GAMEBOY)
