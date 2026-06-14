// ── battery.cpp ─ Мониторинг заряда LiPo ─────────────────────────────────────
// Схема: LiPo(+) → R1=100kΩ → GPIO35 → R2=100kΩ → GND
// Делитель напряжения: V_pin = V_bat × R2/(R1+R2) = V_bat / 2
// Восстановление: V_bat = V_pin × 2
//
// ESP32 ADC: 12-bit, ATT_11db → шкала ≈ 0..3.3V (реально до ~3.1V точно)
// Vref = 3.3V: V_pin = raw / 4095 × 3.3
// Итого: V_bat = raw / 4095 × 3.3 × (R1+R2)/R2

#include "system/battery.h"  // src/system/battery.h (via -I src build flag)
#include "config.h"          // include/config.h     (via -I include build flag)
#include <Arduino.h>

// ── Кривая разряда LiPo 1S ───────────────────────────────────────────────────
// Пары (напряжение В, ёмкость %).  Данные для типичной LiPo 3.7V 1S.
static const struct { float v; int8_t pct; } kCurve[] = {
    { 4.20f, 100 },
    { 4.10f,  95 },
    { 4.00f,  88 },
    { 3.90f,  78 },
    { 3.80f,  65 },
    { 3.70f,  50 },   // номинальное напряжение
    { 3.60f,  35 },
    { 3.50f,  20 },
    { 3.40f,  10 },
    { 3.30f,   5 },
    { 3.00f,   0 },
};
static constexpr int kCurveN = (int)(sizeof(kCurve) / sizeof(kCurve[0]));

// ── Состояние ─────────────────────────────────────────────────────────────────
static int   _batPct = -1;    // -1 = не измерено
static float _batV   = 0.0f;

// ── Внутренние функции ────────────────────────────────────────────────────────

// raw (0-4095) → реальное напряжение батареи (В)
static float _rawToVbat(int32_t raw) {
#if BAT_ADC_PIN >= 0
    // ESP32 ADC при 11db даёт ~0..3.3V (лучше использовать 3.1V как реальную Vref,
    // но 3.3 даёт разумную оценку без esp_adc_cal)
    constexpr float VREF  = 3.3f;
    constexpr float DIV   = (float)(BAT_R_TOP + BAT_R_BOT) / (float)BAT_R_BOT;
    return raw * (VREF / 4095.0f) * DIV;
#else
    (void)raw;
    return 0.0f;
#endif
}

// напряжение (В) → проценты по кривой разряда (линейная интерполяция)
static int _voltToPct(float v) {
    if (v >= kCurve[0].v)            return 100;
    if (v <= kCurve[kCurveN - 1].v)  return 0;
    for (int i = 0; i < kCurveN - 1; i++) {
        if (v >= kCurve[i + 1].v) {
            float span = kCurve[i].v       - kCurve[i + 1].v;
            float pos  = v                 - kCurve[i + 1].v;
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
    analogSetAttenuation(ADC_11db);   // шкала 0-3.3V на всех ADC1 каналах
    pinMode(BAT_ADC_PIN, INPUT);
    batteryUpdate();                   // первое измерение немедленно
#endif
}

void batteryUpdate() {
#if BAT_ADC_PIN >= 0
    // 16 замеров с паузой 2 мс = 32 мс суммарно.
    // Пауза 2 мс нужна при 470кОм делителе — даёт время зарядить
    // конденсатор sample-and-hold АЦП (~100 pF) через 235кОм источника.
    int32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(BAT_ADC_PIN);
        delay(2);
    }
    _batV   = _rawToVbat(sum >> 4);   // sum / 16
    _batPct = _voltToPct(_batV);
#endif
}

// Возвращает 0-100, или -1 если батарея не подключена (BAT_ADC_PIN=-1)
int batteryPercent() {
    return _batPct;
}

// Напряжение батареи в вольтах (0.0 если нет данных)
float batteryVoltage() {
    return _batV;
}
