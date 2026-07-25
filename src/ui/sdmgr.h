#pragma once
#include <Arduino.h>
#include "config.h"

// SD-менеджер — общий браузер файлов SD-карты
// Навигация: UP/DOWN=скролл, A=открыть/войти, B=назад/выход
// Тач: тап на строку=выбор/двойной=открыть, подвал НАЗАД=выход

void    sdMgrOpen();
uint8_t sdMgrHandleTouch(int x, int y);
uint8_t sdMgrNavBtn(uint8_t btn);
