#include "display/led.h"
#include "config.h"

// ── После PSRAM-мода ────────────────────────────────────────
// GPIO16 и GPIO17 заняты PSRAM. Остался один LED на LED_PIN (GPIO4).
// Состояния различаются миганием:
//   LED_OFF    → выкл
//   LED_RED    → постоянно горит         (ошибка)
//   LED_GREEN  → 2 быстрых вспышки       (ОК)
//   LED_BLUE   → 3 быстрых вспышки       (загрузка игры)
//   LED_YELLOW → 1 длинная вспышка       (настройки)
//   LED_BOOT   → быстрое мигание × 6     (анимация старта)

static LedState _state    = LED_OFF;
static uint8_t  _step     = 0;
static uint32_t _nextTime = 0;

// Описание паттерна мигания: {on_ms, off_ms, повторов}
struct BlinkPattern { uint16_t on; uint16_t off; uint8_t reps; };
static const BlinkPattern PATTERNS[] = {
    {0,   0,   0},   // LED_OFF
    {0,   0,   0},   // LED_GREEN  — управляется через ledUpdate
    {0,   0,   0},   // LED_RED    — постоянно вкл, не требует update
    {0,   0,   0},   // LED_BLUE   — управляется через ledUpdate
    {0,   0,   0},   // LED_YELLOW — управляется через ledUpdate
    {0,   0,   0},   // LED_BOOT   — управляется через ledUpdate
};

static inline void ledOn()  { digitalWrite(LED_PIN, LOW);  }   // активный LOW
static inline void ledOff() { digitalWrite(LED_PIN, HIGH); }

// Внутренний запуск анимации
static void startAnim() { _step = 0; _nextTime = millis(); }

void ledInit() {
    pinMode(LED_PIN, OUTPUT);
    ledOff();
}

void ledSet(LedState s) {
    _state = s;
    _step  = 0;
    switch (s) {
        case LED_OFF:
            ledOff();
            break;
        case LED_RED:
            ledOn();          // постоянно горит = ошибка
            break;
        case LED_GREEN:
        case LED_BLUE:
        case LED_YELLOW:
        case LED_BOOT:
            ledOff();
            startAnim();
            break;
    }
}

void ledUpdate() {
    if (millis() < _nextTime) return;

    switch (_state) {

        // 2 быстрых вспышки → ОК
        case LED_GREEN: {
            static const uint16_t seq[] = {80, 80, 80, 400}; // вкл,выкл,вкл,пауза
            if (_step < 4) {
                (_step % 2 == 0) ? ledOn() : ledOff();
                _nextTime = millis() + seq[_step];
                _step++;
            } else {
                ledOff();
                _state = LED_OFF;
            }
            break;
        }

        // 3 быстрых вспышки → загрузка игры
        case LED_BLUE: {
            static const uint16_t seq[] = {70, 70, 70, 70, 70, 400};
            if (_step < 6) {
                (_step % 2 == 0) ? ledOn() : ledOff();
                _nextTime = millis() + seq[_step];
                _step++;
            } else {
                ledOff();
                _state = LED_OFF;
            }
            break;
        }

        // 1 длинная вспышка → настройки
        case LED_YELLOW: {
            if (_step == 0) { ledOn();  _nextTime = millis() + 300; _step++; }
            else            { ledOff(); _state = LED_OFF; }
            break;
        }

        // Быстрое мигание × 6 при старте, затем выкл
        case LED_BOOT: {
            if (_step < 12) {
                (_step % 2 == 0) ? ledOn() : ledOff();
                _nextTime = millis() + 120;
                _step++;
            } else {
                ledOff();
                _state = LED_OFF;
            }
            break;
        }

        default: break;
    }
}
