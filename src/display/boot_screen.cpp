// ─────────────────────────────────────────────────────────────────────────────
//  boot_screen.cpp  —  AleksOS BETA v14.38
//
//  Two modes:
//    • IMAGE  — /boot.raw loaded from SD into PSRAM (320×240 RGB565 pre-inv).
//               Bottom strip y ≥ DARK_START_Y is darkened at load time so the
//               dot spinner and status text are always readable.
//               Dot erase restores exact image pixels (no black smear).
//
//    • FALLBACK — black bg + fade-in "AleksOS" text + subtitle + dots.
//                 Used when SD has no boot.raw or PSRAM is absent.
//
//  Public API:
//    bootLogoLoad()          — load /boot.raw (call after sdMgr.init)
//    bootScreenRun()         — show logo / start animation
//    bootProgress(pct, msg)  — update status line
//    bootTick()              — advance dot spinner (~100 ms)
//    bootScreenDone()        — fade-out, free buffer
// ─────────────────────────────────────────────────────────────────────────────

#include "boot_screen.h"
#include "display/display_manager.h"
#include "led.h"
#include "config.h"
#include <Arduino.h>
#include <SD.h>

// ── Geometry ──────────────────────────────────────────────────────────────────
static constexpr int16_t CX  = SCREEN_W / 2;         // 160
static constexpr int16_t CY  = SCREEN_H / 2 - 18;    // 102  — logo centre

static constexpr int16_t DOT_Y   = SCREEN_H - 44;    // 196  — dot row Y
static constexpr int8_t  DOT_N   = 5;
static constexpr int8_t  DOT_R   = 4;
static constexpr int8_t  DOT_GAP = 14;
static constexpr int16_t DOT_X0  = CX - (DOT_N - 1) * DOT_GAP / 2;  // 132

static constexpr int16_t MSG_Y   = SCREEN_H - 18;    // 222  — status text Y

// Bottom strip: darken this region in the image so text / dots stay visible
static constexpr int16_t DARK_START_Y = 186;         // y ≥ 186 → 25% brightness

// ── Colours ───────────────────────────────────────────────────────────────────
static constexpr uint8_t C_BG_R  =   0, C_BG_G  =   0, C_BG_B  =   0;
static constexpr uint8_t C_LO_R  =  30, C_LO_G  =  30, C_LO_B  =  30;
static constexpr uint8_t C_HI_R  = 220, C_HI_G  = 220, C_HI_B  = 220;
static constexpr uint8_t C_TXT_R = 200, C_TXT_G = 200, C_TXT_B = 200;
static constexpr uint8_t C_SUB_R =  90, C_SUB_G =  90, C_SUB_B =  90;
static constexpr uint8_t C_VER_R =  55, C_VER_G =  55, C_VER_B =  55;
// Status text in image-mode is brighter so it stands out from the dark strip
static constexpr uint8_t C_MSG_R = 185, C_MSG_G = 185, C_MSG_B = 185;
static constexpr uint8_t C_MSG_FBR=  65, C_MSG_FBG=  65, C_MSG_FBB=  65; // fallback

// ── Dot-spinner state ─────────────────────────────────────────────────────────
static int8_t   _dotActive = 0;
static uint32_t _dotNextMs = 0;
static constexpr uint16_t DOT_STEP_MS = 100;

// ── Boot image ────────────────────────────────────────────────────────────────
static uint8_t *_bootImg = nullptr;   // 320×240×2 bytes, pre-inverted RGB565 LE

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static uint32_t lerpCol(uint8_t fr, uint8_t fg, uint8_t fb,
                        uint8_t tr, uint8_t tg, uint8_t tb,
                        int step, int steps) {
    if (steps == 0) return lcd.color888(tr, tg, tb);
    return lcd.color888(fr + (int)(tr-fr)*step/steps,
                        fg + (int)(tg-fg)*step/steps,
                        fb + (int)(tb-fb)*step/steps);
}

// Write a horizontal span from _bootImg to the display.
// Uses the same raw pixel path as osd.cpp: swap_bytes=true, pre-inverted data.
static void restoreImgRow(int y, int x0, int width) {
    if (!_bootImg || y < 0 || y >= SCREEN_H) return;
    int cx = (x0 < 0) ? 0 : x0;
    int cw = width - (cx - x0);
    if (cx + cw > SCREEN_W) cw = SCREEN_W - cx;
    if (cw <= 0) return;
    uint16_t *ptr = (uint16_t *)(_bootImg + ((size_t)y * SCREEN_W + cx) * 2);
    lcd.startWrite();
    lcd.setWindow(cx, y, cx + cw - 1, y);
    lcd.writePixels(ptr, cw, true);   // swap_bytes=true
    lcd.endWrite();
}

static void restoreImgRect(int x0, int y0, int w, int h) {
    if (!_bootImg) return;
    int ye = y0 + h;
    if (ye > SCREEN_H) ye = SCREEN_H;
    for (int y = (y0 < 0 ? 0 : y0); y < ye; y++)
        restoreImgRow(y, x0, w);
}

// ── Dot drawing ───────────────────────────────────────────────────────────────
static void drawDots(int8_t active) {
    for (int8_t i = 0; i < DOT_N; i++) {
        int16_t x = DOT_X0 + i * DOT_GAP;
        int d = abs((int)i - (int)active);
        float br = (d==0) ? 1.0f : (d==1) ? 0.45f : (d==2) ? 0.12f : 0.04f;
        uint8_t r = C_LO_R + (uint8_t)((C_HI_R - C_LO_R) * br);
        uint8_t g = C_LO_G + (uint8_t)((C_HI_G - C_LO_G) * br);
        uint8_t b = C_LO_B + (uint8_t)((C_HI_B - C_LO_B) * br);
        lcd.fillCircle(x, DOT_Y, DOT_R, lcd.color888(r, g, b));
    }
}

// ── Fallback text animation ───────────────────────────────────────────────────
static void animLogo() {
    lcd.setTextDatum(MC_DATUM);
    lcd.setFont(&lgfx::fonts::FreeSansBold18pt7b);
    lcd.setTextSize(1);
    const int STEPS = 22;
    for (int i = 0; i <= STEPS; i++) {
        if (i > 0) {
            lcd.setTextColor(lerpCol(C_BG_R,C_BG_G,C_BG_B,
                                     C_TXT_R,C_TXT_G,C_TXT_B, i-1, STEPS));
            lcd.drawString("AleksOS", CX, CY);
        }
        lcd.setTextColor(lerpCol(C_BG_R,C_BG_G,C_BG_B,
                                  C_TXT_R,C_TXT_G,C_TXT_B, i, STEPS));
        lcd.drawString("AleksOS", CX, CY);
        delay(16);
    }
}

static void animSubtitle() {
    lcd.setTextDatum(MC_DATUM);
    lcd.setFont(&lgfx::fonts::Font2);
    lcd.setTextSize(1);
    const int STEPS = 14;
    for (int i = 0; i <= STEPS; i++) {
        lcd.setTextColor(lerpCol(C_BG_R,C_BG_G,C_BG_B,C_SUB_R,C_SUB_G,C_SUB_B,i,STEPS));
        lcd.drawString("RETRO ESP CONSOLE", CX, CY + 38);
        lcd.setTextColor(lerpCol(C_BG_R,C_BG_G,C_BG_B,C_VER_R,C_VER_G,C_VER_B,i,STEPS));
        lcd.drawString(FIRMWARE_VERSION, CX, CY + 56);
        delay(12);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PUBLIC API
// ─────────────────────────────────────────────────────────────────────────────

void bootLogoLoad() {
    if (!psramFound()) {
        Serial.println("[BOOT] No PSRAM — using fallback");
        return;
    }

    const size_t SIZE = (size_t)SCREEN_W * SCREEN_H * 2;  // 153 600 bytes

    _bootImg = (uint8_t *)ps_malloc(SIZE);
    if (!_bootImg) {
        Serial.println("[BOOT] ps_malloc failed — using fallback");
        return;
    }

    File f = SD.open("/boot.raw");
    if (!f) {
        Serial.println("[BOOT] /boot.raw not found — using fallback");
        free(_bootImg); _bootImg = nullptr;
        return;
    }

    size_t got = f.read(_bootImg, SIZE);
    f.close();

    if (got < SIZE) {
        Serial.printf("[BOOT] boot.raw too small (%u < %u) — fallback\n",
                      (unsigned)got, (unsigned)SIZE);
        free(_bootImg); _bootImg = nullptr;
        return;
    }

    // ── Pre-darken bottom strip ───────────────────────────────────────────────
    // The image bottom (y ≥ DARK_START_Y) is dimmed to 25 % so the white dot
    // spinner and status text are legible over any photo content.
    for (int y = DARK_START_Y; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            int off = (y * SCREEN_W + x) * 2;
            // Each pixel stored LE pre-inverted: raw = ~rgb565 & 0xFFFF
            uint16_t raw   = _bootImg[off] | ((uint16_t)_bootImg[off+1] << 8);
            uint16_t pixel = (~raw) & 0xFFFF;           // un-invert → RGB565
            // Reduce to 25 % brightness (>> 2 on each channel)
            uint8_t r5 = ((pixel >> 11) & 0x1F) >> 2;
            uint8_t g6 = ((pixel >>  5) & 0x3F) >> 2;
            uint8_t b5 = ( pixel        & 0x1F) >> 2;
            uint16_t dark     = ((uint16_t)r5 << 11) | ((uint16_t)g6 << 5) | b5;
            uint16_t dark_inv = (~dark) & 0xFFFF;
            _bootImg[off]     = dark_inv & 0xFF;
            _bootImg[off + 1] = (dark_inv >> 8) & 0xFF;
        }
    }

    Serial.printf("[BOOT] boot.raw loaded (%u bytes)\n", (unsigned)got);
}

// ─────────────────────────────────────────────────────────────────────────────
void bootScreenRun() {
    ledSet(LED_BOOT);

    if (_bootImg) {
        // ── Image mode: push full pre-inverted frame ──────────────────────────
        lcd.startWrite();
        lcd.setWindow(0, 0, SCREEN_W - 1, SCREEN_H - 1);
        lcd.writePixels((uint16_t *)_bootImg, SCREEN_W * SCREEN_H, true);
        lcd.endWrite();
        delay(80);
    } else {
        // ── Fallback text animation ───────────────────────────────────────────
        lcd.fillScreen(lcd.color888(C_BG_R, C_BG_G, C_BG_B));
        delay(80);
        animLogo();
        delay(60);
        animSubtitle();
        delay(40);
    }

    drawDots(_dotActive);
    _dotNextMs = millis() + DOT_STEP_MS;
}

// ─────────────────────────────────────────────────────────────────────────────
void bootTick() {
    if (millis() < _dotNextMs) return;

    if (_bootImg) {
        // Restore image pixels under each dot bounding box
        for (int8_t i = 0; i < DOT_N; i++) {
            int x = DOT_X0 + i * DOT_GAP;
            int r = DOT_R + 1;
            restoreImgRect(x - r, DOT_Y - r, r * 2 + 1, r * 2 + 1);
        }
    } else {
        for (int8_t i = 0; i < DOT_N; i++) {
            lcd.fillCircle(DOT_X0 + i * DOT_GAP, DOT_Y,
                           DOT_R + 1, lcd.color888(C_BG_R, C_BG_G, C_BG_B));
        }
    }

    _dotActive = (_dotActive + 1) % DOT_N;
    drawDots(_dotActive);
    _dotNextMs = millis() + DOT_STEP_MS;
}

// ─────────────────────────────────────────────────────────────────────────────
void bootProgress(uint8_t /*pct*/, const char *msg) {
    // Erase previous status line
    if (_bootImg) {
        restoreImgRect(0, MSG_Y - 10, SCREEN_W, 20);
    } else {
        lcd.fillRect(0, MSG_Y - 10, SCREEN_W, 20,
                     lcd.color888(C_BG_R, C_BG_G, C_BG_B));
    }

    if (!msg || !msg[0]) return;

    lcd.setFont(&lgfx::fonts::Font2);
    lcd.setTextDatum(MC_DATUM);
    if (_bootImg)
        lcd.setTextColor(lcd.color888(C_MSG_R, C_MSG_G, C_MSG_B));
    else
        lcd.setTextColor(lcd.color888(C_MSG_FBR, C_MSG_FBG, C_MSG_FBB));
    lcd.drawString(msg, CX, MSG_Y);

    bootTick();
}

// ─────────────────────────────────────────────────────────────────────────────
void bootScreenDone() {
    bootProgress(100, "Ready!");
    delay(400);

    // Smooth fade to black
    for (int b = 80; b >= 0; b -= 5) {
        lcd.setBrightness((uint8_t)map(max(b, 0), 0, 100, 0, 255));
        delay(20);
    }
    lcd.fillScreen(TFT_BLACK);
    // Brightness restored later by setBrightness() in toMenu()
    lcd.setBrightness((uint8_t)map(80, 0, 100, 0, 255));

    // Release PSRAM buffer
    if (_bootImg) {
        free(_bootImg);
        _bootImg = nullptr;
    }
}
