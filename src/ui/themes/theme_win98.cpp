// theme_win98.cpp — Windows 98 тема для AleksOS
//
// Элементы интерфейса и их действия:
//   Title bar:
//     [_] (минимизировать) — нет действия
//     [□] (развернуть)     — нет действия
//     [X] (закрыть)        — нет действия в главном меню
//     Значок + "NES GAMES" — нет действия
//   Menu bar:
//     "File"  → открыть файловый менеджер (просмотр/переименование ROM)
//     "View"  → нет действия (зарезервировано для переключения вид)
//   Taskbar:
//     [▶ Start] → запустить выбранный ROM
//     Шестерёнка (трей) → открыть настройки
//     Часы → нет действия
// ════════════════════════════════════════════════════════════════════
#include "theme_helpers.h"

// ── Цвета Win98 ─────────────────────────────────────────────────
// t.bg      = 0xC618 (silver)
// t.header  = 0x0010 (navy blue)
// t.hilite  = 0xFFFF (white bevel edge)
// t.shadow  = 0x7BCF (dark gray bevel edge)

// ════════════════════════════════════════════════════════════════════
// ШАПКА: 20px синий title bar + 20px серый menu bar
// ════════════════════════════════════════════════════════════════════
static void w98_drawHeader(int romCount) {
    const Theme565& t = getTheme();

    // ── Title bar (y 0..19, navy) ─────────────────────────────────
    lcd.fillRect(0, 0, SCREEN_W, 20, t.header);

    // Значок: маленький белый прямоугольник (имитация папки/диска)
    lcd.fillRect(5, 5, 10, 10, 0xFFFF);
    lcd.fillRect(6, 7,  8,  6, t.header);
    lcd.drawFastHLine(8, 6, 4, 0xFFFF);

    // Заголовок окна белым
    th_fsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(0xFFFF);
    lcd.drawString(cyrStr(S().listTitle), 20, 10);

    // Кнопки [_][□][X] — raised 3D, 16×16 каждая, у правого края
    for (int bi = 0; bi < 3; bi++) {
        int bx = SCREEN_W - 54 + bi * 18;
        lcd.fillRect(bx, 2, 16, 16, t.bg);
        lcd.drawFastHLine(bx,      2,  16, t.hilite);
        lcd.drawFastVLine(bx,      2,  16, t.hilite);
        lcd.drawFastHLine(bx,      17, 16, t.shadow);
        lcd.drawFastVLine(bx + 15, 2,  16, t.shadow);
    }
    // _ (минимизировать)
    lcd.drawFastHLine(SCREEN_W - 54 + 4, 13, 8, 0x0000);
    // □ (развернуть)
    lcd.drawRect(SCREEN_W - 36 + 4, 5, 8, 8, 0x0000);
    // X (закрыть)
    int cx3 = SCREEN_W - 18;
    lcd.drawLine(cx3 + 4, 5, cx3 + 11, 12, 0x0000);
    lcd.drawLine(cx3 + 11, 5, cx3 + 4, 12, 0x0000);

    // ── Menu bar (y 20..39, silver) ───────────────────────────────
    lcd.fillRect(0, 20, SCREEN_W, 20, t.bg);
    lcd.drawFastHLine(0, 20, SCREEN_W, t.shadow);    // inner top shadow
    lcd.drawFastHLine(0, 39, SCREEN_W, t.shadow);    // bottom border dark
    lcd.drawFastHLine(0, 40, SCREEN_W, t.hilite);    // bottom border light

    th_fsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(t.textPri);
    lcd.drawString("File", 8,  30);
    lcd.drawString("View", 50, 30);

    // Счётчик ROM-ов справа в menu bar
    if (romCount > 0) {
        char badge[12]; snprintf(badge, sizeof(badge), "%d ROMs", romCount);
        lcd.setTextDatum(MR_DATUM); lcd.setTextColor(t.textSec);
        lcd.drawString(badge, SCREEN_W - 8, 30);
    }
}

// ════════════════════════════════════════════════════════════════════
// TASKBAR (нижняя панель)
// ════════════════════════════════════════════════════════════════════
static void w98_drawMenuBar() {
    const Theme565& t = getTheme();
    int by = TH_DPAD_Y;
    int cy = TH_DPAD_CY;

    // Серый фон
    lcd.fillRect(0, by, SCREEN_W, BTNBAR_H, t.bg);
    // Sunken groove по верхнему краю (2 линии: тёмная + светлая)
    lcd.drawFastHLine(0, by,     SCREEN_W, t.shadow);
    lcd.drawFastHLine(0, by + 1, SCREEN_W, t.hilite);

    // ── Кнопка [▶ Start] — raised 3D ─────────────────────────────
    lcd.fillRect(4, by + 4, 82, 36, t.bg);
    lcd.drawFastHLine(4,  by + 4,  82, t.hilite);    // top
    lcd.drawFastVLine(4,  by + 4,  36, t.hilite);    // left
    lcd.drawFastHLine(4,  by + 39, 82, t.shadow);    // bottom
    lcd.drawFastVLine(85, by + 4,  36, t.shadow);    // right
    lcd.drawFastHLine(5,  by + 5,  80, 0xE73C);      // inner top
    lcd.drawFastVLine(5,  by + 5,  34, 0xE73C);      // inner left

    // Windows 95/98 логотип (4 цветных квадрата)
    lcd.fillRect(10, by + 11, 13, 13, 0x0000);       // черная рамка
    lcd.fillRect(11, by + 12, 5, 5, 0x07E0);         // зелёный (верх-лево)
    lcd.fillRect(17, by + 12, 5, 5, 0xF800);         // красный  (верх-право)
    lcd.fillRect(11, by + 18, 5, 5, 0x001F);         // синий    (низ-лево)
    lcd.fillRect(17, by + 18, 5, 5, 0xFFE0);         // жёлтый   (низ-право)

    // Текст "Start" (двойной проход для жирности)
    th_fmd(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(0x0000);
    lcd.drawString("Start", 27, cy);
    lcd.drawString("Start", 28, cy);

    // Sunken разделитель после Start
    lcd.drawFastVLine(93, by + 4, 36, t.shadow);
    lcd.drawFastVLine(94, by + 4, 36, t.hilite);

    // Иконка настроек (системный трей) ~x=118
    th_iconSystem(118, cy, t.textPri);

    // Sunken разделитель перед часами
    lcd.drawFastVLine(SCREEN_W - 70, by + 4, 36, t.shadow);
    lcd.drawFastVLine(SCREEN_W - 69, by + 4, 36, t.hilite);

    // Часы в sunken-панели
    lcd.fillRect(SCREEN_W - 66, by + 4, 62, 36, t.bg);
    lcd.drawFastHLine(SCREEN_W - 66, by + 4,  62, t.shadow);
    lcd.drawFastVLine(SCREEN_W - 66, by + 4,  36, t.shadow);
    lcd.drawFastHLine(SCREEN_W - 66, by + 39, 62, t.hilite);
    lcd.drawFastVLine(SCREEN_W - 4,  by + 4,  36, t.hilite);
    th_fmd(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(0x0000);
    lcd.drawString(timeGetString().c_str(), SCREEN_W - 35, cy);
}

// ════════════════════════════════════════════════════════════════════
// СТРОКИ ROM — белые/светло-серые, синяя полная выборка
// ════════════════════════════════════════════════════════════════════
static void w98_drawRomRow(int romIdx, int slot, bool sel) {
    const Theme565& t = getTheme();
    int y  = M_HDR_H + slot * M_ROW_H;
    int cy = y + M_ROW_H / 2;

    uint16_t bg = sel ? t.selected : ((slot % 2) ? t.rowOdd : t.rowEven);
    lcd.fillRect(0, y, M_LIST_W - 1, M_ROW_H, bg);

    bool isFav = GameStats::favCheck(sdMgr.get(romIdx).path.c_str());
    String name = th_trimName(sdMgr.get(romIdx).name, isFav ? 14 : 16);

    if (sel) {
        lcd.drawFastHLine(0, y,               M_LIST_W - 1, t.hilite);
        lcd.drawFastHLine(0, y + M_ROW_H - 1, M_LIST_W - 1, t.shadow);
        uint16_t stc = t.selText ? t.selText : TH_WHITE;
        th_fsm(); lcd.setTextDatum(ML_DATUM); lcd.setTextColor(stc);
        lcd.drawString(name.c_str(), 6, cy);
    } else {
        lcd.drawFastHLine(0, y + M_ROW_H - 1, M_LIST_W - 1, t.shadow);
        th_fsm(); lcd.setTextColor(t.textPri); lcd.setTextDatum(ML_DATUM);
        lcd.drawString(name.c_str(), 6, cy);
    }

    // Звезда (favourite) справа
    uint16_t sc = isFav ? TH_GOLD : (uint16_t)0x2965u;
    int sx = M_LIST_W - 11;
    lcd.fillTriangle(sx, cy-4, sx-3, cy+2, sx+3, cy+2, sc);
    lcd.fillTriangle(sx, cy+3, sx-3, cy-2, sx+3, cy-2, sc);
}

// ════════════════════════════════════════════════════════════════════
// РЕАКЦИЯ НА НАЖАТИЯ
// ════════════════════════════════════════════════════════════════════

// Нижняя панель (taskbar)
static ThemeAction w98_onBarTap(int x, int /*y*/) {
    if (x <= 86)                          return TA_PLAY;       // [▶ Start]
    if (x >= 95 && x <= 140)             return TA_SETTINGS;   // трей / шестерёнка
    // Зона часов — нет действия
    return TA_NONE;
}

// Шапка (title bar + menu bar)
static ThemeAction w98_onHeaderTap(int x, int y) {
    if (y < 20) {
        // Title bar
        if (x >= SCREEN_W - 18)                  return TA_NONE;   // [X] в главном меню — нет действия
        if (x >= SCREEN_W - 36 && x < SCREEN_W - 18) return TA_NONE; // [□] — нет действия
        // [_] — нет действия
    } else {
        // Menu bar (y 20..39)
        if (x < 42)              return TA_FILEMANAGER;  // File → файловый менеджер
        if (x >= 44 && x < 80)  return TA_NONE;         // View → (зарезервировано)
    }
    return TA_NONE;
}

static ThemeAction w98_onRowTap(int /*row*/, int taps) {
    if (taps >= 2) return TA_ROMINFO;
    return TA_NONE;
}

// ════════════════════════════════════════════════════════════════════
// ЧАСТИЧНОЕ ОБНОВЛЕНИЕ ВРЕМЕНИ
// ════════════════════════════════════════════════════════════════════
static void w98_timeTick() {
    const Theme565& t = getTheme();
    int by = TH_DPAD_Y;
    // Очищаем только внутреннюю часть sunken clock panel
    lcd.fillRect(SCREEN_W - 65, by + 5, 61, 34, t.bg);
    th_fmd(); lcd.setTextDatum(MC_DATUM); lcd.setTextColor(0x0000);
    lcd.drawString(timeGetString().c_str(), SCREEN_W - 35, TH_DPAD_CY);
}

// ════════════════════════════════════════════════════════════════════
// РЕГИСТРАЦИЯ
// ════════════════════════════════════════════════════════════════════
static const ThemePlugin PLUGIN_WIN98 = {
    "Win98",
    // bg=silver  header=navy  rowEven=white  rowOdd=lightgray  sel=navy
    // textPri=black  textSec=gray  accent=navy  danger=red  ok=green
    // hilite=white  shadow=darkgray  selText=white  style=FLAT|BEVEL|TASKBAR
    { 0xC618, 0x0010, 0xFFFF, 0xEF7D, 0x0010,
      0x0000, 0x7BCF, 0x0010, 0xF800, 0x07E0,
      0xFFFF, 0x7BCF, 0xFFFF, 0x13, 0 },
    w98_drawHeader, w98_drawMenuBar, w98_drawRomRow,
    w98_onBarTap, w98_onHeaderTap, w98_onRowTap, w98_timeTick
};
THEME_REGISTER(PLUGIN_WIN98)
