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

// Коррекция процента во время зарядки.
// Проблема: TP4056 держит клеммное напряжение ~4.2V — АЦП читает 100%, хотя реальный заряд ниже.
// Решение: запоминаем % ДО начала зарядки и медленно увеличиваем во время неё.
static int      _batPctBeforeCharge = -1;  // % в момент начала зарядки
static uint32_t _chargeStartMs      =  0;  // millis() когда началась зарядка

// Детектирование скачка/провала напряжения при подключении/отключении зарядника.
static float    _prevV       = 0.0f;
// Последний известный % без зарядки (для старта с уже подключённым зарядником).
static int      _lastKnownPct = -1;
// Защита от сохранения цикла ДО загрузки статистики с SD.
static bool     _statsLoaded = false;

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
    delay(5);   // пре-settle: даём конденсатору зарядиться перед первым замером
    int32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += analogRead(BAT_ADC_PIN);
        delay(1);
    }
    float newV = _rawToVbat(sum >> 4);

    // ── Детектирование зарядки ────────────────────────────────────────────────
    // Два критерия: порог напряжения (CV-режим TP4056 при высоком заряде)
    // ИЛИ резкий скачок напряжения (CC-режим при низком заряде — зарядник только подключили).
    // Резкий провал = зарядник отключён (клеммное напряжение упало до ОХХ батареи).
    float vDelta = (_prevV > 0.5f) ? (newV - _prevV) : 0.0f;
    _prevV = newV;

    bool suddenJump = (vDelta >=  0.15f && newV >= 3.90f);  // подключение зарядника
    bool suddenDrop = (vDelta <= -0.12f);                    // отключение зарядника

    if (newV >= 4.18f || suddenJump)
        _charging = true;
    else if (newV < 4.05f || suddenDrop)
        _charging = false;

    // Детектируем момент подключения зарядки (false → true)
    bool justStartedCharging = _charging && !_prevCharging;
    if (justStartedCharging) {
        // Базовый % берём из актуального значения (если уже было замерено),
        // иначе из последнего сохранённого на SD (для старта с подключённым зарядником).
        if (_batPct >= 0)
            _batPctBeforeCharge = _batPct;
        else if (_lastKnownPct >= 0)
            _batPctBeforeCharge = _lastKnownPct;
        else
            _batPctBeforeCharge = 0;
        _chargeStartMs = millis();
        // Счётчик цикла только после загрузки статистики с SD (иначе перезапишем).
        if (_statsLoaded) {
            _stats.cycles++;
            _stats.sessionSec = 0;
            batteryStatsSave();
        }
    }
    _prevCharging = _charging;
    _batV = newV;

    if (!_charging) {
        // Зарядник отключён: реальное напряжение надёжно
        _batPct             = _voltToPct(_batV);
        _batPctBeforeCharge = -1;
        _lastKnownPct       = _batPct;   // обновляем последний известный %
    } else {
        // Зарядник подключён: клеммное напряжение завышено зарядником —
        // не доверяем ему для расчёта %. Берём % до начала зарядки и медленно
        // прибавляем: +1% каждую минуту (≈100 мин на полный заряд).
        int base = (_batPctBeforeCharge >= 0) ? _batPctBeforeCharge : 0;
        int inc  = (int)((millis() - _chargeStartMs) / 60000u);   // +1% / мин
        _batPct  = min(99, base + inc);  // 100% только после отключения и замера
    }
#endif
}

int   batteryPercent()  { return _batPct; }
float batteryVoltage()  { return _batV; }
bool  batteryCharging() { return _charging; }

// ── Публичное API: статистика ────────────────────────────────────────────────

void batteryStatsLoad() {
    File f = SD.open(BAT_STATS_FILE, FILE_READ);
    if (!f) { _statsLoaded = true; return; }   // файла нет — состояние чистое, разрешаем сохранять
    StaticJsonDocument<128> doc;
    if (!deserializeJson(doc, f)) {
        _stats.sessionSec = doc["session"] | (uint32_t)0;
        _stats.totalSec   = doc["total"]   | (uint32_t)0;
        _stats.cycles     = doc["cycles"]  | (uint32_t)0;
        int sp = doc["pct"] | -1;
        if (sp >= 0 && sp <= 100) _lastKnownPct = sp;
    }
    f.close();
    _lastSaveMs   = millis();
    _statsLoaded  = true;
}

void batteryStatsSave() {
    File f = SD.open(BAT_STATS_FILE, FILE_WRITE);
    if (!f) return;
    StaticJsonDocument<128> doc;
    doc["session"] = _stats.sessionSec;
    doc["total"]   = _stats.totalSec;
    doc["cycles"]  = _stats.cycles;
    if (_batPct >= 0) doc["pct"] = _batPct;   // сохраняем последний % для старта с зарядником
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
