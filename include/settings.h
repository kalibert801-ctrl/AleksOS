#pragma once

/*  settings.h — совместим и с C и с C++.
    НЕ включает Arduino.h/config.h напрямую.                        */

/* ── Перечисления ───────────────────────────────────────────────── */
typedef enum { THEME_DARK=0, THEME_LIGHT, THEME_GREEN, THEME_AMBER, THEME_COUNT } Theme;
typedef enum { LANG_RU=0, LANG_EN, LANG_COUNT } Language;
typedef enum { SCALE_FIT=0, SCALE_43, SCALE_11, SCALE_COUNT } Scale;
typedef enum { SND_BEEP=0, SND_CLICK, SND_CHIME, SND_COUNT } SoundType;

/* ── Структура настроек ─────────────────────────────────────────── */
typedef struct {
    Theme         theme;
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
} Settings;

/* ── Цвета темы RGB565 ──────────────────────────────────────────── */
typedef struct {
    unsigned short bg, header, rowEven, rowOdd, selected;
    unsigned short textPri, textSec, accent, danger, ok;
} Theme565;

/* ── C++ секция ─────────────────────────────────────────────────── */
#ifdef __cplusplus

extern Settings settings;
extern const Theme565 THEMES[THEME_COUNT];

inline const Theme565& getTheme() {
    return THEMES[(int)settings.theme % (int)THEME_COUNT];
}

inline void settingsDefault(Settings &s) {
    s.theme          = THEME_DARK;
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
}

#endif /* __cplusplus */
