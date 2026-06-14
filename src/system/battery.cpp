// ── battery.cpp ─ Мониторинг заряда LiPo ─────────────────────────────────────
// Схема: LiPo(+) → R1=470kΩ → GPIO35 → R2=470kΩ → GND
// Делитель напряжения: V_pin = V_bat × R2/(R1+R2) = V_bat / 2
// Восстановление: V_bat = V_pin × 2
//
// ESP32 ADC: 12-bit, ATT_11db → шкала ≈ 0..3.3V
// Итого: V_bat = raw / 4095 × 3.3 × (R1+R2)/R2
//
// Определение зарядки: программное — по тренду напряжения.
// Два подряд измерения с ростом >15мВ каждые 30 с = зарядка идёт.
// Дополнительный GPIO не нужен.

#include "system/battery.h"
#include "config.h"
#include <Arduino.h>

// ── Кривая разряда LiPo 1S ───────────────────────────────────────────────────
static const struct { float v; int8_t pct; } kCurve[] = {
    { 4.20f, 100 },
    { 4.10f,  95 },
    { 4.00f,  88 },
    { 3.90f,  78 },
    { 3.80f,  65 },
    { 3.70f,  50 },
    { 3.60f,  35 },
    { 3.50f,  20 },
    { 3.40f,  10 },
    { 3.30f,   5 },
    { 3.00f,   0 },
};
static constexpr int kCurveN = (int)(sizeof(kCurve) / sizeof(kCurve[0]));

// ── Состояние ─────────────────────────────────────────────────────────────────
static int   _batPct   = -1;     // -1 = не измерено
static float _batV     = 0.0f;
static float _batVPrev = 0.0f;   // предыдущее измерение (для тренда)
static bool  _charging = false;  // идёт ли зарядка

// ── Внутренние функции ────────────────────────────────────────────────────────

static float _rawToVbat(int32_t raw) {
#if BAT_ADC_PIN >= 0
    constexpr float VREF = 3.3f;
    constexpr float DIV  = (float)(BAT_R_TOP + BAT_R_BOT) / (float)BAT_R_BOT;
    return raw * (VREF / 4095.0f) * DIV;
#else
    (void)raw; return 0.0f;
#endif
}

static int _voltToPct(float v) {
    if (v >= kCurve[0].v)            return 100;
    if (v <= kCurve[kCurveN - 1].v)  return 0;
    for (int i = 0; i < kCurveN - 1; i++) {
        if (v >= kCurve[i + 1].v) {
            float span = kCurve[i].v - kCurve[i + 1].v;
            float pos  = v           - kCurve[i + 1].v;
            int   lo   = kCurve[i + 1].pct;
            int   hi   = kCurve[i].pct;
            return lo + (int)((pos / span) * (float)(hi - lo) + 0.5f);
        }
    }
    return 0;
}

// ── Публичное API ─────────────────────────────────────────────────────────────

void batteryInit() {
#if BAT_ADC_PIN >= 0
    analogSetAttenuation(ADC_11db);
    pinMode(BAT_ADC_PIN, INPUT);
    batteryUpdate();
#endif
}

void batteryUpdate() {
#if BAT_ADC_PIN >= 0
    // 16 замеров × 2 мс = 32 мс. Пауза нужна для 470кОм делителя:
    // заряд sample-and-hold конденсатора через 235кОм источника.
    int32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(BAT_ADC_PIN);
        delay(2);
    }
    float newV = _rawToVbat(sum >> 4);

    // ── Определение зарядки ───────────────────────────────────────────────────
    // Принцип: при зарядке TP4056 напряжение стабильно растёт каждые 30 с.
    // Порог +20мВ за интервал = зарядка.  -10мВ = разрядка.  Иначе — гистерезис.
    if (_batVPrev > 0.5f) {
        float delta = newV - _batVPrev;
        if      (delta >  0.020f) _charging = true;   // напряжение растёт
        else if (delta < -0.010f) _charging = false;  // напряжение падает
        // иначе: стабильно — сохраняем предыдущее состояние (гистерезис)
    }

    _batVPrev = newV;
    _batV     = newV;
    _batPct   = _voltToPct(_batV);
#endif
}

int   batteryPercent()  { return _batPct; }
float batteryVoltage()  { return _batV; }
bool  batteryCharging() { return _charging; }
