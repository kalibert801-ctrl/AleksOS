#pragma once
#ifdef __cplusplus
#include <cstring>   // strncpy в settingsDefault()
#endif

/*  settings.h — совместим и с C и с C++.
    НЕ включает Arduino.h/config.h напрямую.                        */

/* ── Перечисления ───────────────────────────────────────────────── */
// Theme enum удалён — темы теперь определяются по имени через ThemeRegistry.
// THEME_STYLE_* bitmask для Theme565.style остаётся без изменений.

/* ── Стиль темы (bitmask для Theme565.style) ──────────────── */
#define THEME_STYLE_FLAT     0x01   /* прямые углы вместо скруглённых                     */
#define THEME_STYLE_BEVEL    0x02   /* 3D-bevel: светлый top/left, тёмный bottom/right    */
#define THEME_STYLE_CARD     0x04   /* Android: ROM-строки как Material-карточки           */
#define THEME_STYLE_TABBAR   0x08   /* iOS/GameBoy: нижний бар как tab bar с иконками      */
#define THEME_STYLE_TASKBAR  0x10   /* Win98: taskbar + Start button внизу                 */
#define THEME_STYLE_APPBAR   0x20   /* Android: app bar с hamburger + иконками вверху      */
#define THEME_STYLE_PIXEL    0x40   /* GameBoy: двойная рамка, pixel-стиль                 */

typedef enum { LANG_RU=0, LANG_EN, LANG_CZ, LANG_COUNT } Language;
typedef enum { SCALE_FIT=0, SCALE_43, SCALE_11, SCALE_COUNT } Scale;
typedef enum { SND_BEEP=0, SND_CLICK, SND_CHIME, SND_COUNT } SoundType;

/* ── Структура настроек ─────────────────────────────────────────── */
typedef struct {
    char          themeName[24]; // имя активной темы: "Dark", "Win98", …
    Language      language;
    unsigned char brightness;
    unsigned char volume;
    unsigned char emuVolume;
    Scale         scale;
    unsigned char showFPS;
    unsigned char autoSave;
    unsigned char autoBrightness;
    unsigned char soundEnabled;
    SoundType     soundType;
    /* Переназначение кнопок */
    unsigned char btnMap[8];
    /* Вибро */
    unsigned char vibroEnabled;
    unsigned char vibroStrength;
    /* ── Время (обновляется time_manager) ── */
    unsigned char timeH;    // часы   0–23
    unsigned char timeM;    // минуты 0–59
    unsigned char autoScroll; // авто-прокрутка при удержании кнопки
    // ── Диагностика (вывод в Serial) ────────────────────────
    unsigned char diagButtons;  // нажатия кнопок
    unsigned char diagFPS;      // FPS и память эмулятора
    unsigned char diagEmu;      // запуск / выход эмулятора
    unsigned char diagTouch;    // координаты тача
    // ── WiFi ─────────────────────────────────────────────────
    char          wifiSSID[64]; // сохранённая точка доступа
    char          wifiPass[64]; // пароль
    unsigned char wifiEnabled;  // авто-подключение при старте
} Settings;

/* ── Цвета темы RGB565 ──────────────────────────────────────────── */
typedef struct {
    unsigned short bg, header, rowEven, rowOdd, selected;
    unsigned short textPri, textSec, accent, danger, ok;
    /* Дополнительные поля стиля */
    unsigned short hilite;  /* bevel: светлый край (top/left)          */
    unsigned short shadow;  /* bevel: тёмный край (bottom/right)        */
    unsigned short selText; /* цвет текста на выделенном bg (0xFFFF=бел) */
    unsigned char  style;   /* THEME_STYLE_* bitmask                    */
    unsigned char  _pad;
} Theme565;

/* ── C++ секция ─────────────────────────────────────────────────── */
#ifdef __cplusplus

extern Settings settings;

/* getTheme() — реализована в theme_registry.cpp.
   Возвращает цветовую палитру активного плагина темы.               */
const Theme565& getTheme();

inline void settingsDefault(Settings &s) {
    // Тема по умолчанию — Dark
    strncpy(s.themeName, "Dark", sizeof(s.themeName) - 1);
    s.themeName[sizeof(s.themeName) - 1] = '\0';

    s.language       = LANG_RU;
    s.brightness     = 80;
    s.volume         = 70;
    s.emuVolume      = 80;
    s.scale          = SCALE_43;
    s.showFPS        = 0;
    s.autoSave       = 1;
    s.autoBrightness = 0;
    s.soundEnabled   = 1;
    s.soundType      = SND_CLICK;
    for (int i = 0; i < 8; i++) s.btnMap[i] = (unsigned char)(1 << i);
    s.vibroEnabled  = 1;
    s.vibroStrength = 70;
    s.timeH         = 12;
    s.timeM         = 0;
    s.autoScroll    = 1;
    s.diagButtons   = 0;
    s.diagFPS       = 0;
    s.diagEmu       = 0;
    s.diagTouch     = 0;
    s.wifiSSID[0]   = '\0';
    s.wifiPass[0]   = '\0';
    s.wifiEnabled   = 0;
}

#endif /* __cplusplus */
