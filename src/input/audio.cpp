// audio.cpp — RetroESP sound system v10.0
//
// ВИПРАВЛЕННЯ vs v9.1:
//   WAV плеєр: dacWrite + delayMicroseconds → I2S DMA
//     ─ Апаратний тайминг замість busy-wait → нема джиттеру → нема хрипу
//     ─ CPU вільний під час відтворення (i2s_write поступається задачам)
//     ─ Якщо емулятор стартує під час WAV → i2s_write повертає помилку,
//       задача коректно завершується без зависання
//   Гучність: виправлено масштабування від центру (128), а не від нуля
//     ─ Стара формула: sample * vol / 256  → перекидала семпли через центр
//     ─ Нова формула: (sample-128)*vol/100+128 → правильна зміна амплітуди
//   LEDC: обмежено максимальний duty cycle 90→55 → менший струм через GPIO
//
// АПАРАТНА ПРИМІТКА:
//   GPIO26 без підсилювача не може нормально живити динамік (8 Ом).
//   При великій гучності струм перевищує 40мА ліміт GPIO → просідає 3.3В →
//   мерехтить дисплей. Програмний ліміт MAX_DAC_VOL зменшує це, але
//   справжнє рішення — додати підсилювач (PAM8403 / LM386 / транзистор + резистор).
//
// SD ЗВУКИ: покласти файли в /sounds/ на SD карті:
//   click.wav, select.wav, back.wav, ok.wav, error.wav
//   Формат: PCM, 8 або 16 біт, mono або stereo, до 44100 Гц

#include "input/audio.h"
#include "config.h"
#include "settings.h"
#include <SD.h>
#include "driver/i2s.h"

#define AUDIO_CH      1        // LEDC канал (0 зайнятий підсвіткою)
#define AUDIO_BITS    8        // LEDC розрядність

// I2S для WAV плеєра (той самий порт що й емулятор, але вони не перетинаються)
#define WAV_I2S       I2S_NUM_0
#define WAV_DMA_BUFS  4
#define WAV_DMA_LEN   256      // семплів на DMA буфер

// Після gain mod (x14.5 → знижений) амплітуду більше не обмежуємо —
// підсилювач вже не кліпує при нормальному рівні DAC.

// ── Стан піна ─────────────────────────────────────────────────
static bool _pinActive = false;

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
    pinMode(AUDIO_PIN, INPUT_PULLDOWN);
    _pinActive = false;
}

// ── LEDC ноти ─────────────────────────────────────────────────
struct Note { uint16_t freq; uint16_t ms; };
static Note     _queue[8];
static int      _qLen = 0, _qIdx = 0;
static uint32_t _noteEnd = 0;
static bool     _notePlay = false;

static uint8_t noteVolume() {
    if (!settings.soundEnabled || settings.volume == 0) return 0;
    // Обмежено 55 (раніше 90) — зменшує струм через GPIO → менше мерехтіння
    return (uint8_t)map(settings.volume, 1, 100, 2, 55);
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

// ── WAV helper ────────────────────────────────────────────────
static uint32_t rd32(File &f) {
    uint8_t b[4]; f.read(b, 4);
    return (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
}
static uint16_t rd16(File &f) {
    uint8_t b[2]; f.read(b, 2);
    return (uint16_t)b[0] | ((uint16_t)b[1]<<8);
}

// ── WAV плеєр (I2S DMA) ───────────────────────────────────────
static bool          _sdSounds = false;
static TaskHandle_t  _wavTask  = nullptr;
static volatile bool _wavBusy  = false;
static char          _wavReq[52];

// DMA буфер — статичний, не займає стек задачі
static uint16_t _i2sBuf[WAV_DMA_LEN * 2];   // стерео пари для I2S DAC

static void wavPlayerTask(void*) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!settings.soundEnabled) { _wavBusy = false; continue; }

        File f = SD.open(_wavReq, FILE_READ);
        if (!f) { _wavBusy = false; continue; }

        // ── Розбір WAV заголовку ──────────────────────────────
        uint8_t tag[4];
        f.read(tag, 4);
        if (memcmp(tag, "RIFF", 4) != 0) { f.close(); _wavBusy = false; continue; }
        rd32(f);                   // розмір файлу
        f.read(tag, 4);            // "WAVE"
        f.read(tag, 4);            // "fmt "
        uint32_t fmtSz         = rd32(f);
        uint16_t audioFmt      = rd16(f);   // 1 = PCM
        uint16_t channels      = rd16(f);
        uint32_t sampleRate    = rd32(f);
        rd32(f); rd16(f);          // byte rate, block align
        uint16_t bitsPerSample = rd16(f);
        if (fmtSz > 16) f.seek(f.position() + (fmtSz - 16));

        uint32_t dataSize = 0;
        bool found = false;
        for (int i = 0; i < 10 && !found; i++) {
            f.read(tag, 4);
            uint32_t sz = rd32(f);
            if (memcmp(tag, "data", 4) == 0) { dataSize = sz; found = true; }
            else f.seek(f.position() + sz);
        }
        if (!found || audioFmt != 1 || channels < 1 || channels > 2
                   || bitsPerSample > 16 || sampleRate == 0) {
            f.close(); _wavBusy = false; continue;
        }

        // ── Ініціалізація I2S ─────────────────────────────────
        // Відключаємо LEDC якщо він тримав пін
        ledcDetachPin(AUDIO_PIN);
        pinMode(AUDIO_PIN, INPUT);
        // Знімаємо драйвер якщо залишився від попереднього сеансу
        i2s_driver_uninstall(WAV_I2S);

        i2s_config_t cfg = {};
        cfg.mode              = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
        cfg.sample_rate       = sampleRate;
        cfg.bits_per_sample   = I2S_BITS_PER_SAMPLE_16BIT;
        cfg.channel_format    = I2S_CHANNEL_FMT_RIGHT_LEFT;
        cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
        cfg.dma_buf_count     = WAV_DMA_BUFS;
        cfg.dma_buf_len       = WAV_DMA_LEN;
        cfg.use_apll          = false;  // APLL несумісний з WiFi на ESP32
        cfg.intr_alloc_flags  = 0;

        if (i2s_driver_install(WAV_I2S, &cfg, 0, nullptr) != ESP_OK) {
            f.close(); _wavBusy = false; continue;
        }
        i2s_set_pin(WAV_I2S, nullptr);
        i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);  // GPIO26 = DAC2 = Left
        i2s_zero_dma_buffer(WAV_I2S);
        _pinActive = false;  // пін тепер під I2S, не LEDC

        // ── Стримінг семплів ──────────────────────────────────
        const int frameBytes = ((bitsPerSample == 8) ? 1 : 2) * (int)channels;
        uint8_t readBuf[512];
        uint32_t pos = 0;
        // Гучність 0..100 → ±амплітуда з урахуванням MAX_AMP
        uint8_t vol = settings.volume > 100 ? 100 : settings.volume;
        bool    ok  = true;

        while (ok && pos < dataSize && f.available()) {
            int toRead = (int)min((uint32_t)sizeof(readBuf), dataSize - pos);
            toRead = (toRead / frameBytes) * frameBytes;
            if (toRead <= 0) break;

            int got = f.read(readBuf, toRead);
            if (got <= 0) break;

            int samples = got / frameBytes;
            for (int i = 0; i < samples; i++) {
                // Витягуємо семпл як знакове ціле в діапазоні −128..+127
                int32_t s;
                if (bitsPerSample == 8) {
                    s = (int32_t)readBuf[i * frameBytes] - 128;   // unsigned8 → signed
                } else {
                    int16_t s16 = (int16_t)(readBuf[i * frameBytes] | (readBuf[i * frameBytes + 1] << 8));
                    s = (int32_t)s16 >> 8;  // 16-bit → −128..+127
                }
                // (Для стерео беремо тільки лівий канал; правий пропускаємо через frameBytes)

                // Масштабуємо амплітуду по гучності (0..100%)
                s = (s * (int32_t)vol) / 100;

                // ESP32 I2S DAC: старші 8 біт 16-бітного слова → DAC (0=мін, 255=макс)
                // Центр = 128 → DAC word = 0x8000
                uint16_t dacWord = (uint16_t)((uint8_t)(s + 128)) << 8;

                // Пишемо в обидва канали (DAC читає Left = GPIO26)
                _i2sBuf[i * 2]     = dacWord;  // LEFT  → DAC2 → GPIO26
                _i2sBuf[i * 2 + 1] = dacWord;  // RIGHT → не використовується
            }

            size_t written;
            // Якщо емулятор запустився і зняв I2S драйвер — вийдемо коректно
            esp_err_t err = i2s_write(WAV_I2S, _i2sBuf, (size_t)(samples * 4),
                                      &written, pdMS_TO_TICKS(200));
            if (err != ESP_OK) { ok = false; break; }
            pos += got;
        }

        // ── Завершення: тиша → дренуємо DMA → знімаємо драйвер ──
        if (ok) {
            // Заповнюємо буфер центральним значенням (тиша для DAC)
            for (int i = 0; i < WAV_DMA_LEN * 2; i++) _i2sBuf[i] = 0x8000;
            size_t written;
            i2s_write(WAV_I2S, _i2sBuf, sizeof(_i2sBuf), &written, pdMS_TO_TICKS(100));
            vTaskDelay(pdMS_TO_TICKS(80));  // даємо DMA буферам вичерпатись
        }

        i2s_driver_uninstall(WAV_I2S);
        pinMode(AUDIO_PIN, INPUT_PULLDOWN);

        f.close();
        _wavBusy = false;
    }
}

static void playWav(const char *name) {
    if (!_sdSounds || !_wavTask || _wavBusy) return;
    snprintf(_wavReq, sizeof(_wavReq), "/sounds/%s.wav", name);
    _wavBusy = true;
    xTaskNotifyGive(_wavTask);
}

// ══════════════════════════════════════════════════════════════
// PUBLIC API
// ══════════════════════════════════════════════════════════════

void audioInit() {
    pinMode(AUDIO_PIN, INPUT_PULLDOWN);
    _pinActive = false;

    if (SD.exists("/sounds")) {
        _sdSounds = true;
        xTaskCreatePinnedToCore(wavPlayerTask, "wav", 4096,
                                nullptr, 1, &_wavTask, 1);
        Serial.println("[AUD] SD sounds enabled — I2S DMA mode (/sounds/*.wav)");
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
        pinDisable();
        return;
    }
    startNote(_queue[_qIdx]);
}

// ── Звуки ─────────────────────────────────────────────────────

void soundClick() {
    if (_sdSounds) { playWav("click"); return; }
    static const Note n[] = {{1760,12},{0,8},{1760,10},{0,0}};
    enqueue(n, 4);
}

void soundSelect() {
    if (_sdSounds) { playWav("select"); return; }
    static const Note n[] = {{523,55},{659,55},{784,55},{1047,90},{0,0}};
    enqueue(n, 5);
}

void soundBack() {
    if (_sdSounds) { playWav("back"); return; }
    static const Note n[] = {{1047,55},{784,75},{0,0}};
    enqueue(n, 3);
}

void soundError() {
    if (_sdSounds) { playWav("error"); return; }
    static const Note n[] = {{196,90},{0,45},{196,120},{0,0}};
    enqueue(n, 4);
}

void soundOK() {
    if (_sdSounds) { playWav("ok"); return; }
    static const Note n[] = {{523,55},{659,55},{784,55},{1047,55},{2093,110},{0,0}};
    enqueue(n, 6);
}
