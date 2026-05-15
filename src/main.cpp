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

Settings settings;

static char _emuPath[128];

static int runEmulator_blocking(const char *path) {
    strncpy(_emuPath, path, sizeof(_emuPath)-1);
    return emu_run(path);
}

// ─────────────────────────────────────────────────────────────
// STATE MACHINE
// ─────────────────────────────────────────────────────────────

enum State { S_MENU, S_SETTINGS, S_REMAP, S_PLAYING };
static State state = S_MENU;

static void toMenu()     { state = S_MENU;     menuDraw(); }
static void toSettings() { state = S_SETTINGS; settingsDraw(); }
static void toRemap()    { state = S_REMAP;    btnMapDraw(); }

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

    // ── Время (читает settings.timeH/timeM после cfgLoad) ─────
    timeInit();

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

    uint8_t btnNew = buttons.readNew();
    int x = 0, y = 0;
    bool tapped = touch.isTouched();
    if (tapped) touch.getXY(x, y);

    switch (state) {

    case S_MENU: {
        menuTimeTick();   // обновляет время в нижней панели (раз в минуту)

        // ── Авто-прокрутка при удержании кнопки ─────────────────────────
        {
            static uint32_t holdStart   = 0;
            static uint8_t  holdDir     = 0;
            static uint32_t lastStep    = 0;
            static bool     autoActive  = false;

            uint8_t cur = buttons.readCurrent();  // текущее состояние кнопок

            if (btnNew & BTN_UP)   { holdDir = BTN_UP;   holdStart = millis(); autoActive = false; }
            if (btnNew & BTN_DOWN) { holdDir = BTN_DOWN; holdStart = millis(); autoActive = false; }

            if (holdDir && !(cur & holdDir)) {
                holdDir = 0; autoActive = false;   // кнопка отпущена
            }

            if (settings.autoScroll && holdDir && !autoActive
                && millis() - holdStart >= 1800) {
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
        if (btnNew & BTN_A) {
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
            if (r == 0x40)  { soundClick(); buttons.vibrate1(40); toRemap(); break; }  // open remap
            soundClick(); break;
        }
        if (!tapped) break;
        uint8_t action = settingsHandleTouch(x, y);
        if (action & BTN_B)       { soundBack(); cfgSave(); toMenu(); }
        else if (action == 0x40)  { soundClick(); toRemap(); }  // open remap
        else if (action)            soundClick();
        break;
    }

    case S_REMAP: {
        if (btnNew) {
            uint8_t r = btnMapNavBtn(btnNew);
            if (r & BTN_B) { soundBack(); buttons.vibrate1(50); btnMapApply(); cfgSave(); toSettings(); break; }
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

    Serial.printf("[SYS] Starting nofrendo NES emulator\n");
    int result = emu_run(path);
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
