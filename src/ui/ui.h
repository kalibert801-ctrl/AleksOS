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

// Запросить версию Pico при старте ESP32 (вызывать после buttons.init())
void settingsPrefetchPicoVer();

// ── Навигация с кнопок Pico ───────────────────────────────
uint8_t settingsNavBtn(uint8_t btn);
uint8_t btnMapNavBtn(uint8_t btn);

// ── WiFi менеджер ─────────────────────────────────────────
// Возвращает BTN_B при нажатии Back, 0 иначе.
void    wifiManagerDraw();
uint8_t wifiManagerHandleTouch(int x, int y);
uint8_t wifiManagerNavBtn(uint8_t btn);

// ── Экран ввода пароля WiFi ────────────────────────────────
// ssid — имя сети, для которой вводится пароль.
// Возвращает BTN_B при Cancel, BTN_A при OK.
void    wifiKeyboardDraw(const char *ssid);
uint8_t wifiKeyboardHandleTouch(int x, int y);
uint8_t wifiKeyboardNavBtn(uint8_t btn);
// Получить введённый пароль (заполняется при BTN_A).
const char *wifiKeyboardGetPassword();

// ── Экран OTA-обновления ESP32 ────────────────────────────
void otaScreen();

// ── Экран OTA-обновления Pico ─────────────────────────────
// picoUrl — прямая ссылка на pico_firmware.bin (info.picoUrl из otaCheckUpdate).
void picoOtaScreen(const char *picoUrl);

// ── Всплывающее окно ──────────────────────────────────────
// Показывает popup с заголовком title и строкой msg.
// Ждёт tap или до timeoutMs мс.
void popupShow(const char *title, const char *msg, uint32_t timeoutMs = 3000);

// ── Вспомогательные (WiFi) ────────────────────────────────
// Возвращает SSID выбранной в списке сети.
const char *wifiManagerSelectedSSID();
// Возвращает true если выбранная сеть зашифрована.
bool wifiManagerSelectedEncrypted();
// Сбросить буфер клавиатуры (вызвать перед открытием).
void wifiKeyboardReset();

