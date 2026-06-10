#pragma once
// ════════════════════════════════════════════════════════════════════
//  theme_plugin.h — модульная система тем AleksOS
//
//  Как добавить тему:
//    1. Создать src/ui/themes/theme_mythem.cpp
//    2. Реализовать 7 функций (drawHeader, drawMenuBar, drawRomRow,
//       onBarTap, onHeaderTap, onRowTap, timeTick)
//    3. Описать ThemePlugin и в конце файла написать THEME_REGISTER(VAR)
//    4. PlatformIO автоматически скомпилирует новый файл
//
//  Как удалить тему:
//    Просто удалить файл — проект не упадёт.
//    Если удалённая тема была активной — система переключится на первую
//    зарегистрированную тему.
// ════════════════════════════════════════════════════════════════════
#ifdef __cplusplus

#include "settings.h"   // Theme565, THEME_STYLE_*
#include "config.h"     // BTN_A / BTN_B / BTN_SEL / BTN_LEFT / BTN_RIGHT
#include <stdint.h>

// ── Семантические коды действий ───────────────────────────────────
enum ThemeAct : uint8_t {
    ACT_NONE        = 0,
    ACT_PLAY        = 1,    // запустить выбранный ROM
    ACT_SETTINGS    = 2,    // открыть настройки
    ACT_BACK        = 3,    // назад / закрыть
    ACT_FILEMANAGER = 4,    // открыть файловый менеджер (Win98 File)
    ACT_TOGGLE_VIEW = 5,    // переключить вид (будущее)
    ACT_ROMINFO     = 6,    // показать ROM Info
};

// ── Результат обработки нажатия ───────────────────────────────────
struct ThemeAction {
    ThemeAct act;
    uint8_t  btnMask;   // BTN_* для совместимости с main.cpp
};

// Удобные константы-инициализаторы
#define TA_NONE        (ThemeAction{ACT_NONE,        0x00    })
#define TA_PLAY        (ThemeAction{ACT_PLAY,         BTN_A  })
#define TA_SETTINGS    (ThemeAction{ACT_SETTINGS,     BTN_SEL})
#define TA_BACK        (ThemeAction{ACT_BACK,         BTN_B  })
#define TA_FILEMANAGER (ThemeAction{ACT_FILEMANAGER,  BTN_LEFT })
#define TA_ROMINFO     (ThemeAction{ACT_ROMINFO,      BTN_RIGHT})

// ════════════════════════════════════════════════════════════════════
//  Интерфейс плагина темы
// ════════════════════════════════════════════════════════════════════
struct ThemePlugin {
    const char*  name;      // уникальное имя: "Dark", "Win98", …
    Theme565     colors;    // цветовая палитра RGB565

    // ── Отрисовка ────────────────────────────────────────────────
    // drawHeader  : шапка главного меню (y 0..HDR_H-1)
    // drawMenuBar : нижняя панель (y DPAD_Y..SCREEN_H-1)
    // drawRomRow  : одна строка ROM; sel=true если строка выбрана
    void         (*drawHeader)(int romCount);
    void         (*drawMenuBar)(void);
    void         (*drawRomRow)(int romIdx, int slot, bool sel);

    // ── Реакция на нажатие ───────────────────────────────────────
    // onBarTap    : нажатие в нижней панели
    // onHeaderTap : нажатие в шапке (app bar / title bar / nav bar)
    // onRowTap    : нажатие на строку ROM (taps = 1 или 2)
    ThemeAction  (*onBarTap)   (int x, int y);
    ThemeAction  (*onHeaderTap)(int x, int y);
    ThemeAction  (*onRowTap)   (int row, int taps);

    // ── Частичное обновление времени (раз в минуту) ──────────────
    void         (*timeTick)(void);
};

// ════════════════════════════════════════════════════════════════════
//  Реестр тем — использует «construct on first use» для безопасной
//  авто-регистрации через статические инициализаторы
// ════════════════════════════════════════════════════════════════════
class ThemeRegistry {
public:
    static constexpr int MAX = 24;

    // Вызывается из THEME_REGISTER() при старте программы
    static void               reg(const ThemePlugin* p);

    // Доступ к зарегистрированным темам
    static int                count();
    static const ThemePlugin* get(int idx);            // 0..count()-1
    static const ThemePlugin* getByName(const char* name);

    // Активная тема
    static const ThemePlugin* active();
    static int                activeIndex();           // 0-based
    static bool               setActive(int idx);
    static bool               setActiveByName(const char* name);
    static const char*        activeName();
};

// ── Макрос авто-регистрации (в конец каждого theme_*.cpp) ────────
//  THEME_REGISTER(MY_PLUGIN)  ← MY_PLUGIN — имя переменной ThemePlugin
#define THEME_REGISTER(var) \
    namespace { \
        static const bool _treg_##var = (ThemeRegistry::reg(&var), true); \
    }

// ── getTheme() — совместимость со всем кодом проекта ────────────
//   Реализована в theme_registry.cpp.
//   Возвращает colors активного плагина (или встроенную тёмную палитру).
const Theme565& getTheme();

#endif // __cplusplus
