#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  time_manager.h  —  Модульные часы для AleksOS
//  Не зависит от RTC. Отсчёт через millis() от установленного пользователем
//  базового времени. При перезагрузке читает settings.timeH / settings.timeM.
// ─────────────────────────────────────────────────────────────────────────────
#pragma once
#include <Arduino.h>

void    timeInit();                    // вызвать в setup() ПОСЛЕ cfgLoad()
void    timeUpdate();                  // вызвать в loop() — зарезервировано для RTC
void    timeSet(uint8_t h, uint8_t m); // установить время (обновляет settings)
uint8_t timeGetH();                    // текущий час 0–23
uint8_t timeGetM();                    // текущая минута 0–59
String  timeGetString();               // "HH:MM"
