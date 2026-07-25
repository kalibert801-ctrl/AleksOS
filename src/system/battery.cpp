// ── battery.cpp ─ Мониторинг заряда LiPo + статистика ───────────────────────
// Схема: LiPo(+) → R1=100kΩ → GPIO35 → R2=100kΩ → GND  + 100нФ на GPIO35→GND
// Делитель: V_pin = V_bat / 2.  ADC: 12-bit, ATT_11db (0–3.3V).
// V_bat = raw / 4095 × 3.3 × (R1+R2)/R2 × BAT_CAL_GAIN

#include "system/battery.h"
#include "config.h"
#include <Arduino.h>
#include <SD.h>
#include <ArduinoJson.h>

#define BAT_STATS_FILE  "/battery_stats.json"
#define BAT_SAVE_EVERY  300u   // авто-сохранение каждые 300 с (5 мин)

// ── Кривая разряда LiPo 1S ───────────────────────────────────────────────────
static const struct { float v; int8_t pct; } kCurve[] = {
    { 4.20f, 100 }, { 4.10f, 95 }, { 4.00f, 88 }, { 3.90f, 78 },
    { 3.80f, 65  }, { 3.70f, 50 }, { 3.60f, 35 }, { 3.50f, 20 },
    { 3.40f, 10  }, { 3.30f,  5 }, { 3.00f,  0 },
};
static constexpr int kCurveN = (int)(sizeof(kCurve) / sizeof(kCurve[0]));

// ── Состояние ─────────────────────────────────────────────────────────────────
static int      _batPct       = -1;
static float    _batV         = 0.0f;
static bool     _charging     = false;
static bool     _prevCharging = false;

// Детектирование зарядки: сравниваем каждые 30 секунд.
// Порог 100 мВ за 30 с — уверенно выше шума АЦП (~30 мВ).
// При первом подключении TP4056 напряжение скачет на 100-300 мВ мгновенно.
static float    _batVFor30s   = 0.0f;  // снимок напряжения 30 секунд назад
static uint32_t _chargeChkMs  = 0;     // момент последней проверки
static int8_t   _chargeConfirm = 0;    // счётчик подтверждений (+ зарядка, - разрядка)

static BattStats _stats       = { 0, 0, 0 };
static uint32_t  _lastSaveMs  = 0;   // для авто-сохранения каждые 5 мин

// ── Внутренние функции ────────────────────────────────────────────────────────

static float _rawToVbat(int32_t raw) {
#if BAT_ADC_PIN >= 0
    constexpr float VREF = 3.3f;
    constexpr float DIV  = (float)(BAT_R_TOP + BAT_R_BOT) / (float)BAT_R_BOT;
    return raw * (VREF / 4095.0f) * DIV * BAT_CAL_GAIN;
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
            int   lo   = kCurve[i + 1].pct, hi = kCurve[i].pct;
            return lo + (int)((pos / span) * (float)(hi - lo) + 0.5f);
        }
    }
    return 0;
}

// ── Публичное API: ADC ────────────────────────────────────────────────────────

void batteryInit() {
#if BAT_ADC_PIN >= 0
    analogSetPinAttenuation(BAT_ADC_PIN, ADC_11db);  // только GPIO35, не все каналы
    pinMode(BAT_ADC_PIN, INPUT);
    batteryUpdate();
#endif
}

void batteryUpdate() {
#if BAT_ADC_PIN >= 0
    // 16 замеров × 2 мс = 32 мс.
    // ТРЕБУЕТСЯ: конденсатор 100нФ между GPIO35 и GND для стабилизации напряжения.
    // Без него S/H АЦП (~100пФ) не успевает зарядиться через 50кОм источника (RC≈5мкс).
    delay(5);   // пре-settle: даём конденсатору зарядиться перед первым замером
    int32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(BAT_ADC_PIN);
        delay(1);
    }
    float newV = _rawToVbat(sum >> 4);

    // ── Детектирование зарядки ────────────────────────────────────────────────
    // Fast path: скачок >80мВ за 5 с (между соседними вызовами) — зарядник только что подключён.
    // _batV здесь ещё не обновлён, поэтому содержит предыдущий замер (5 с назад).
    if (!_charging && _batV > 0.5f && (newV - _batV) > 0.080f) {
        _charging      = true;
        _chargeConfirm = 2;
    }

    // Slow path: каждые 30 с сравниваем с предыдущим снимком.
    // Порог снижен до 30 мВ — покрывает стационарный режим CC (10-40 мВ/30 с).
    uint32_t ms = millis();
    if (_batVFor30s < 0.1f || _chargeChkMs == 0) {
        _batVFor30s  = newV;
        _chargeChkMs = ms;
    } else if (ms - _chargeChkMs >= 30000u) {
        float delta = newV - _batVFor30s;
        _batVFor30s  = newV;
        _chargeChkMs = ms;

        if (delta > 0.030f) {
            // Напряжение выросло на >30мВ за 30с → зарядка (одно подтверждение достаточно)
            _chargeConfirm = min((int8_t)2, (int8_t)(_chargeConfirm + 1));
            if (_chargeConfirm >= 1) _charging = true;
        } else if (delta < -0.040f) {
            // Напряжение упало на >40мВ за 30с → зарядник отключён
            _chargeConfirm = max((int8_t)-2, (int8_t)(_chargeConfirm - 1));
            if (_chargeConfirm <= -1) _charging = false;
        }
        // иначе (±30..40мВ): стабильно — сохраняем предыдущее состояние
    }

    // Детектируем момент подключения зарядки (false → true)
    if (_charging && !_prevCharging) {
        _stats.cycles++;
        _stats.sessionSec = 0;   // сбрасываем счётчик сессии
        batteryStatsSave();
    }
    _prevCharging = _charging;
    _batV   = newV;
    _batPct = _voltToPct(_batV);
#endif
}

int   batteryPercent()  { return _batPct; }
float batteryVoltage()  { return _batV; }
bool  batteryCharging() { return _charging; }

// ── Публичное API: статистика ────────────────────────────────────────────────

void batteryStatsLoad() {
    File f = SD.open(BAT_STATS_FILE, FILE_READ);
    if (!f) return;
    StaticJsonDocument<128> doc;
    if (!deserializeJson(doc, f)) {
        _stats.sessionSec = doc["session"] | (uint32_t)0;
        _stats.totalSec   = doc["total"]   | (uint32_t)0;
        _stats.cycles     = doc["cycles"]  | (uint32_t)0;
    }
    f.close();
    _lastSaveMs = millis();
}

void batteryStatsSave() {
    File f = SD.open(BAT_STATS_FILE, FILE_WRITE);
    if (!f) return;
    StaticJsonDocument<128> doc;
    doc["session"] = _stats.sessionSec;
    doc["total"]   = _stats.totalSec;
    doc["cycles"]  = _stats.cycles;
    serializeJson(doc, f);
    f.close();
    _lastSaveMs = millis();
}

// Добавить секунды к счётчикам.  Вызывать из menuTimeTick каждые 5 с.
void batteryStatsAddTime(uint32_t sec) {
    if (_charging) return;   // во время зарядки время работы не считаем
    _stats.sessionSec += sec;
    _stats.totalSec   += sec;

    // Авто-сохранение каждые BAT_SAVE_EVERY секунд
    if (millis() - _lastSaveMs >= BAT_SAVE_EVERY * 1000u)
        batteryStatsSave();
}

BattStats batteryStatsGet() { return _stats; }
