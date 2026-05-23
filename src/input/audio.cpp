// audio.cpp — RetroESP sound system v11.0
//
// ГЛАВНОЕ ИЗМЕНЕНИЕ vs v10:
//   UI звуки: LEDC меандр → I2S DAC синус-волна
//     ─ Меандр (LEDC) имеет гармоники 3f, 5f, 7f... → «хрипящий» тон
//     ─ Синус (I2S DAC) — только основная частота → чистый, мягкий звук
//     ─ Тот же I2S порт что и WAV плеер, не пересекаются
//     ─ LEDC полностью освобождён (используется только для подсветки)
//
// NES эмулятор использует I2S в osd.cpp — с UI не пересекается.
//
// GPIO26 → SC8002B Class D → динамик 8 Ом

#include "input/audio.h"
#include "config.h"
#include "settings.h"
#include <SD.h>
#include <Arduino.h>
#include "driver/i2s.h"
#include "driver/rtc_io.h"
#include <math.h>

static inline void restoreTouch() { rtc_gpio_deinit(GPIO_NUM_25); }

#define AUDIO_SR      22050      // sample rate для тонов
#define WAV_I2S       I2S_NUM_0
#define WAV_DMA_BUFS  4
#define WAV_DMA_LEN   256

// ── Синус-таблица (256 точек, unsigned 8-bit) ──────────────────
// Вычисляется один раз при старте. sinf дает чистую синусоиду без гармоник.
static uint8_t _sineTable[256];
static bool    _sineReady = false;

static void buildSineTable() {
    if (_sineReady) return;
    for (int i = 0; i < 256; i++)
        _sineTable[i] = (uint8_t)(128 + 127.5f * sinf(2.0f * (float)M_PI * i / 256.0f));
    _sineReady = true;
}

// ── Структуры нот и запросов ───────────────────────────────────
struct Note { uint16_t freq; uint16_t ms; };

enum AudioReqType { AUDIO_NONE, AUDIO_WAV, AUDIO_TONE };

struct AudioReq {
    AudioReqType type;
    char  path[52];     // для AUDIO_WAV
    Note  notes[8];     // для AUDIO_TONE
    int   noteCount;
};

static AudioReq      _req;
static volatile bool _busy  = false;
static TaskHandle_t  _task  = nullptr;
static bool          _sdSounds = false;

// ── TPDF дизеринг для WAV ─────────────────────────────────────
static uint32_t _wav_lfsr = 0xCAFEBABEu;
static inline int32_t wav_tpdf() {
    _wav_lfsr ^= _wav_lfsr << 13;
    _wav_lfsr ^= _wav_lfsr >> 17;
    _wav_lfsr ^= _wav_lfsr << 5;
    int32_t r1 = (int32_t)(_wav_lfsr & 0xFF);
    _wav_lfsr ^= _wav_lfsr << 13;
    _wav_lfsr ^= _wav_lfsr >> 17;
    _wav_lfsr ^= _wav_lfsr << 5;
    int32_t r2 = (int32_t)(_wav_lfsr & 0xFF);
    return r1 - r2;
}

// ── DMA буфер (статический — не занимает стек задачи) ─────────
static uint16_t _i2sBuf[WAV_DMA_LEN * 2];

// ── I2S старт/стоп (общий для WAV и тонов) ────────────────────
static bool i2sStart(uint32_t sr) {
    ledcDetachPin(AUDIO_PIN);
    pinMode(AUDIO_PIN, INPUT);
    i2s_driver_uninstall(WAV_I2S);
    restoreTouch();

    i2s_config_t cfg = {};
    cfg.mode             = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate      = sr;
    cfg.bits_per_sample  = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format   = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    cfg.dma_buf_count    = WAV_DMA_BUFS;
    cfg.dma_buf_len      = WAV_DMA_LEN;
    cfg.use_apll         = false;  // UI звуки — WiFi может быть активен
    cfg.intr_alloc_flags = 0;

    if (i2s_driver_install(WAV_I2S, &cfg, 0, nullptr) != ESP_OK) return false;
    i2s_set_pin(WAV_I2S, nullptr);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
    i2s_zero_dma_buffer(WAV_I2S);
    return true;
}

static void i2sStop() {
    // Заполняем буферы тишиной (центр DAC = 0x8000) и ждём дренажа DMA
    for (int i = 0; i < WAV_DMA_LEN * 2; i++) _i2sBuf[i] = 0x8000;
    size_t w;
    i2s_write(WAV_I2S, _i2sBuf, WAV_DMA_LEN * 4, &w, pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(80));
    i2s_driver_uninstall(WAV_I2S);
    restoreTouch();
    pinMode(AUDIO_PIN, INPUT_PULLDOWN);
}

// ── WAV helper ────────────────────────────────────────────────
static uint32_t rd32(File &f) {
    uint8_t b[4]; f.read(b, 4);
    return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
}
static uint16_t rd16(File &f) {
    uint8_t b[2]; f.read(b, 2);
    return (uint16_t)b[0] | ((uint16_t)b[1]<<8);
}

// ── Воспроизведение WAV файла ─────────────────────────────────
static void playWavFile(const char *path) {
    File f = SD.open(path, FILE_READ);
    if (!f) return;

    // Разбор заголовка WAV
    uint8_t tag[4];
    f.read(tag, 4);
    if (memcmp(tag, "RIFF", 4) != 0) { f.close(); return; }
    rd32(f); f.read(tag, 4); f.read(tag, 4);
    uint32_t fmtSz         = rd32(f);
    uint16_t audioFmt      = rd16(f);
    uint16_t channels      = rd16(f);
    uint32_t sampleRate    = rd32(f);
    rd32(f); rd16(f);
    uint16_t bitsPerSample = rd16(f);
    if (fmtSz > 16) f.seek(f.position() + (fmtSz - 16));

    uint32_t dataSize = 0; bool found = false;
    for (int i = 0; i < 10 && !found; i++) {
        f.read(tag, 4); uint32_t sz = rd32(f);
        if (memcmp(tag, "data", 4) == 0) { dataSize = sz; found = true; }
        else f.seek(f.position() + sz);
    }
    if (!found || audioFmt != 1 || channels < 1 || channels > 2
               || bitsPerSample > 16 || sampleRate == 0) { f.close(); return; }

    if (!i2sStart(sampleRate)) { f.close(); return; }

    const int frameBytes = ((bitsPerSample == 8) ? 1 : 2) * (int)channels;
    uint8_t readBuf[512];
    uint32_t pos = 0;
    uint8_t vol = settings.volume > 100 ? 100 : settings.volume;
    bool ok = true;

    while (ok && pos < dataSize && f.available()) {
        int toRead = (int)min((uint32_t)sizeof(readBuf), dataSize - pos);
        toRead = (toRead / frameBytes) * frameBytes;
        if (toRead <= 0) break;
        int got = f.read(readBuf, toRead);
        if (got <= 0) break;

        int samples = got / frameBytes;
        for (int i = 0; i < samples; i++) {
            // Читаем в 16-битном пространстве — сохраняем точность до самого конца
            int32_t s32;
            if (bitsPerSample == 8) {
                s32 = ((int32_t)readBuf[i * frameBytes] - 128) << 8;
            } else {
                s32 = (int32_t)(int16_t)(readBuf[i * frameBytes] |
                                        (readBuf[i * frameBytes + 1] << 8));
            }
            s32 = (s32 * (int32_t)vol) / 100;
            s32 += wav_tpdf();                          // TPDF дизеринг ДО квантования
            if (s32 >  32767) s32 =  32767;
            if (s32 < -32768) s32 = -32768;
            uint16_t dacWord = (uint16_t)((uint8_t)((s32 + 32768) >> 8)) << 8;
            _i2sBuf[i * 2]     = dacWord;
            _i2sBuf[i * 2 + 1] = dacWord;
        }
        size_t written;
        esp_err_t err = i2s_write(WAV_I2S, _i2sBuf, (size_t)(samples * 4),
                                  &written, pdMS_TO_TICKS(200));
        if (err != ESP_OK) ok = false;
        pos += got;
    }

    f.close();
    i2sStop();
}

// ── Синтез синус-волны (I2S DAC) ──────────────────────────────
// Вместо LEDC меандра: фазовый аккумулятор → синус-таблица → I2S DAC.
// Результат: чистый тон без гармонических искажений.
static void playSineSeq(const Note *notes, int count) {
    if (!i2sStart(AUDIO_SR)) return;

    // Громкость: 1..100% → амплитуда 15..110 из 127 (не полная шкала, запас)
    uint8_t vol = (settings.soundEnabled && settings.volume > 0)
                  ? (uint8_t)map((long)settings.volume, 1L, 100L, 15L, 110L)
                  : 0;

    bool ok = true;
    for (int ni = 0; ni < count && ok; ni++) {
        const Note &note = notes[ni];
        if (note.ms == 0) break;

        uint32_t totalSamples = (uint32_t)AUDIO_SR * note.ms / 1000;
        float phase = 0.0f;
        // Шаг фазы: сколько позиций таблицы (0..255) за один сэмпл
        float step = (note.freq > 0)
                     ? (256.0f * (float)note.freq / (float)AUDIO_SR)
                     : 0.0f;

        for (uint32_t pos = 0; pos < totalSamples && ok; ) {
            uint32_t chunk = min((uint32_t)128, totalSamples - pos);
            for (uint32_t i = 0; i < chunk; i++) {
                uint8_t dacVal;
                if (note.freq == 0 || vol == 0) {
                    dacVal = 128;  // тишина (центр DAC)
                } else {
                    int32_t s = (int32_t)_sineTable[(int)phase & 0xFF] - 128;
                    s = (s * vol) / 127;
                    // clamp
                    if (s >  127) s =  127;
                    if (s < -128) s = -128;
                    dacVal = (uint8_t)(s + 128);
                }
                uint16_t dacWord = (uint16_t)dacVal << 8;
                _i2sBuf[i * 2]     = dacWord;
                _i2sBuf[i * 2 + 1] = dacWord;
                phase += step;
                if (phase >= 256.0f) phase -= 256.0f;
            }
            size_t written;
            esp_err_t err = i2s_write(WAV_I2S, _i2sBuf, chunk * 4,
                                      &written, pdMS_TO_TICKS(100));
            if (err != ESP_OK) ok = false;
            pos += chunk;
        }
    }

    i2sStop();
}

// ── Единый аудио-таск (Core 1, priority 1) ────────────────────
static void audioTask(void*) {
    buildSineTable();
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!settings.soundEnabled) { _busy = false; continue; }

        if      (_req.type == AUDIO_WAV)  playWavFile(_req.path);
        else if (_req.type == AUDIO_TONE) playSineSeq(_req.notes, _req.noteCount);

        _busy = false;
    }
}

// ── Вспомогательные функции отправки запросов ─────────────────
static void triggerWav(const char *name) {
    if (!_sdSounds || !_task || _busy) return;
    snprintf(_req.path, sizeof(_req.path), "/sounds/%s.wav", name);
    _req.type = AUDIO_WAV;
    _busy = true;
    xTaskNotifyGive(_task);
}

static void triggerTone(const Note *notes, int len) {
    if (!_task || _busy || !settings.soundEnabled) return;
    _req.type = AUDIO_TONE;
    int c = min(len, 8);
    memcpy(_req.notes, notes, c * sizeof(Note));
    _req.noteCount = c;
    _busy = true;
    xTaskNotifyGive(_task);
}

// ══════════════════════════════════════════════════════════════
// PUBLIC API (неизменный интерфейс — main.cpp не трогать)
// ══════════════════════════════════════════════════════════════

void audioInit() {
    pinMode(AUDIO_PIN, INPUT_PULLDOWN);
    buildSineTable();

    if (SD.exists("/sounds")) {
        _sdSounds = true;
        Serial.println("[AUD] SD sounds enabled — WAV + I2S sine fallback");
    } else {
        Serial.println("[AUD] No /sounds — I2S sine tones (clean sine, no LEDC buzz)");
    }

    xTaskCreatePinnedToCore(audioTask, "aud", 4096, nullptr, 1, &_task, 1);
}

void audioUpdate() {
    // Задача управляет всем самостоятельно — здесь ничего не нужно
}

void soundClick() {
    if (_sdSounds) { triggerWav("click"); return; }
    static const Note n[] = {{1760,12},{0,8},{1760,10},{0,0}};
    triggerTone(n, 4);
}

void soundSelect() {
    if (_sdSounds) { triggerWav("select"); return; }
    static const Note n[] = {{523,55},{659,55},{784,55},{1047,90},{0,0}};
    triggerTone(n, 5);
}

void soundBack() {
    if (_sdSounds) { triggerWav("back"); return; }
    static const Note n[] = {{1047,55},{784,75},{0,0}};
    triggerTone(n, 3);
}

void soundError() {
    if (_sdSounds) { triggerWav("error"); return; }
    static const Note n[] = {{196,90},{0,45},{196,120},{0,0}};
    triggerTone(n, 4);
}

void soundOK() {
    if (_sdSounds) { triggerWav("ok"); return; }
    static const Note n[] = {{523,55},{659,55},{784,55},{1047,55},{2093,110},{0,0}};
    triggerTone(n, 6);
}
