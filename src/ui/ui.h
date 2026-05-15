#pragma once
#include <Arduino.h>
#include "config.h"
#include "input/touch_handler.h"

#define DP_LEFT  0
#define DP_UP    1
#define DP_DOWN  2
#define DP_RIGHT 3

int  getDpadBtn(int x, int y);

void drawBoot();
void drawSDError();

// ── ROM меню ──────────────────────────────────────────────
uint8_t menuHandleTouch(int x, int y, int &romAction);
void menuDraw();
void menuScrollUp();
void menuScrollDown();
void menuTimeTick();     // вызывать в loop() — обновляет часы в нижней панели
int  menuSelected();
void showRomInfo(int idx);

// ── Настройки ─────────────────────────────────────────────
uint8_t settingsHandleTouch(int x, int y);
void settingsDraw();
void settingsScrollUp();
void settingsScrollDown();

// ── Переназначение кнопок ─────────────────────────────────
void    btnMapDraw();
uint8_t btnMapHandleTouch(int x, int y);
void    btnMapApply();

// ── Навигация с кнопок Pico ───────────────────────────────
uint8_t settingsNavBtn(uint8_t btn);
uint8_t btnMapNavBtn(uint8_t btn);
