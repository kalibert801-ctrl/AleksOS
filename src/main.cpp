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
#include "ui/ui.h"
#include "emulator/emu_runner.h"
#include "network/wifi_manager.h"
#include "network/ntp_manager.h"
#include "network/ota_manager.h"
#include "network/pico_ota.h"

Settings settings;

static char _emuPath[128];

static int runEmulator_blocking(const char *path) {
    strncpy(_emuPath, path, sizeof(_emuPath)-1);
    return emu_run(path);
}

// ─────────────────────────────────────────────────────────────
// STATE MACHINE
// ─────────────────────────────────────────────────────────────

enum State { S_MENU, S_SETTINGS, S_REMAP, S_PLAYING, S_WIFI, S_WIFI_KB };
static State state = S_MENU;

static void toMenu()     { state = S_MENU;     menuDraw(); }
static void toSettings() { state = S_SETTINGS; settingsDraw(); }
static void toRemap()    { state = S_REMAP;    btnMapDraw(); }
static void toWifi()     { state = S_WIFI;     wifiManagerDraw(); }

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
    settings.wifiEnabled = 1;

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
    Serial.printf("[SYS] AleksOS %s  Core=%d\n", FIRMWARE_VERSION, xPortGetCoreID());

    if (psramFound()) {
        Serial.printf("[PSRAM] OK — total: %u KB, free: %u KB\n",
            ESP.getPsramSize()  / 1024,
            ESP.getFreePsram()  / 1024);
    } else {
        Serial.println("[PSRAM] NOT FOUND");
    }

    settingsDefault(settings);
    ledInit();
    initDisplay();

    // ── Boot screen ───────────────────────────────────────────
    bootScreenRun();

    // ── SD карта ─────────────────────────────────────────────
    bootProgress(15, "Mounting SD...");
    bool sdOk = sdMgr.init();

    if (sdOk) {
        bootProgress(40, "Loading config...");
        cfgLoad();
        // Диагностика: печатаем что загрузилось из конфига
        Serial.printf("[CFG] btnMap: A=%02X B=%02X SEL=%02X STA=%02X UP=%02X DN=%02X LT=%02X RT=%02X\n",
            settings.btnMap[0], settings.btnMap[1], settings.btnMap[2], settings.btnMap[3],
            settings.btnMap[4], settings.btnMap[5], settings.btnMap[6], settings.btnMap[7]);
        Serial.printf("[CFG] identity=%s  (A=01 B=02 SEL=04 STA=08 UP=10 DN=20 LT=40 RT=80)\n",
            (settings.btnMap[0]==0x01 && settings.btnMap[1]==0x02 &&
             settings.btnMap[2]==0x04 && settings.btnMap[3]==0x08) ? "YES" : "NO ← MISMATCH!");
        setBrightness(settings.brightness);
        ledSet(LED_GREEN);

        bootProgress(65, "Scanning ROMs...");
        for (int i = 0; i < 6; i++) { bootTick(); delay(30); }

        char buf[32];
        snprintf(buf, sizeof(buf), "Found %d ROMs", sdMgr.count());
        bootProgress(80, buf);
    } else {
        ledSet(LED_RED);
        bootProgress(15, "SD error!");
        for (int i = 0; i < 12; i++) { bootTick(); delay(50); }
    }

    // ── Периферия ─────────────────────────────────────────────
    bootProgress(88, "Init audio...");
    audioInit();

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

    // ── WiFi ─────────────────────────────────────────────────
    bootProgress(97, "Init WiFi...");
    wifiMgr.init();
    if (wifiMgr.isConnected()) {
        bootProgress(99, "NTP sync...");
        ntpSync();
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
    timeUpdate();
    updateAutoBrightness();
    buttons.update();

    // btnPhys  — сырые физические кнопки (для экрана ремапа, где нужен физ. ввод)
    // btnNew   — кнопки после применения таблицы переназначения (для всего остального)
    uint8_t btnPhys = buttons.readNew();
    uint8_t btnNew  = buttons.applyBtnMap(btnPhys);
    // Диагностика нажатий в оболочке
    if (settings.diagButtons && btnPhys) {
        Serial.printf("[SHELL] phys=0x%02X mapped=0x%02X |", btnPhys, btnNew);
        if (btnNew & BTN_A)     Serial.print(" STA");
        if (btnNew & BTN_B)     Serial.print(" SEL");
        if (btnNew & BTN_SEL)   Serial.print(" A");
        if (btnNew & BTN_STA)   Serial.print(" B");
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

    switch (state) {

    case S_MENU: {
        menuTimeTick();   // обновляет время в нижней панели (раз в минуту)

        // ── Авто-прокрутка при удержании кнопки ─────────────────────────
        {
            static uint32_t holdStart   = 0;
            static uint8_t  holdDir     = 0;
            static uint32_t lastStep    = 0;
            static bool     autoActive  = false;

            uint8_t cur = buttons.applyBtnMap(buttons.readCurrent());  // текущее состояние с учётом ремапа

            if (btnNew & BTN_UP)   { holdDir = BTN_UP;   holdStart = millis(); autoActive = false; }
            if (btnNew & BTN_DOWN) { holdDir = BTN_DOWN; holdStart = millis(); autoActive = false; }

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

        if (btnNew & BTN_UP)   { soundClick(); buttons.vibrate1(40); menuScrollUp();   break; }
        if (btnNew & BTN_DOWN) { soundClick(); buttons.vibrate1(40); menuScrollDown(); break; }
        if (btnNew & BTN_SEL)  {
            soundBack(); buttons.vibrate1(60); ledSet(LED_YELLOW); delay(120); ledSet(LED_OFF);
            toSettings(); break;
        }
        if (btnNew & BTN_RIGHT) {
            // RIGHT → показать информацию о выбранной игре
            int sel = menuSelected();
            if (sel >= 0 && sel < sdMgr.count()) {
                soundClick(); buttons.vibrate1(30);
                showRomInfo(sel); menuDraw();
            }
            break;
        }
        if (btnNew & BTN_STA) {
            // START → запуск игры
            int sel = menuSelected();
            if (sel >= 0 && sel < sdMgr.count()) {
                soundSelect();
                ledSet(LED_BLUE); delay(150); ledSet(LED_OFF);
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
        } else if (action & BTN_A) {
            if (romAction == 2) {
                soundClick(); buttons.vibrate1(40); showRomInfo(menuSelected()); menuDraw();
            } else if (romAction == 1) {
                int sel = menuSelected();
                if (sel >= 0 && sel < sdMgr.count()) {
                    soundSelect(); buttons.vibrate1(80);
                    ledSet(LED_BLUE); delay(150); ledSet(LED_OFF);
                    runEmulator(sel);
                    ledSet(LED_GREEN); delay(200); ledSet(LED_OFF);
                    toMenu();
                }
            }
        } else { soundClick(); buttons.vibrate1(30); }
        break;
    }

    case S_SETTINGS: {
        if (btnNew) {
            uint8_t r = settingsNavBtn(btnNew);
            if (r & BTN_B) { soundBack(); buttons.vibrate1(50); cfgSave(); toMenu(); break; }
            if (r == 0x40) { soundClick(); buttons.vibrate1(40); toRemap(); break; }
            if (r == 0x80) { soundClick(); cfgSave(); toWifi(); break; }
            if (r == 0xA0) { soundClick(); otaScreen(); settingsDraw(); break; }
            soundClick(); break;
        }
        if (!tapped) break;
        uint8_t action = settingsHandleTouch(x, y);
        if (action & BTN_B)       { soundBack(); cfgSave(); toMenu(); }
        else if (action == 0x40)  { soundClick(); toRemap(); }
        else if (action == 0x80)  { soundClick(); cfgSave(); toWifi(); }
        else if (action == 0xA0)  { soundClick(); otaScreen(); settingsDraw(); }
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
        if (action & BTN_B) { soundBack(); btnMapApply(); cfgSave(); toSettings(); }
        else if (action) soundClick();
        break;
    }

    case S_PLAYING:
        break;

    case S_WIFI: {
        if (btnNew) {
            uint8_t r = wifiManagerNavBtn(btnNew);
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
        if (btnNew & BTN_SEL) { soundBack(); toWifi(); break; }
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

    if (settings.diagEmu)
        Serial.printf("[UI]  Starting '%s' %uKB  heap=%uKB\n",
                      path, (unsigned)(rom.size/1024),
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

    if (settings.diagEmu) Serial.printf("[SYS] Starting nofrendo NES emulator\n");
    int result = emu_run(path);
    if (settings.diagEmu)
        Serial.printf("[SYS] Emulator exited: result=%d  heap=%uKB\n",
                      result, (unsigned)(ESP.getFreeHeap()/1024));

    initDisplay();
    touch.init();

    if (result != 0) {
        char h[32];
        snprintf(h, sizeof(h), "Heap: %uKB", (unsigned)(ESP.getFreeHeap()/1024));
        showError("Emulator error", "ROM load failed", h);
        initDisplay(); touch.init();
    }
}
