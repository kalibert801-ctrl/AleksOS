// audio.cpp — RetroESP sound system v9.1
//
// УПРАВЛЕНИЕ ПИНОМ:
//   Когда ничего не играет — DAC/LEDC отключается.
//   GPIO26 переводится в INPUT_PULLDOWN → ноль на выходе → тишина.
//   При воспроизведении пин снова подключается.
//
// КАЧЕСТВО ЗВУКА:
//   SD WAV: реальные полифонические звуки через ЦАП (GPIO26).
//           Формат: 8-bit unsigned PCM, mono, до 44100 Гц.
//   LEDC fallback: квадратные волны (пищалки) если SD не найдена.
//
// SD ЗВУКИ: положить файлы в /sounds/ на SD карте:
//   click.wav, select.wav, back.wav, ok.wav, error.wav

#include "input/audio.h"
#include "config.h"
#include <SD.h>

#define AUDIO_CH    1      // LEDC канал (0 занят подсветкой)
#define AUDIO_BITS  8

// ── Состояние пина ────────────────────────────────────────────
static bool _pinActive = false;   // true = пин подключён к LEDC/DAC

static void pinEnable() {
    if (_pinActive) return;
    ledcSetup(AUDIO_CH, 2000, AUDIO_BITS);
    ledcAttachPin(AUDIO_PIN, AUDIO_CH);
    ledcWrite(AUDIO_CH, 0);
    _pinActive = true;
}

static void pinDisable() {
    if (!_pinActive) return;
    ledcWrite(AUDIO_CH, 0);
    ledcDetachPin(AUDIO_PIN);
    pinMode(AUDIO_PIN, INPUT_PULLDOWN);  // прижимаем к GND → нет шума
    _pinActive = false;
}

// ── LEDC notes ───────────────────────────────────────────────
struct Note { uint16_t freq; uint16_t ms; };
static Note     _queue[8];
static int      _qLen = 0, _qIdx = 0;
static uint32_t _noteEnd = 0;
static bool     _notePlay = false;

static uint8_t noteVolume() {
    if (!settings.soundEnabled || settings.volume == 0) return 0;
    return (uint8_t)map(settings.volume, 1, 100, 3, 90);
}

static void startNote(const Note &n) {
    if (n.freq == 0) {
        ledcWrite(AUDIO_CH, 0);
    } else {
        ledcWriteTone(AUDIO_CH, n.freq);
        ledcWrite(AUDIO_CH, noteVolume());
    }
    _noteEnd = millis() + n.ms;
}

static void enqueue(const Note *notes, int len) {
    if (!settings.soundEnabled) return;
    int c = min(len, 8);
    memcpy(_queue, notes, c * sizeof(Note));
    _qLen = c; _qIdx = 0; _notePlay = true;
    pinEnable();
    startNote(_queue[0]);
}

// ── WAV player ────────────────────────────────────────────────
static bool         _sdSounds = false;
static TaskHandle_t _wavTask  = nullptr;
static volatile bool _wavBusy = false;
static char          _wavReq[52];

static uint32_t rd32(File &f) {
    uint8_t b[4]; f.read(b, 4);
    return (uint32_t)b[0] | (b[1]<<8) | (b[2]<<16) | (b[3]<<24);
}
static uint16_t rd16(File &f) {
    uint8_t b[2]; f.read(b, 2);
    return (uint16_t)b[0] | (b[1]<<8);
}

// Маленький буфер для батчевого чтения с SD (снижает задержку ЦАП)
#define WAV_BUF_SZ 128
static uint8_t _wavBuf[WAV_BUF_SZ];

static void wavPlayerTask(void*) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!settings.soundEnabled) { _wavBusy = false; vTaskDelay(1); continue; }

        File f = SD.open(_wavReq, FILE_READ);
        if (!f) { _wavBusy = false; vTaskDelay(1); continue; }

        // ── Разбор WAV-заголовка ──────────────────────────
        uint8_t tag[4];
        f.read(tag, 4);                       // "RIFF"
        if (memcmp(tag, "RIFF", 4) != 0) { f.close(); _wavBusy=false; vTaskDelay(1); continue; }
        rd32(f);                              // file size
        f.read(tag, 4);                       // "WAVE"
        f.read(tag, 4);                       // "fmt "
        uint32_t fmtSz       = rd32(f);
        uint16_t audioFmt    = rd16(f);       // 1=PCM
        uint16_t channels    = rd16(f);
        uint32_t sampleRate  = rd32(f);
        rd32(f);                              // byte rate
        rd16(f);                              // block align
        uint16_t bitsPerSample = rd16(f);
        if (fmtSz > 16) f.seek(f.position() + (fmtSz - 16));

        // Ищем "data" chunk
        uint32_t dataSize = 0; bool found = false;
        for (int i = 0; i < 10 && !found; i++) {
            f.read(tag, 4);
            uint32_t sz = rd32(f);
            if (memcmp(tag, "data", 4) == 0) { dataSize = sz; found = true; }
            else f.seek(f.position() + sz);
        }
        if (!found || audioFmt != 1 || channels > 2 || bitsPerSample > 16 || sampleRate == 0) {
            f.close(); _wavBusy=false; vTaskDelay(1); continue;
        }

        // ── Воспроизведение через ЦАП ───────────────────
        // Отключаем LEDC — DAC и LEDC не могут работать на одном пине
        ledcDetachPin(AUDIO_PIN);
        dacWrite(AUDIO_PIN, 128);  // DC bias = тишина

        uint8_t vol = (uint8_t)map(settings.volume, 0, 100, 0, 255);
        uint32_t periodUs = 1000000UL / sampleRate;
        // Компенсация задержки на чтение SD (батч)
        int bytesPerSample = (bitsPerSample == 8) ? 1 : 2;
        int frameBytes     = bytesPerSample * channels;
        uint32_t pos = 0;

        while (pos < dataSize && f.available()) {
            // Читаем блок заранее
            int toRead = min((uint32_t)WAV_BUF_SZ, dataSize - pos);
            toRead = (toRead / frameBytes) * frameBytes;  // выравниваем по кадру
            if (toRead == 0) break;
            int got = f.read(_wavBuf, toRead);
            if (got <= 0) break;

            for (int i = 0; i < got; i += frameBytes) {
                uint8_t sample;
                if (bitsPerSample == 8) {
                    sample = _wavBuf[i];                    // 8-bit unsigned
                } else {
                    int16_t s16 = (int16_t)(_wavBuf[i] | (_wavBuf[i+1] << 8));
                    sample = (uint8_t)((s16 >> 8) + 128);  // 16-bit → unsigned 8
                }
                // Если стерео — берём только левый канал
                // (второй канал уже пропущен через frameBytes)

                // Применяем громкость через таблицу умножения
                sample = (uint8_t)((sample * vol) >> 8);
                dacWrite(AUDIO_PIN, sample);
                delayMicroseconds(periodUs);
            }
            pos += got;
        }

        // ── Завершение: ЦАП → тишина → пин отключить ───
        dacWrite(AUDIO_PIN, 128);
        delay(2);
        dacWrite(AUDIO_PIN, 0);
        // Возвращаем пин в безопасное состояние
        pinMode(AUDIO_PIN, INPUT_PULLDOWN);
        _pinActive = false;  // пин свободен — можно снова включить LEDC если нужно

        f.close();
        _wavBusy = false;
        vTaskDelay(1);
    }
}

static void playWav(const char *name) {
    if (!_sdSounds || !_wavTask) return;
    if (_wavBusy) return;
    snprintf(_wavReq, sizeof(_wavReq), "/sounds/%s.wav", name);
    _wavBusy = true;
    xTaskNotifyGive(_wavTask);
}

// ══════════════════════════════════════════════════════════════
// PUBLIC API
// ══════════════════════════════════════════════════════════════

void audioInit() {
    // Начинаем с отключённым пином (нет шума при старте)
    pinMode(AUDIO_PIN, INPUT_PULLDOWN);
    _pinActive = false;

    if (SD.exists("/sounds")) {
        _sdSounds = true;
        xTaskCreatePinnedToCore(wavPlayerTask, "wav", 4096,
                                nullptr, 1, &_wavTask, 1);
        Serial.println("[AUD] SD sounds enabled (/sounds/*.wav)");
    } else {
        Serial.println("[AUD] No /sounds — LEDC beeps");
    }
}

void audioUpdate() {
    if (!_notePlay) return;
    if (millis() < _noteEnd) return;
    ledcWrite(AUDIO_CH, 0);
    _qIdx++;
    if (_qIdx >= _qLen) {
        _notePlay = false;
        pinDisable();   // ← отключаем пин сразу после последней ноты
        return;
    }
    startNote(_queue[_qIdx]);
}

// ── Звуки ─────────────────────────────────────────────────────

void soundClick() {
    if (_sdSounds) { playWav("click"); return; }
    // Чистый короткий тик — нет щелчка при включении/выключении
    static const Note n[] = {{1760,12},{0,8},{1760,10},{0,0}};
    enqueue(n, 4);
}

void soundSelect() {
    if (_sdSounds) { playWav("select"); return; }
    // До-ми-соль — мажорное трезвучие
    static const Note n[] = {{523,55},{659,55},{784,55},{1047,90},{0,0}};
    enqueue(n, 5);
}

void soundBack() {
    if (_sdSounds) { playWav("back"); return; }
    // Нисходящая терция
    static const Note n[] = {{1047,55},{784,75},{0,0}};
    enqueue(n, 3);
}

void soundError() {
    if (_sdSounds) { playWav("error"); return; }
    // Два коротких низких бипа
    static const Note n[] = {{196,90},{0,45},{196,120},{0,0}};
    enqueue(n, 4);
}

void soundOK() {
    if (_sdSounds) { playWav("ok"); return; }
    // До-ми-соль-до (октава выше)
    static const Note n[] = {{523,55},{659,55},{784,55},{1047,55},{2093,110},{0,0}};
    enqueue(n, 6);
}
