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

// -- ROM menu ----------------------------------------------------------
uint8_t menuHandleTouch(int x, int y, int &romAction);
void menuDraw();
void menuScrollUp();
void menuScrollDown();
void menuTimeTick();     // call in loop() -- updates clock in bottom bar
int  menuSelected();
void showRomInfo(int idx);

// -- Settings ----------------------------------------------------------
uint8_t settingsHandleTouch(int x, int y);
void settingsDraw();
void settingsScrollUp();
void settingsScrollDown();

// -- Button remap ------------------------------------------------------
void    btnMapDraw();
uint8_t btnMapHandleTouch(int x, int y);
void    btnMapApply();

// Prefetch Pico version at startup (call after buttons.init())
void settingsPrefetchPicoVer();

// -- Pico button navigation --------------------------------------------
uint8_t settingsNavBtn(uint8_t btn);
uint8_t btnMapNavBtn(uint8_t btn);

// -- WiFi manager ------------------------------------------------------
// Returns BTN_B on Back, 0 otherwise.
void    wifiManagerDraw();
uint8_t wifiManagerHandleTouch(int x, int y);
uint8_t wifiManagerNavBtn(uint8_t btn);

// -- Text keyboard (shared: WiFi password + file rename) ---------------
// wifiKeyboardDraw     -- WiFi mode: shows "Network: <ssid>"
// wifiKeyboardSetLabel -- override top label line (for file manager)
// wifiKeyboardSetInitial -- pre-fill input buffer
// wifiKeyboardSetMask  -- true = show dots (password), false = show chars (rename)
// Returns BTN_B on Cancel, BTN_A on OK.
void    wifiKeyboardDraw(const char *ssid);
void    wifiKeyboardSetLabel(const char *label);
void    wifiKeyboardSetInitial(const char *text);
void    wifiKeyboardSetMask(bool mask);
uint8_t wifiKeyboardHandleTouch(int x, int y);
uint8_t wifiKeyboardNavBtn(uint8_t btn);
// Get typed text (valid after BTN_A).
const char *wifiKeyboardGetPassword();
// Reset keyboard state (call before opening keyboard).
void wifiKeyboardReset();

// -- OTA screens -------------------------------------------------------
void otaScreen();
void picoOtaScreen(const char *picoUrl);

// -- Popup -------------------------------------------------------------
void popupShow(const char *title, const char *msg, uint32_t timeoutMs = 3000);

// -- WiFi helpers ------------------------------------------------------
const char *wifiManagerSelectedSSID();

// -- File manager ------------------------------------------------------
// Access: BTN_LEFT in main menu.  Returns BTN_B on exit.
void    fileMgrDraw();
uint8_t fileMgrHandleTouch(int x, int y);
uint8_t fileMgrNavBtn(uint8_t btn);
int     fileMgrSelected();
