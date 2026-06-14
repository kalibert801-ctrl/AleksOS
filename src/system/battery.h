#pragma once
// ── Мониторинг заряда LiPo аккумулятора ──────────────────────────────────────
// Схема: LiPo(+) → R_TOP(100kΩ) → GPIO35 → R_BOT(100kΩ) → GND
// Делитель × 2: 4.2V → 2.1V на пине, 3.0V → 1.5V на пине (ADC11db шкала 0-3.3V)
//
// Если BAT_ADC_PIN = -1 (не подключён) — все функции возвращают -1 / 0.0f.
// batteryPercent() = -1 означает «данные недоступны», UI показывает знак «?».

void  batteryInit();        // вызывать в setup()
void  batteryUpdate();      // вызывать раз в ~5 с (menuTimeTick делает это)
int   batteryPercent();     // 0-100, или -1 если не подключён
float batteryVoltage();     // напряжение батареи в вольтах
bool  batteryCharging();    // true если TP4056 заряжает (по тренду напряжения)
