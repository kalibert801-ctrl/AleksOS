#pragma once
#include <stdint.h>   // uint32_t, float — нужен для BattStats до Arduino.h
// ── Мониторинг заряда LiPo аккумулятора ──────────────────────────────────────
// Схема: LiPo(+) → R_TOP(100kΩ) → GPIO35 → R_BOT(100kΩ) → GND  + 100нФ GND
// Делитель × 2: 4.2V → 2.1V, 3.0V → 1.5V (ADC11db шкала 0-3.3V).
//
// Определение зарядки: программное, по тренду напряжения (+20мВ/цикл).
// BAT_ADC_PIN = -1 → все функции возвращают -1 / 0.0f / false / нули.

// ── ADC / charging ────────────────────────────────────────────────────────────
void  batteryInit();        // вызывать в setup() (ADC init + первое измерение)
void  batteryUpdate();      // вызывать раз в ~5 с (menuTimeTick делает это)
int   batteryPercent();     // 0-100, или -1 если не подключён
float batteryVoltage();     // напряжение батареи в вольтах
bool  batteryCharging();    // true если TP4056 заряжает (по тренду)

// ── Battery stats (persistent on SD) ─────────────────────────────────────────
// Данные сохраняются в /battery_stats.json при подключении зарядки
// и каждые 5 минут работы.
struct BattStats {
    uint32_t sessionSec;  // время работы с момента последнего подключения зарядки
    uint32_t totalSec;    // суммарное время работы за всё время
    uint32_t cycles;      // сколько раз подключалась зарядка
};

void       batteryStatsLoad();               // вызывать после SD init (main.cpp setup)
void       batteryStatsSave();               // принудительное сохранение
void       batteryStatsAddTime(uint32_t sec);// добавить секунды к session + total
BattStats  batteryStatsGet();                // получить текущую статистику
