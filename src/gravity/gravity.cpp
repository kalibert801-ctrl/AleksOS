#include "gravity.h"
#include "GDMicro.h"
#include "GDGfx.h"
#include "LevelLoader.h"
#include "GamePhysics.h"
#include "GameCanvas.h"
#include "config.h"
#include "display/display_manager.h"
#include "ui/ui.h"
#include "input/button_handler.h"
#include "input/touch_handler.h"
#include "settings.h"
#include <Arduino.h>

// Micro stub definitions
bool Micro::isInGameMenu = false;
bool Micro::field_249    = true;

// Raw physical button bits from Pico UART protocol (independent of user btnMap)
#define PHYS_STA   0x01
#define PHYS_SEL   0x02
#define PHYS_B     0x04
#define PHYS_A     0x08
#define PHYS_UP    0x10
#define PHYS_DOWN  0x20
#define PHYS_LEFT  0x40
#define PHYS_RIGHT 0x80

static LevelLoader*  s_ll     = nullptr;
static GamePhysics*  s_gp     = nullptr;
static GameCanvas*   s_gc     = nullptr;

static int  s_touchX = -1;
static uint8_t s_btnState = 0;
static uint32_t s_lastFrameMs = 0;
static const uint32_t FRAME_MS = 33;
static const int NUM_PHYSICS_LOOPS = 2;
static bool s_exitRequested = false;

bool gravityOpen(int league, int level) {
    gravityClose();
    Serial.printf("[GD] Opening league=%d level=%d\n", league, level);

    s_ll = new LevelLoader("/levels.mrg");
    if (!s_ll->gameLevel) {
        Serial.println("[GD] Failed: gameLevel is null");
        gravityClose();
        return false;
    }

    // Select the desired league/level
    if (league != 0 || level != 0) {
        s_ll->method_88(league, level);
    }

    s_gp = new GamePhysics(s_ll);
    s_gp->setMode(1);

    s_gc = new GameCanvas();
    s_gc->init(s_gp);

    s_touchX = -1;
    s_btnState = 0;
    s_lastFrameMs = millis();
    s_exitRequested = false;

    // Draw initial frame
    GDGfx gfx(SCREEN_W, SCREEN_H);
    gfx.startFrame();
    s_gc->drawGame(&gfx);
    gfx.endFrame();

    Serial.println("[GD] Game started OK");
    return true;
}

void gravityClose() {
    delete s_gp; s_gp = nullptr;
    delete s_ll; s_ll = nullptr;
    delete s_gc; s_gc = nullptr;
}

bool gravityLevelSelect(int* outLeague, int* outLevel) {
    LevelLoader* ll = new LevelLoader("/levels.mrg");
    if (!ll->gameLevel) {
        delete ll;
        popupShow("Gravity Defied", "levels.mrg not found!\nCopy to SD root.", 3000);
        return false;
    }

    const Theme565& t = getTheme();
    int league = 0, level = 0, stage = 0;

    auto draw = [&]() {
        lcd.startWrite();
        lcd.fillScreen(t.bg);
        lcd.fillRect(0, 0, SCREEN_W, 34, t.header);
        lcd.setFont(&lgfx::fonts::DejaVu18);
        lcd.setTextDatum(MC_DATUM);
        lcd.setTextColor(0xFD20u);
        lcd.drawString("GRAVITY DEFIED", SCREEN_W / 2, 17);

        if (stage == 0) {
            lcd.setFont(&lgfx::fonts::DejaVu12);
            lcd.setTextColor(t.textSec);
            lcd.setTextDatum(MC_DATUM);
            lcd.drawString("Select Liga", SCREEN_W / 2, 50);

            const char* names[] = { "Liga 1", "Liga 2", "Liga 3" };
            for (int i = 0; i < 3; i++) {
                bool hi = (i == league);
                lcd.fillRoundRect(20, 66 + i * 52, 280, 44, 6,
                                  hi ? t.accent : t.rowEven);
                lcd.setTextColor(hi ? (uint16_t)0xFFFF : t.textPri);
                lcd.setTextDatum(MC_DATUM);
                lcd.drawString(names[i], SCREEN_W / 2, 66 + i * 52 + 22);
            }
        } else {
            int n = (int)ll->levelNames[league].size();
            char hdr[20];
            sprintf(hdr, "Liga %d", league + 1);
            lcd.setFont(&lgfx::fonts::DejaVu12);
            lcd.setTextColor(t.accent);
            lcd.setTextDatum(MC_DATUM);
            lcd.drawString(hdr, SCREEN_W / 2, 48);

            int visible = 4;
            int start = level - 1;
            if (start < 0) start = 0;
            if (n > visible && start + visible > n) start = n - visible;

            for (int i = 0; i < visible && start + i < n; i++) {
                int lvl = start + i;
                bool hi = (lvl == level);
                lcd.fillRoundRect(20, 62 + i * 42, 280, 36, 5,
                                  hi ? t.accent : t.rowEven);
                lcd.setTextColor(hi ? (uint16_t)0xFFFF : t.textPri);
                lcd.setTextDatum(ML_DATUM);
                lcd.drawString(ll->getName(league, lvl).c_str(), 34, 62 + i * 42 + 18);
            }
        }

        lcd.setFont(&lgfx::fonts::DejaVu9);
        lcd.setTextColor(t.textSec);
        lcd.setTextDatum(MC_DATUM);
        if (stage == 0)
            lcd.drawString("UP/DOWN: select  A/START: ok  B: cancel", SCREEN_W / 2, 228);
        else
            lcd.drawString("UP/DOWN: select  A/START: play  B: back", SCREEN_W / 2, 228);
        lcd.endWrite();
    };

    draw();
    delay(200);
    while (buttons.read()) delay(10);

    bool result = false;
    for (;;) {
        buttons.update();
        uint8_t btn = buttons.readNew();  // raw physical bits

        // Touch navigation
        if (touch.isTouched()) {
            int tx, ty; touch.getXY(tx, ty);
            if (stage == 0) {
                for (int i = 0; i < 3; i++) {
                    if (tx >= 20 && tx <= 300 && ty >= 66+i*52 && ty < 66+i*52+44) {
                        if (i == league) { stage = 1; level = 0; draw(); }
                        else { league = i; draw(); }
                        break;
                    }
                }
            } else {
                int n = (int)ll->levelNames[league].size(); if (n < 1) n = 1;
                int vis = 4;
                int start = level - 1; if (start < 0) start = 0;
                if (n > vis && start + vis > n) start = n - vis;
                for (int i = 0; i < vis && start+i < n; i++) {
                    int lvl = start + i;
                    if (tx >= 20 && tx <= 300 && ty >= 62+i*42 && ty < 62+i*42+36) {
                        if (lvl == level) {
                            *outLeague = league; *outLevel = level; result = true;
                            delete ll; return result;
                        }
                        level = lvl; draw(); break;
                    }
                }
            }
        }

        if (!btn) { delay(16); continue; }

        if (stage == 0) {
            if (btn & PHYS_UP)              { league = (league + 2) % 3; draw(); }
            if (btn & PHYS_DOWN)            { league = (league + 1) % 3; draw(); }
            if (btn & (PHYS_A | PHYS_STA))  { stage = 1; level = 0; draw(); }
            if (btn & PHYS_B)               { result = false; break; }
        } else {
            int n = (int)ll->levelNames[league].size();
            if (n < 1) n = 1;
            if (btn & PHYS_UP)              { level = (level > 0) ? level - 1 : n - 1; draw(); }
            if (btn & PHYS_DOWN)            { level = (level < n - 1) ? level + 1 : 0; draw(); }
            if (btn & (PHYS_A | PHYS_STA))  { *outLeague = league; *outLevel = level; result = true; break; }
            if (btn & PHYS_B)               { stage = 0; level = 0; draw(); }
        }
        delay(16);
    }

    delete ll;
    return result;
}

void gravityHandleTouch(int x, int y) {
    s_touchX = x;
}

void gravityNoTouch() {
    s_touchX = -1;
}

bool gravityNavBtn(uint8_t heldPhysBtn) {
    s_btnState = heldPhysBtn;
    return false;
}

int gravityUpdate() {
    if (!s_gp || !s_gc) return 3;
    if (s_exitRequested) return 3;

    uint32_t now = millis();
    if (now - s_lastFrameMs < FRAME_MS) return 0;
    s_lastFrameMs = now;

    // Advance game time
    s_gc->gameTimeMs += FRAME_MS;

    // Set input: combine touch + buttons
    int upDown = 0, lr = 0;

    if (s_touchX >= 0) {
        upDown = 1;                            // any touch = gas
        lr     = (s_touchX < SCREEN_W / 2) ? -1 : 1;
    }
    if (s_btnState & PHYS_A)     upDown = 1;   // physical A = gas
    if (s_btnState & PHYS_B)     upDown = -1;  // physical B = brake
    if (s_btnState & PHYS_LEFT)  lr = -1;
    if (s_btnState & PHYS_RIGHT) lr = 1;

    s_gp->method_30(upDown, lr);

    // Physics update — stop on first non-running result
    int result = 0;
    for (int i = 0; i < NUM_PHYSICS_LOOPS; ++i) {
        int r = s_gp->updatePhysics();
        if (r != 5 && r != 4) { result = r; break; }
        result = r;
    }
    s_gp->method_53();

    // Handle game events
    if (result == 2) {
        // Goal / level complete
        s_gc->scheduleGameTimerTask("Level Complete!", 2000);
    } else if (result == 3) {
        // Crash (wheels too close)
        s_gc->scheduleGameTimerTask("Crashed!", 1500);
    }

    // Render — wrap in SPI transaction to eliminate flicker
    GDGfx gfx(SCREEN_W, SCREEN_H);
    gfx.startFrame();
    s_gc->drawGame(&gfx);
    gfx.endFrame();

    if (result == 2) return 1; // level complete
    if (result == 3) return 2; // crashed

    return 0; // running
}

// Pause menu — returns false if user chose Exit
bool gravityPause() {
    if (!s_gp || !s_ll) return false;

    const Theme565& t = getTheme();
    const char* items[] = { "Resume", "Restart", "Exit to Menu" };
    const int N = 3;
    int sel = 0;

    auto draw = [&]() {
        lcd.startWrite();
        lcd.fillRoundRect(60, 72, 200, 112, 8, t.header);
        lcd.drawRoundRect(60, 72, 200, 112, 8, t.accent);
        lcd.setFont(&lgfx::fonts::DejaVu12);
        lcd.setTextDatum(MC_DATUM);
        lcd.setTextColor(t.accent);
        lcd.drawString("PAUSE", 160, 87);
        for (int i = 0; i < N; i++) {
            bool hi = (i == sel);
            lcd.fillRect(70, 98 + i * 28, 180, 24, hi ? t.accent : t.header);
            lcd.setTextColor(hi ? (uint16_t)0xFFFF : t.textPri);
            lcd.drawString(items[i], 160, 98 + i * 28 + 12);
        }
        lcd.endWrite();
    };

    draw();
    delay(200);
    while (buttons.read()) delay(10); // wait for release

    for (;;) {
        buttons.update();
        uint8_t btn = buttons.readNew();  // raw physical bits
        if (btn & PHYS_UP)              { sel = sel > 0 ? sel - 1 : N - 1; draw(); }
        if (btn & PHYS_DOWN)            { sel = sel < N - 1 ? sel + 1 : 0; draw(); }
        if (btn & (PHYS_A | PHYS_STA)) {
            if (sel == 0) return true;  // Resume
            if (sel == 1) {             // Restart
                gravityOpen(s_ll->field_125, s_ll->field_126);
                return true;
            }
            return false;              // Exit to Menu
        }
        if (btn & PHYS_B) return true;  // B = resume
        delay(16);
    }
}
