// ─────────────────────────────────────────────────────────────────────────────
//  boot_screen.cpp  —  AleksOS BETA v11.24  —  Windows-style boot screen
//
//  Дизайн: чёрный фон, лого AleksOS по центру, под ним RETRO ESP CONSOLE,
//  внизу экрана — анимация «бегущих точек» (Windows 10/11 spinner).
//  Всё рисуется через LovyanGFX, без статических bitmap-массивов.
//
//  Публичный API:
//    bootScreenRun()         — вступительная анимация
//    bootProgress(pct, msg)  — обновить строку статуса
//    bootTick()              — шаг анимации точек (вызывать ~100 мс)
//    bootScreenDone()        — fade-out и очистка
// ─────────────────────────────────────────────────────────────────────────────

#include "boot_screen.h"
#include "display_manager.h"
#include "led.h"
#include "config.h"
#include <Arduino.h>

// ── Геометрия ─────────────────────────────────────────────────────────────
static constexpr int16_t CX = SCREEN_W / 2;      // 160
static constexpr int16_t CY = SCREEN_H / 2 - 18; // 102  (центр логотипа)

// Область точек
static constexpr int16_t DOT_Y   = SCREEN_H - 44; // y центра точек
static constexpr int8_t  DOT_N   = 5;              // количество точек
static constexpr int8_t  DOT_R   = 4;              // радиус точки
static constexpr int8_t  DOT_GAP = 14;             // шаг между центрами
static constexpr int16_t DOT_X0  = CX - (DOT_N - 1) * DOT_GAP / 2; // x первой

// Строка статуса
static constexpr int16_t MSG_Y   = SCREEN_H - 18;

// ── Цвета ─────────────────────────────────────────────────────────────────
// Всё в RGB888 → конвертируем через lcd.color888(r,g,b)
static constexpr uint8_t C_BG_R = 0,   C_BG_G = 0,   C_BG_B = 0;      // чёрный фон
static constexpr uint8_t C_LO_R = 30,  C_LO_G = 30,  C_LO_B = 30;     // тёмно-серый (неакт. точка)
static constexpr uint8_t C_HI_R = 220, C_HI_G = 220, C_HI_B = 220;    // почти белый (акт. точка)
static constexpr uint8_t C_TXT_R= 200, C_TXT_G= 200, C_TXT_B= 200;    // логотип
static constexpr uint8_t C_SUB_R= 90,  C_SUB_G= 90,  C_SUB_B= 90;     // подпись
static constexpr uint8_t C_VER_R= 55,  C_VER_G= 55,  C_VER_B= 55;     // версия
static constexpr uint8_t C_MSG_R= 65,  C_MSG_G= 65,  C_MSG_B= 65;     // статус

// ── Состояние анимации точек ──────────────────────────────────────────────
static int8_t   _dotActive   = 0;       // индекс «светлой» точки 0–4
static uint32_t _dotNextMs   = 0;       // когда переключить следующую
static constexpr uint16_t DOT_STEP_MS = 100; // скорость: 1 точка/100 мс

// ── Вспомогательные ───────────────────────────────────────────────────────

// Линейная интерполяция двух цветов (каждый как r,g,b)
static uint32_t lerpCol(
    uint8_t fr, uint8_t fg, uint8_t fb,
    uint8_t tr, uint8_t tg, uint8_t tb,
    int step, int steps)
{
    if (steps == 0) return lcd.color888(tr, tg, tb);
    return lcd.color888(
        (uint8_t)(fr + (int)(tr - fr) * step / steps),
        (uint8_t)(fg + (int)(tg - fg) * step / steps),
        (uint8_t)(fb + (int)(tb - fb) * step / steps));
}

// ── Рисуем фон ────────────────────────────────────────────────────────────
static void drawBg() {
    lcd.fillScreen(lcd.color888(C_BG_R, C_BG_G, C_BG_B));
}

// ── Логотип «AleksOS» fade-in ─────────────────────────────────────────────
static void animLogo() {
    lcd.setTextDatum(MC_DATUM);
    lcd.setFont(&lgfx::fonts::FreeSansBold18pt7b);
    lcd.setTextSize(1);

    const int STEPS = 22;
    for (int i = 0; i <= STEPS; i++) {
        // Гасим предыдущий текст (рисуем bg-цветом) только если не первый кадр
        if (i > 0) {
            uint32_t prev = lerpCol(C_BG_R, C_BG_G, C_BG_B,
                                    C_TXT_R, C_TXT_G, C_TXT_B, i - 1, STEPS);
            lcd.setTextColor(prev);
            lcd.drawString("AleksOS", CX, CY);
        }
        uint32_t cur = lerpCol(C_BG_R, C_BG_G, C_BG_B,
                               C_TXT_R, C_TXT_G, C_TXT_B, i, STEPS);
        lcd.setTextColor(cur);
        lcd.drawString("AleksOS", CX, CY);
        delay(16);
    }
}

// ── Подпись + версия fade-in ──────────────────────────────────────────────
static void animSubtitle() {
    lcd.setTextDatum(MC_DATUM);
    lcd.setFont(&lgfx::fonts::Font2);
    lcd.setTextSize(1);

    const int STEPS = 14;
    for (int i = 0; i <= STEPS; i++) {
        uint32_t cs = lerpCol(C_BG_R, C_BG_G, C_BG_B,
                              C_SUB_R, C_SUB_G, C_SUB_B, i, STEPS);
        uint32_t cv = lerpCol(C_BG_R, C_BG_G, C_BG_B,
                              C_VER_R, C_VER_G, C_VER_B, i, STEPS);
        lcd.setTextColor(cs);
        lcd.drawString("RETRO ESP CONSOLE", CX, CY + 38);
        lcd.setTextColor(cv);
        lcd.drawString(FIRMWARE_VERSION, CX, CY + 56);  // ← из config.h
        delay(12);
    }
}

// ── Рисуем один кадр точек ────────────────────────────────────────────────
static void drawDots(int8_t active) {
    for (int8_t i = 0; i < DOT_N; i++) {
        int16_t x = DOT_X0 + i * DOT_GAP;

        // Яркость: активная = HI, остальные = LO с небольшим градиентом
        // Создаём «хвост» — соседние точки чуть светлее
        int dist = (int)abs((int)i - (int)active);
        // dist 0→1: 1.0, 1→0.5, 2→0.15, 3+→0.05
        float brightness;
        if      (dist == 0) brightness = 1.0f;
        else if (dist == 1) brightness = 0.45f;
        else if (dist == 2) brightness = 0.12f;
        else                brightness = 0.04f;

        uint8_t r = C_LO_R + (uint8_t)((C_HI_R - C_LO_R) * brightness);
        uint8_t g = C_LO_G + (uint8_t)((C_HI_G - C_LO_G) * brightness);
        uint8_t b = C_LO_B + (uint8_t)((C_HI_B - C_LO_B) * brightness);

        lcd.fillCircle(x, DOT_Y, DOT_R, lcd.color888(r, g, b));
    }
}

// ── Публичные функции ─────────────────────────────────────────────────────

void bootScreenRun() {
    ledSet(LED_BOOT);

    drawBg();
    delay(80);

    animLogo();
    delay(60);

    animSubtitle();
    delay(40);

    // Начальный кадр точек
    drawDots(_dotActive);
    _dotNextMs = millis() + DOT_STEP_MS;
}

// ─────────────────────────────────────────────────────────────────────────────
void bootTick() {
    if (millis() < _dotNextMs) return;

    // Стираем старые точки (заливаем фоном)
    for (int8_t i = 0; i < DOT_N; i++) {
        lcd.fillCircle(DOT_X0 + i * DOT_GAP, DOT_Y,
                       DOT_R + 1, lcd.color888(C_BG_R, C_BG_G, C_BG_B));
    }

    // Сдвигаем активную точку
    _dotActive = (_dotActive + 1) % DOT_N;
    drawDots(_dotActive);

    _dotNextMs = millis() + DOT_STEP_MS;
}

// ─────────────────────────────────────────────────────────────────────────────
void bootProgress(uint8_t pct, const char* msg) {
    // Стираем строку статуса — перерисовываем фон на этой строке
    lcd.fillRect(0, MSG_Y - 10, SCREEN_W, 20, lcd.color888(C_BG_R, C_BG_G, C_BG_B));

    if (!msg || !msg[0]) return;
    lcd.setFont(&lgfx::fonts::Font2);
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(lcd.color888(C_MSG_R, C_MSG_G, C_MSG_B));
    lcd.drawString(msg, CX, MSG_Y);

    // Тикаем точки при вызове из main
    bootTick();
}

// ─────────────────────────────────────────────────────────────────────────────
void bootScreenDone() {
    // Финальное состояние
    bootProgress(100, "Ready!");
    delay(400);

    // Плавное угасание через яркость дисплея
    for (int b = 80; b >= 0; b -= 5) {
        lcd.setBrightness((uint8_t)map(max(b, 0), 0, 100, 0, 255));
        delay(20);
    }
    lcd.fillScreen(TFT_BLACK);
    // Яркость восстановит setBrightness() из settings при первом вызове
    lcd.setBrightness((uint8_t)map(80, 0, 100, 0, 255));
}
