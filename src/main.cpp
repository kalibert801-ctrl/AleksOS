// main.cpp — RetroESP AleksOS

#include <Arduino.h>
#include "config.h"
#include "settings.h"
#include "display/display_manager.h"
#include "display/led.h"
#include "boot_screen.h"
#include "storage/sd_manager.h"
#include "storage/config_manager.h"
#include "input/touch_handler.h"
#include "input/button_handler.h"
#include "input/audio.h"
#include "system/time_manager.h"
#include "system/battery.h"
#include "ui/ui.h"
#include "lang.h"
#include "emulator/emu_runner.h"
#include "network/wifi_manager.h"
#include "network/ntp_manager.h"
#include "network/ota_manager.h"
#include "network/pico_ota.h"
#include "network/bt_manager.h"
#include "network/web_manager.h"
#include "network/web_console.h"
#include "storage/game_stats.h"
#include "emulator/game_genie.h"
#include "emulator/game_shark.h"

#include "ui/sdmgr.h"

Settings settings;

// ─────────────────────────────────────────────────────────────
// STATE MACHINE
// ─────────────────────────────────────────────────────────────

enum State { S_MENU, S_SETTINGS, S_REMAP, S_PLAYING, S_WIFI, S_WIFI_KB,
             S_FILEMGR, S_FILEMGR_KB, S_GG, S_GG_KB, S_GS, S_GS_KB,
             S_SEARCH_KB, S_WEBMGR, S_SDMGR };
static State state = S_MENU;
static bool  _settingsFromMenu = false;  // opened from main screen footer tap → Back returns to menu

// ── Screen-transition helpers ─────────────────────────────────────────────
// fadeOut: dim to black in 5 steps × 10 ms ≈ 50 ms
// fadeIn : restore brightness in 5 steps × 10 ms ≈ 50 ms
// Total transition: ~100 ms — fast enough to feel instant, slow enough to look smooth.

static void fadeOut() {
    int start = (settings.brightness > 0) ? settings.brightness : 80;
    for (int i = 5; i >= 0; i--) {
        lcd.setBrightness((uint8_t)map(start * i / 5, 0, 100, 0, 255));
        delay(10);
    }
}

static void fadeIn() {
    int target = (settings.brightness > 0) ? settings.brightness : 80;
    for (int i = 0; i <= 5; i++) {
        lcd.setBrightness((uint8_t)map(target * i / 5, 0, 100, 0, 255));
        delay(10);
    }
    // Ensure exact target brightness
    lcd.setBrightness((uint8_t)map(target, 0, 100, 0, 255));
}

static void toMenu()     { fadeOut(); state = S_MENU;     menuDraw();          fadeIn(); }
static void toSettings() { fadeOut(); state = S_SETTINGS; settingsDraw();      fadeIn(); }
static void toRemap()    { fadeOut(); state = S_REMAP;    btnMapDraw();        fadeIn(); }
static void toWifi()     { fadeOut(); state = S_WIFI;     wifiManagerDraw();   fadeIn(); }
static void toFileMgr()  { fadeOut(); state = S_FILEMGR;  fileMgrDraw();       fadeIn(); }
static void toGG()       { fadeOut(); state = S_GG;        ggScreenDraw();     fadeIn(); }
static void toGS()       { fadeOut(); state = S_GS;        gsScreenDraw();     fadeIn(); }


// WEB Manager screen — рисуется inline при переходе
static void drawWebScreen() {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);
    lcd.fillRect(0, 0, SCREEN_W, 34, t.header);
    lcd.setFont(&lgfx::fonts::DejaVu18);
    lcd.setTextDatum(MC_DATUM); lcd.setTextColor(0xFD20u);
    lcd.drawString("WEB MANAGER", SCREEN_W / 2, 17);

    lcd.setFont(&lgfx::fonts::DejaVu12);
    lcd.setTextColor(t.textSec); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("Open in browser:", SCREEN_W / 2, 60);

    lcd.setFont(&lgfx::fonts::DejaVu18);
    lcd.setTextColor(0x07FFu);
    lcd.drawString((String("http://") + webMgrIP()).c_str(), SCREEN_W / 2, 90);

    lcd.setFont(&lgfx::fonts::DejaVu12);
    lcd.setTextColor(t.textSec);
    lcd.drawString("Manage files, upload ROMs", SCREEN_W / 2, 130);
    lcd.drawString("Music, Sounds, Firmware", SCREEN_W / 2, 148);
    lcd.drawString("Both devices on same WiFi", SCREEN_W / 2, 170);

    int by = SCREEN_H - 44;
    lcd.fillRect(0, by, SCREEN_W, 44, t.header);
    lcd.drawFastHLine(0, by, SCREEN_W, t.accent);
    lcd.setTextColor(t.danger); lcd.setTextDatum(MC_DATUM);
    lcd.drawString("Reset = Stop server & back", SCREEN_W / 2, by + 22);
}

static void toWebMgr() {
    if (!WiFi.isConnected()) {
        if (!settings.wifiEnabled || settings.wifiSSID[0] == '\0') {
            popupShow("Web Manager", "No WiFi configured!\nSet up WiFi in Settings first.", 3000);
            return;
        }
        // Показываем экран подключения
        const Theme565 &t = getTheme();
        lcd.fillScreen(t.bg);
        lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
        lcd.setFont(&lgfx::fonts::DejaVu18);
        lcd.setTextDatum(MC_DATUM); lcd.setTextColor(0xFD20u);
        lcd.drawString("WEB MANAGER", SCREEN_W / 2, HDR_H / 2);
        lcd.setFont(&lgfx::fonts::DejaVu12);
        lcd.setTextColor(t.textSec); lcd.setTextDatum(MC_DATUM);
        lcd.drawString("Connecting to WiFi...", SCREEN_W / 2, 120);

        WiFi.mode(WIFI_STA);
        if (!wifiMgr.connect(settings.wifiSSID, settings.wifiPass, 12000)) {
            popupShow("Web Upload", "WiFi connection failed!", 3000);
            WiFi.mode(WIFI_OFF);
            return;
        }
    }
    fadeOut();
    state = S_WEBMGR;
    webMgrStart();
    drawWebScreen();
    fadeIn();
}


static void toSdMgr() {
    fadeOut();
    state = S_SDMGR;
    sdMgrOpen();
    fadeIn();
}

// ── Web Debug: включить/выключить консоль в фоне ─────────────────────────────
static void toggleWebDebug() {
    if (webDbgRunning()) {
        webDbgStop();
        WiFi.mode(WIFI_OFF);
        settingsDraw();
        popupShow("Web Debug", "Console stopped.", 1500);
        settingsDraw();
        return;
    }
    if (!settings.wifiSSID[0]) {
        popupShow("Web Debug", "No WiFi configured!\nSet WiFi in Settings.", 3000);
        settingsDraw();
        return;
    }
    if (!WiFi.isConnected()) {
        const Theme565 &t = getTheme();
        lcd.fillScreen(t.bg);
        lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
        lcd.setFont(&lgfx::fonts::DejaVu18);
        lcd.setTextDatum(MC_DATUM); lcd.setTextColor(0xFD20u);
        lcd.drawString("WEB DEBUG", SCREEN_W / 2, HDR_H / 2);
        lcd.setFont(&lgfx::fonts::DejaVu12);
        lcd.setTextColor(t.textSec);
        lcd.drawString("Connecting to WiFi...", SCREEN_W / 2, 120);
        WiFi.mode(WIFI_STA);
        if (!wifiMgr.connect(settings.wifiSSID, settings.wifiPass, 12000)) {
            popupShow("Web Debug", "WiFi failed!", 2000);
            WiFi.mode(WIFI_OFF);
            settingsDraw();
            return;
        }
    }
    webDbgStart();
    // Показываем экран с адресом — пользователь запоминает и нажимает кнопку
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    lcd.setFont(&lgfx::fonts::DejaVu18);
    lcd.setTextDatum(MC_DATUM); lcd.setTextColor(0xFD20u);
    lcd.drawString("WEB DEBUG ON", SCREEN_W / 2, HDR_H / 2);
    lcd.setFont(&lgfx::fonts::DejaVu12);
    lcd.setTextColor(t.textPri);
    lcd.drawString("Open in browser:", SCREEN_W / 2, 90);
    lcd.setFont(&lgfx::fonts::DejaVu18);
    lcd.setTextColor(t.accent);
    char url[40]; snprintf(url, sizeof(url), "http://%s/remote", webDbgIP());
    lcd.drawString(url, SCREEN_W / 2, 120);
    lcd.setFont(&lgfx::fonts::DejaVu12);
    lcd.setTextColor(t.textSec);
    lcd.drawString("Press any button to continue", SCREEN_W / 2, 165);
    lcd.drawString("Console works while you play!", SCREEN_W / 2, 185);
    // Ждём нажатия кнопки
    delay(500);
    while (!buttons.read()) { webDbgHandle(); delay(50); }
    settingsDraw();
}

// WiFi manager keyboard helper — opens password keyboard for selected network
static void openWifiKeyboard() {
    wifiKeyboardReset();
    const char *ssid = wifiManagerSelectedSSID();
    state = S_WIFI_KB;
    wifiKeyboardDraw(ssid);
}

// Connect with current keyboard input, update settings, NTP sync
static void doWifiConnect() {
    const char *ssid = wifiManagerSelectedSSID();
    const char *pass = wifiKeyboardGetPassword();

    // Save credentials
    strncpy(settings.wifiSSID, ssid, sizeof(settings.wifiSSID)-1);
    settings.wifiSSID[sizeof(settings.wifiSSID)-1] = '\0';
    strncpy(settings.wifiPass, pass, sizeof(settings.wifiPass)-1);
    settings.wifiPass[sizeof(settings.wifiPass)-1] = '\0';

    // Show "Connecting..." popup style
    const Theme565 &t = getTheme();
    lcd.fillRect(30, 90, 260, 60, t.header);
    lcd.drawRoundRect(30, 90, 260, 60, 8, t.accent);
    lcd.setFont(&lgfx::fonts::DejaVu12);
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(t.textPri);
    lcd.drawString("Connecting...", SCREEN_W/2, 120);

    bool ok = wifiMgr.connect(ssid, pass, 12000);
    if (ok) {
        settings.wifiEnabled = 1;
        cfgSave();
        ntpSync();
        popupShow("WiFi", (String("Connected: ") + ssid).c_str(), 3000);
    } else {
        popupShow("WiFi", "Connection failed.", 3000);
    }
    toWifi();
}

static void showError(const char *title, const char *l1, const char *l2 = nullptr);
static void runEmulator(int idx);

// ─────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    webConsoleInit();
    webConsolePrintf("[SYS] AleksOS %s  Core=%d\n", FIRMWARE_VERSION, xPortGetCoreID());

    if (psramFound()) {
        webConsolePrintf("[PSRAM] OK — total: %u KB, free: %u KB\n",
            ESP.getPsramSize()  / 1024,
            ESP.getFreePsram()  / 1024);
    } else {
        webConsolePrintf("[PSRAM] NOT FOUND\n");
    }

    settingsDefault(settings);
    ledInit();
    initDisplay();

    // ── SD карта + boot logo ──────────────────────────────────
    // SD is initialised first so bootLogoLoad() can read /boot.raw.
    bool sdOk = sdMgr.init();
    bootLogoLoad();          // loads image into PSRAM (no-op if missing)
    bootScreenRun();         // show image (or fallback animation)

    if (sdOk) {
        bootProgress(30, "Loading config...");
        cfgLoad();
        GameStats::load();
        // Активируем сохранённую тему (или "Dark" если не найдена)
        if (!ThemeRegistry::setActiveByName(settings.themeName)) {
            ThemeRegistry::setActive(0);   // fallback на первую
            strncpy(settings.themeName, ThemeRegistry::activeName(),
                    sizeof(settings.themeName)-1);
        }
        // Диагностика: печатаем что загрузилось из конфига
        Serial.printf("[CFG] btnMap: A=%02X B=%02X SEL=%02X STA=%02X UP=%02X DN=%02X LT=%02X RT=%02X\n",
            settings.btnMap[0], settings.btnMap[1], settings.btnMap[2], settings.btnMap[3],
            settings.btnMap[4], settings.btnMap[5], settings.btnMap[6], settings.btnMap[7]);
        Serial.printf("[CFG] identity=%s  (A=01 B=02 SEL=04 STA=08 UP=10 DN=20 LT=40 RT=80)\n",
            (settings.btnMap[0]==0x01 && settings.btnMap[1]==0x02 &&
             settings.btnMap[2]==0x04 && settings.btnMap[3]==0x08) ? "YES" : "NO ← MISMATCH!");
        setBrightness(settings.brightness);
        ledSet(LED_GREEN);

        bootProgress(60, "Scanning ROMs...");
        for (int i = 0; i < 6; i++) { bootTick(); delay(30); }

        char buf[32];
        snprintf(buf, sizeof(buf), "Found %d ROMs", sdMgr.count());
        bootProgress(75, buf);

        // ── Предгенерация thumbnail для обложек игр ───────────────────────
        // При первом запуске генерирует .thm (27KB) для каждого .raw (150KB).
        // На последующих загрузках — мгновенный возврат (все .thm уже есть).
        uiPreCacheThumbnails([](int cur, int total) {
            char msg[36];
            snprintf(msg, sizeof(msg), "Cover art %d / %d", cur + 1, total);
            bootProgress((uint8_t)(76 + (cur * 11) / (total > 0 ? total : 1)), msg);
        });
    } else {
        ledSet(LED_RED);
        bootProgress(20, "SD error! No card?");
        for (int i = 0; i < 12; i++) { bootTick(); delay(50); }
    }

    // ── Периферия ─────────────────────────────────────────────
    bootProgress(88, "Init audio...");
    audioInit();
    batteryStatsLoad();  // загрузить session/total/cycles/pct из /battery_stats.json ПЕРЕД init
    batteryInit();       // ADC1 GPIO35, делитель 470k+470k (safe с WiFi)

    bootProgress(93, "Init touch...");
    touch.init();

    bootProgress(97, "Init buttons...");
    buttons.init();

    bootProgress(98, "Reading Pico fw...");
    settingsPrefetchPicoVer();
    // Синхронизация настройки виброотклика кнопок с Pico
    buttons.picoHapticEnable(settings.vibroEnabled);

    // ── Время (читает settings.timeH/timeM после cfgLoad) ─────
    timeInit();

    // ── WiFi (только для NTP синхронизации, потом выключается) ───
    bootProgress(97, "Init WiFi...");
    wifiMgr.init();
    if (wifiMgr.isConnected()) {
        bootProgress(99, "NTP sync...");
        ntpSync();
        // Выключаем WiFi — он нужен только для NTP и OTA.
        // При обновлениях reconnect происходит автоматически в otaScreen().
        WiFi.mode(WIFI_OFF);
        Serial.println("[WiFi] Off after NTP sync.");
    }

    // ── Готово ────────────────────────────────────────────────
    bootProgress(100, "AleksOS Ready!");
    ledSet(LED_OFF);
    bootScreenDone();

    toMenu();
    if (sdOk) { soundOK(); buttons.vibrate1(120); }
}

// ─────────────────────────────────────────────────────────────
void loop() {
    ledUpdate();
    audioUpdate();
    updateAutoBrightness();
    buttons.update();
    if (webDbgRunning()) webDbgHandle();   // фоновый debug-сервер работает в любом состоянии

    // btnPhys  — сырые физические кнопки (для экрана ремапа, где нужен физ. ввод)
    // btnNew   — кнопки после применения таблицы переназначения (для всего остального)
    // shellBtn — btnNew с переставленными A↔STA и B↔SEL, чтобы раскладка оболочки
    //            совпадала с раскладкой эмулятора. Эмулятор получает оригинальный btnNew.
    uint8_t btnPhys = buttons.readNew();
    uint8_t btnNew  = buttons.applyBtnMap(btnPhys);
    // Swap A↔STA и B↔SEL только для оболочки
    uint8_t shellBtn = btnNew;
    {
        uint8_t a   = (shellBtn & BTN_A)   ? BTN_STA : 0;
        uint8_t sta = (shellBtn & BTN_STA) ? BTN_A   : 0;
        uint8_t b   = (shellBtn & BTN_B)   ? BTN_SEL : 0;
        uint8_t sel = (shellBtn & BTN_SEL) ? BTN_B   : 0;
        shellBtn = (shellBtn & ~(BTN_A | BTN_STA | BTN_B | BTN_SEL)) | a | sta | b | sel;
    }
    // Диагностика нажатий в оболочке
    if (settings.diagButtons && btnPhys) {
        Serial.printf("[SHELL] phys=0x%02X mapped=0x%02X |", btnPhys, btnNew);
        if (btnNew & BTN_A)     Serial.print(" A");
        if (btnNew & BTN_B)     Serial.print(" B");
        if (btnNew & BTN_SEL)   Serial.print(" SEL");
        if (btnNew & BTN_STA)   Serial.print(" STA");
        if (btnNew & BTN_UP)    Serial.print(" UP");
        if (btnNew & BTN_DOWN)  Serial.print(" DOWN");
        if (btnNew & BTN_LEFT)  Serial.print(" LEFT");
        if (btnNew & BTN_RIGHT) Serial.print(" RIGHT");
        Serial.println();
    }
    int x = 0, y = 0;
    bool tapped = touch.isTouched();
    if (tapped) {
        touch.getXY(x, y);
        buttons.vibrateBoth(30);  // тач — оба мотора 30мс
    }
    if (settings.diagTouch && tapped) Serial.printf("[TOUCH] x=%d y=%d\n", x, y);

    // ── Sleep режим ───────────────────────────────────────────────────────
    // Отслеживаем активность; при таймауте гасим экран.
    // Любое нажатие/тап во время сна просыпает устройство (ввод поглощается).
    {
        static uint32_t _lastActivityMs = 0;
        static bool     _sleeping       = false;
        bool anyInput = (btnPhys != 0) || tapped;

        if (anyInput) {
            if (_sleeping) {
                // Просыпаемся: восстанавливаем яркость, поглощаем ввод
                _sleeping = false;
                setBrightness(settings.brightness);
                state = S_MENU;
                menuDraw();
                _lastActivityMs = millis();
                return;   // не обрабатываем нажатие — оно только для пробуждения
            }
            _lastActivityMs = millis();
        }

        // В WEB Manager экран не гасим — пользователь работает с браузера
        bool _noSleep = (state == S_WEBMGR);
        if (_noSleep) _lastActivityMs = millis();

        if (!_sleeping && settings.sleepTimeout > 0 && !_noSleep) {
            uint32_t limitMs = (uint32_t)settings.sleepTimeout * 60000UL;
            if (millis() - _lastActivityMs >= limitMs) {
                _sleeping = true;
                lcd.setBrightness(0);
            }
        }
        if (_sleeping) return;  // не рисуем ничего пока спим
    }

    // ── Drag scroll для главного меню ─────────────────────────────────────
    // rawTouched() = true пока палец удерживается (не edge-triggered)
    if (state == S_MENU) {
        static bool _prevHeld = false;
        bool _nowHeld = touch.rawTouched() && !tapped;
        if (_nowHeld) {
            int rx = 0, ry = 0;
            touch.getXY(rx, ry);
            menuRawTouch(rx, ry);
        } else if (_prevHeld && !_nowHeld) {
            menuTouchUp();
        }
        _prevHeld = _nowHeld || tapped;
    }
    // ─────────────────────────────────────────────────────────────────────

    switch (state) {

    case S_MENU: {
        menuTimeTick();          // обновляет время в нижней панели (раз в минуту)
        menuBatteryAnimTick();   // анимация зарядки (600 мс, только если charging)

        // ── Авто-прокрутка при удержании кнопки ─────────────────────────
        {
            static uint32_t holdStart   = 0;
            static uint8_t  holdDir     = 0;
            static uint32_t lastStep    = 0;
            static bool     autoActive  = false;

            uint8_t cur = buttons.applyBtnMap(buttons.readCurrent());  // текущее состояние с учётом ремапа

            if (shellBtn & BTN_UP)   { holdDir = BTN_UP;   holdStart = millis(); autoActive = false; }
            if (shellBtn & BTN_DOWN) { holdDir = BTN_DOWN; holdStart = millis(); autoActive = false; }

            if (holdDir && !(cur & holdDir)) {
                holdDir = 0; autoActive = false;   // кнопка отпущена
            }

            if (settings.autoScroll && holdDir && !autoActive
                && millis() - holdStart >= 1000) {
                autoActive = true;
                lastStep   = millis();
            }

            if (autoActive && millis() - lastStep >= 90) {
                if (holdDir == BTN_UP)   { soundClick(); menuScrollUp();   }
                if (holdDir == BTN_DOWN) { soundClick(); menuScrollDown(); }
                lastStep = millis();
                break;
            }
        }
        // ────────────────────────────────────────────────────────────────

        if (shellBtn & BTN_UP)   { soundClick(); buttons.vibrate1(40); menuScrollUp();   break; }
        if (shellBtn & BTN_DOWN) { soundClick(); buttons.vibrate1(40); menuScrollDown(); break; }
        if (shellBtn & BTN_SEL)  {
            soundBack(); buttons.vibrate1(60); ledSet(LED_YELLOW); delay(120); ledSet(LED_OFF);
            toSettings(); break;
        }
        if (shellBtn & BTN_LEFT) {
            // LEFT → file manager (browse, rename, delete ROMs)
            soundClick(); buttons.vibrate1(40);
            toFileMgr(); break;
        }
        if (shellBtn & BTN_RIGHT) {
            // RIGHT → показать информацию о выбранной игре
            int sel = menuSelected();
            if (sel >= 0 && sel < sdMgr.count()) {
                soundClick(); buttons.vibrate1(30);
                showRomInfo(sel); menuDraw();
            }
            break;
        }
        if (shellBtn & BTN_STA) {
            // START → запуск игры
            int sel = menuSelected();
            if (sel >= 0 && sel < sdMgr.count()) {
                soundSelect();
                ledSet(LED_BLUE); delay(150); ledSet(LED_OFF);
                soundStop();   // ждём чистого завершения I2S перед захватом NES
                runEmulator(sel);
                ledSet(LED_GREEN); delay(200); ledSet(LED_OFF);
                toMenu();
            }
            break;
        }
        // BTN_A, BTN_B — ничего не делают в главном меню
        if (!tapped) break;
        int romAction = 0;
        uint8_t action = menuHandleTouch(x, y, romAction);
        if (action & BTN_SEL) {
            soundBack(); ledSet(LED_YELLOW); delay(120); ledSet(LED_OFF);
            toSettings();
        } else if (action == 0xC0) {
            // 🔍 Поиск — открываем клавиатуру
            soundClick();
            wifiKeyboardReset();
            wifiKeyboardSetLabel(settings.language==LANG_RU ? cyrStr("Поиск:") : "Search ROMs:");
            wifiKeyboardSetMask(false);
            state = S_SEARCH_KB;
            wifiKeyboardDraw("");
            menuDrawSearchKbBack();   // кнопка "← BACK" под клавиатурой
        } else if (action == 0xD2) {
            // 🌐 Веб-загрузчик файлов
            soundClick();
            toWebMgr();
        } else if (action == 0xE0) {
            // WiFi icon tap → System settings
            soundClick(); buttons.vibrate1(30);
            _settingsFromMenu = true;
            fadeOut(); state = S_SETTINGS; settingsOpenCat(2); fadeIn();
        } else if (action == 0xE1) {
            // Time tap → Date & Time sub (cat=9, in Display)
            soundClick(); buttons.vibrate1(30);
            _settingsFromMenu = true;
            fadeOut(); state = S_SETTINGS; settingsOpenCat(9); fadeIn();
        } else if (action == 0xE2) {
            // Date tap → Date & Time sub (cat=9, in Display)
            soundClick(); buttons.vibrate1(30);
            _settingsFromMenu = true;
            fadeOut(); state = S_SETTINGS; settingsOpenCat(9); fadeIn();
        } else if (action == 0xE3) {
            // Battery icon tap → Battery virtual screen
            soundClick(); buttons.vibrate1(30);
            _settingsFromMenu = true;
            fadeOut(); state = S_SETTINGS; settingsOpenCat(7); fadeIn();
        } else if (action & BTN_A) {
            if (romAction == 2) {
                soundClick(); buttons.vibrate1(40); showRomInfo(menuSelected()); menuDraw();
            } else if (romAction >= 1) {
                int sel = menuSelected();
                if (sel >= 0 && sel < sdMgr.count()) {
                    soundSelect(); buttons.vibrate1(80);
                    ledSet(LED_BLUE); delay(150); ledSet(LED_OFF);
                    runEmulator(sel);
                    ledSet(LED_GREEN); delay(200); ledSet(LED_OFF);
                    toMenu();
                }
            }
        } else if (action & BTN_LEFT) {
            soundClick(); buttons.vibrate1(40);
            toFileMgr();
        } else if (action & BTN_RIGHT) {
            int sel = menuSelected();
            if (sel >= 0 && sel < sdMgr.count()) {
                soundClick(); buttons.vibrate1(30);
                showRomInfo(sel); menuDraw();
            }
        } else if (action) { soundClick(); buttons.vibrate1(30); }
        break;
    }

    case S_SETTINGS: {
        settingsBatteryTick();
        if (shellBtn) {
            uint8_t r = settingsNavBtn(shellBtn);
            if (r & BTN_B) { soundBack(); buttons.vibrate1(50); cfgSave(); _settingsFromMenu = false; toMenu(); break; }
            if (r == 0x40) { soundClick(); buttons.vibrate1(40); toRemap(); break; }
            if (r == 0x80) { soundClick(); cfgSave(); toWifi(); break; }
            if (r == 0xA0) { soundClick(); otaScreen(); settingsDraw(); break; }
            if (r == 0xB0) { soundClick(); cfgSave(); toGG(); break; }
            if (r == 0xC0) { soundClick(); toggleWebDebug(); break; }
            if (r == 0xD0) { soundClick(); cfgSave(); toGS(); break; }
            if (r == 0xD4) { soundClick(); cfgSave(); toSdMgr(); break; }
            soundClick(); break;
        }
        if (!tapped) break;
        uint8_t action = settingsHandleTouch(x, y);
        if (action & BTN_B)       { soundBack(); cfgSave(); _settingsFromMenu = false; toMenu(); }
        else if (action == 0x40)  { soundClick(); toRemap(); }
        else if (action == 0x80)  { soundClick(); cfgSave(); toWifi(); }
        else if (action == 0xA0)  { soundClick(); otaScreen(); settingsDraw(); }
        else if (action == 0xB0)  { soundClick(); cfgSave(); toGG(); }
        else if (action == 0xC0)  { soundClick(); toggleWebDebug(); }
        else if (action == 0xD0)  { soundClick(); cfgSave(); toGS(); }
        else if (action == 0xD4)  { soundClick(); cfgSave(); toSdMgr(); }
        else if (action)            soundClick();
        break;
    }

    case S_REMAP: {
        // Экран ремапа: используем ФИЗИЧЕСКИЕ кнопки (btnPhys), иначе
        // пользователь не сможет управлять экраном если переназначил кнопки.
        // btnMapNavBtn проверяет BTN_SEL (SELECT физ.) для возврата.
        if (btnPhys) {
            uint8_t r = btnMapNavBtn(btnPhys);
            if (r & BTN_B) { soundBack(); buttons.vibrate1(50); cfgSave(); toSettings(); break; }
            soundClick(); break;
        }
        if (!tapped) break;
        uint8_t action = btnMapHandleTouch(x, y);
        if (action & BTN_B) { soundBack(); cfgSave(); toSettings(); }
        else if (action) soundClick();
        break;
    }

    case S_FILEMGR: {
        if (shellBtn) {
            uint8_t r = fileMgrNavBtn(shellBtn);
            if (r == BTN_B) { soundBack(); buttons.vibrate1(40); toMenu(); break; }
            if (r == BTN_A && sdMgr.count() > 0) {
                // START → rename selected ROM
                soundClick();
                wifiKeyboardReset();
                wifiKeyboardSetLabel(settings.language==LANG_RU ? cyrStr("Переим.:") : "Rename:");
                wifiKeyboardSetMask(false);  // show plain text, not dots
                wifiKeyboardSetInitial(sdMgr.get(fileMgrSelected()).name.c_str());
                state = S_FILEMGR_KB;
                wifiKeyboardDraw(sdMgr.get(fileMgrSelected()).name.c_str());
                break;
            }
            if (r == 0xFF) { soundClick(); break; }  // list changed, redrawn
            break;
        }
        if (!tapped) break;
        uint8_t action = fileMgrHandleTouch(x, y);
        if (action == BTN_B) { soundBack(); toMenu(); }
        else if (action == BTN_A && sdMgr.count() > 0) {
            // RENAME button tapped
            soundClick();
            wifiKeyboardReset();
            wifiKeyboardSetLabel("Rename:");
            wifiKeyboardSetMask(false);
            wifiKeyboardSetInitial(sdMgr.get(fileMgrSelected()).name.c_str());
            state = S_FILEMGR_KB;
            wifiKeyboardDraw(sdMgr.get(fileMgrSelected()).name.c_str());
        }
        // 0xFF = delete completed and redrawn, nothing else to do
        break;
    }

    case S_FILEMGR_KB: {
        // Rename keyboard — full physical button navigation
        auto doRename = [&]() {
            const char *newName = wifiKeyboardGetPassword();
            if (newName && strlen(newName) > 0) {
                bool ok = sdMgr.renameROM(fileMgrSelected(), newName);
                if (ok) { soundSelect(); buttons.vibrate1(80); popupShow("Rename", "Renamed successfully", 1500); }
                else    { popupShow("Rename", "Rename failed!", 2000); }
            }
            toFileMgr();
        };
        if (shellBtn) {
            uint8_t r = wifiKeyboardNavBtn(shellBtn);
            if (r == BTN_B) { soundBack(); toFileMgr(); break; }
            if (r == BTN_A) { soundClick(); doRename(); break; }
            soundClick(); break;
        }
        if (!tapped) break;
        uint8_t action = wifiKeyboardHandleTouch(x, y);
        if (action == BTN_B) { soundBack(); toFileMgr(); }
        else if (action == BTN_A) { soundClick(); doRename(); }
        break;
    }

    case S_GG: {
        // ── Экран Game Genie кодов ────────────────────────────────────────
        // ggScreenNavBtn() возвращает:
        //   BTN_B        — вернуться в настройки
        //   0xC1..0xC8   — открыть клавиатуру для слота N
        auto openGGKeyboard = [&](int slot) {
            // Заполняем клавиатуру текущим кодом слота (или пустой)
            wifiKeyboardReset();
            wifiKeyboardSetLabel("Game Genie code (6 or 8 chars):");
            wifiKeyboardSetMask(false);
            const char *existing = settings.ggCodes[slot - 1];
            if (existing[0]) wifiKeyboardSetInitial(existing);
            ggScreenSetSlot(slot);
            state = S_GG_KB;
            wifiKeyboardDraw("APZLGITYEOXUKSVN");
        };

        if (shellBtn) {
            uint8_t r = ggScreenNavBtn(shellBtn);
            if (r == BTN_B) { soundBack(); buttons.vibrate1(40); cfgSave(); toSettings(); break; }
            if (r >= 0xC1 && r <= 0xC8) { soundClick(); openGGKeyboard(r - 0xC0); break; }
            if (r) soundClick();
            break;
        }
        if (!tapped) break;
        {
            uint8_t a = ggScreenHandleTouch(x, y);
            if (a == BTN_B) { soundBack(); cfgSave(); toSettings(); }
            else if (a >= 0xC1 && a <= 0xC8) { soundClick(); openGGKeyboard(a - 0xC0); }
            else if (a) soundClick();
        }
        break;
    }

    case S_GG_KB: {
        // ── Ввод Game Genie кода через клавиатуру ─────────────────────────
        // Слот для сохранения установлен через ggScreenSetSlot() при переходе в S_GG_KB.

        auto doSaveGGCode = [&]() {
            const char *raw = wifiKeyboardGetPassword();
            if (!raw || raw[0] == '\0') { toGG(); return; }

            // Приводим к верхнему регистру
            char upper[9]; int i;
            for (i = 0; i < 8 && raw[i]; i++) upper[i] = (char)toupper((unsigned char)raw[i]);
            upper[i] = '\0';

            // Проверяем что код корректный (6 или 8 символов из GG-алфавита)
            uint16_t addr; uint8_t val, cmp; bool hasCmp;
            if (!gg_decode(upper, &addr, &val, &cmp, &hasCmp)) {
                popupShow("Game Genie", "Invalid code! Use only letters APZLGITYEOXUKSVN, 6 or 8 chars.", 3000);
                toGG(); return;
            }

            ggCodeSaveToSlot(upper);  // сохраняем в текущий _ggSel слот
            cfgSave();
            popupShow("Game Genie", (String("Saved: ") + upper).c_str(), 1500);
            toGG();
        };

        if (shellBtn) {
            uint8_t r = wifiKeyboardNavBtn(shellBtn);
            if (r == BTN_B) { soundBack(); toGG(); break; }
            if (r == BTN_A) { soundClick(); doSaveGGCode(); break; }
            soundClick(); break;
        }
        if (!tapped) break;
        uint8_t action = wifiKeyboardHandleTouch(x, y);
        if (action == BTN_B) { soundBack(); toGG(); }
        else if (action == BTN_A) { soundClick(); doSaveGGCode(); }
        break;
    }

    case S_GS: {
        // ── Экран Game Shark кодов ────────────────────────────────────────
        auto openGSKeyboard = [&](int slot) {
            wifiKeyboardReset();
            wifiKeyboardSetLabel("Code (hex AAAAVV):");
            wifiKeyboardSetMask(false);
            const char *existing = settings.gsCodes[slot - 1];
            if (existing[0]) wifiKeyboardSetInitial(existing);
            gsScreenSetSlot(slot);
            state = S_GS_KB;
            wifiKeyboardDraw("Game Shark");
        };
        if (shellBtn) {
            uint8_t r = gsScreenNavBtn(shellBtn);
            if (r == BTN_B) { soundBack(); buttons.vibrate1(40); cfgSave(); toSettings(); break; }
            if (r >= 0xC1 && r <= 0xC8) { soundClick(); openGSKeyboard(r - 0xC0); break; }
            if (r) soundClick();
            break;
        }
        if (!tapped) break;
        {
            uint8_t a = gsScreenHandleTouch(x, y);
            if (a == BTN_B) { soundBack(); cfgSave(); toSettings(); }
            else if (a >= 0xC1 && a <= 0xC8) { soundClick(); openGSKeyboard(a - 0xC0); }
            else if (a) soundClick();
        }
        break;
    }

    case S_GS_KB: {
        // ── Ввод Game Shark кода через клавиатуру ─────────────────────────
        auto doSaveGSCode = [&]() {
            const char *raw = wifiKeyboardGetPassword();
            if (!raw || raw[0] == '\0') { toGS(); return; }

            // Приводим к верхнему регистру
            char upper[7]; int i;
            for (i = 0; i < 6 && raw[i]; i++) upper[i] = (char)toupper((unsigned char)raw[i]);
            upper[i] = '\0';

            uint16_t addr; uint8_t val;
            if (!gs_decode(upper, &addr, &val)) {
                popupShow("Game Shark", "Invalid! Enter exactly 6 hex digits (AAAAVV).", 3000);
                toGS(); return;
            }

            gsCodeSaveToSlot(upper);
            cfgSave();
            char msg[32]; snprintf(msg, sizeof(msg), "Saved: %c%c%c%c:%c%c",
                upper[0], upper[1], upper[2], upper[3], upper[4], upper[5]);
            popupShow("Game Shark", msg, 1500);
            toGS();
        };

        if (shellBtn) {
            uint8_t r = wifiKeyboardNavBtn(shellBtn);
            if (r == BTN_B) { soundBack(); toGS(); break; }
            if (r == BTN_A) { soundClick(); doSaveGSCode(); break; }
            soundClick(); break;
        }
        if (!tapped) break;
        uint8_t action = wifiKeyboardHandleTouch(x, y);
        if (action == BTN_B) { soundBack(); toGS(); }
        else if (action == BTN_A) { soundClick(); doSaveGSCode(); }
        break;
    }

    case S_SEARCH_KB: {
        // ── Ввод поискового запроса ───────────────────────────────────────
        if (shellBtn) {
            uint8_t r = wifiKeyboardNavBtn(shellBtn);
            if (r == BTN_B) { soundBack(); menuClearSearch(); toMenu(); break; }
            if (r == BTN_A) {
                soundClick();
                menuSetSearch(wifiKeyboardGetPassword());
                toMenu();
                break;
            }
            soundClick(); break;
        }
        if (!tapped) break;
        // ── Тач "← BACK" (зона под клавиатурой: y>=210, x<120) ──────────
        if (y >= 210 && x < 120) {
            soundBack(); menuClearSearch(); toMenu(); break;
        }
        {
            uint8_t ac = wifiKeyboardHandleTouch(x, y);
            if (ac == BTN_B) { soundBack(); menuClearSearch(); toMenu(); }
            else if (ac == BTN_A) {
                soundClick();
                menuSetSearch(wifiKeyboardGetPassword());
                toMenu();
            }
        }
        break;
    }


    case S_WEBMGR: {
        webMgrHandle();   // обрабатываем HTTP запросы
        if ((shellBtn & BTN_B) || tapped) {
            // Любой тап или B = выход
            bool exitNow = (shellBtn & BTN_B) != 0;
            if (!exitNow && tapped) {
                int by = SCREEN_H - 44;
                exitNow = (y >= by);
            }
            if (exitNow) {
                soundBack();
                webMgrStop();
                toMenu();
            }
        }
        break;
    }



    case S_SDMGR: {
        if (shellBtn) {
            uint8_t r = sdMgrNavBtn(shellBtn);
            if (r == BTN_B) { soundBack(); cfgSave(); toSettings(); }
            break;
        }
        if (!tapped) break;
        {
            uint8_t r = sdMgrHandleTouch(x, y);
            if (r == BTN_B) { soundBack(); cfgSave(); toSettings(); }
        }
        break;
    }


    case S_PLAYING:
        break;

    case S_WIFI: {
        if (shellBtn) {
            uint8_t r = wifiManagerNavBtn(shellBtn);
            if (r == BTN_B) { soundBack(); buttons.vibrate1(40); cfgSave(); toSettings(); break; }
            if (r == BTN_A) { soundClick(); openWifiKeyboard(); break; }
            soundClick(); break;
        }
        if (!tapped) break;
        uint8_t action = wifiManagerHandleTouch(x, y);
        if (action == BTN_B) { soundBack(); cfgSave(); toSettings(); }
        else if (action == BTN_A) { soundClick(); openWifiKeyboard(); }
        break;
    }

    case S_WIFI_KB: {
        // Full physical button navigation for password keyboard
        if (shellBtn) {
            uint8_t r = wifiKeyboardNavBtn(shellBtn);
            if (r == BTN_B) { soundBack(); toWifi(); break; }
            if (r == BTN_A) { soundClick(); doWifiConnect(); break; }
            soundClick(); break;
        }
        if (!tapped) break;
        uint8_t action = wifiKeyboardHandleTouch(x, y);
        if (action == BTN_B) { soundBack(); toWifi(); }
        else if (action == BTN_A) { soundClick(); doWifiConnect(); }
        break;
    }

    }

    delay(16);
}

// ─────────────────────────────────────────────────────────────
static void showError(const char *title, const char *l1, const char *l2) {
    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);
    lcd.setTextDatum(MC_DATUM);
    lcd.setFont(&lgfx::fonts::DejaVu18);
    lcd.setTextColor(0xF800);
    lcd.drawString(title, SCREEN_W/2, 70);
    lcd.setFont(&lgfx::fonts::DejaVu12);
    lcd.setTextColor(t.textPri);
    lcd.drawString(l1, SCREEN_W/2, 115);
    if (l2) lcd.drawString(l2, SCREEN_W/2, 140);
    lcd.setFont(&lgfx::fonts::DejaVu9);
    lcd.setTextColor(t.textSec);
    lcd.drawString("Tap to return", SCREEN_W/2, 200);
    uint32_t dl = millis() + 5000;
    while (millis() < dl) { if (touch.isTouched()) break; delay(50); }
}

static void runEmulator(int idx) {
    if (idx < 0 || idx >= sdMgr.count()) return;
    state = S_PLAYING;

    const ROMInfo &rom = sdMgr.get(idx);
    const char *path   = rom.path.c_str();

    // ── mapper pre-check: avoid black screen for unsupported ROMs ─────────
    int romMapper = nes_read_mapper(path);
    if (!nes_mapper_supported(romMapper)) {
        state = S_MENU;
        char ml[36];
        snprintf(ml, sizeof(ml), "Mapper %d not supported", romMapper);
        webConsolePrintf("[NES] %s — %s\n", ml, path);
        showError("Cannot load ROM", ml, rom.name.c_str());
        return;
    }

    if (settings.diagEmu)
        webConsolePrintf("[UI]  Starting '%s' %uKB  mapper=%d  heap=%uKB\n",
                         path, (unsigned)(rom.size/1024), romMapper,
                         (unsigned)(ESP.getFreeHeap()/1024));

    const Theme565 &t = getTheme();
    lcd.fillScreen(t.bg);
    lcd.fillRect(0, 0, SCREEN_W, HDR_H, t.header);
    lcd.setTextDatum(MC_DATUM);
    lcd.setFont(&lgfx::fonts::DejaVu12);
    lcd.setTextColor(t.textPri);
    lcd.drawString("Loading...", SCREEN_W/2, HDR_H/2);

    lcd.fillCircle(SCREEN_W/2, 110, 46, t.accent);
    lcd.setTextColor(t.bg);
    lcd.setFont(&lgfx::fonts::DejaVu18);
    lcd.drawString("NES", SCREEN_W/2, 110);

    lcd.setFont(&lgfx::fonts::DejaVu12);
    lcd.setTextColor(t.textPri);
    String name = rom.name;
    if (name.length() > 26) name = name.substring(0, 24) + "..";
    lcd.drawString(name.c_str(), SCREEN_W/2, 168);
    lcd.setFont(&lgfx::fonts::DejaVu9);
    lcd.setTextColor(t.textSec);
    char info[48];
    snprintf(info, sizeof(info), "ROM: %uKB   Heap: %uKB",
             (unsigned)(rom.size/1024), (unsigned)(ESP.getFreeHeap()/1024));
    lcd.drawString(info, SCREEN_W/2, 188);
    lcd.setTextColor(buttons.isConnected() ? t.ok : t.textSec);
    lcd.drawString(buttons.isConnected() ? "Pico connected" : "Touch top = exit",
                   SCREEN_W/2, 207);
    delay(400);

    uint32_t _playStart = millis();
    GameStats::recentAdd(path);
    GameStats::save();

    if (settings.diagEmu) webConsolePrintf("[SYS] Starting nofrendo NES emulator\n");
    if (settings.btEnabled) btMgrStart();
    int result = emu_run(path);
    if (settings.btEnabled) btMgrStop();

    // Записываем время игры (в секундах)
    uint32_t _playSecs = (millis() - _playStart) / 1000;
    if (_playSecs > 5) {  // игнорируем слишком короткие запуски
        GameStats::playTimeAdd(path, _playSecs);
        GameStats::save();
    }

    if (settings.diagEmu)
        webConsolePrintf("[SYS] Emulator exited: result=%d  heap=%uKB  playtime=%us\n",
                         result, (unsigned)(ESP.getFreeHeap()/1024), (unsigned)_playSecs);

    initDisplay();
    touch.init();

    if (result != 0) {
        char h[32];
        snprintf(h, sizeof(h), "Heap: %uKB", (unsigned)(ESP.getFreeHeap()/1024));
        showError("Emulator error", "ROM load failed", h);
        initDisplay(); touch.init();
    }
}
