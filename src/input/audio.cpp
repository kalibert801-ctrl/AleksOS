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

// minimp3 — single-header MP3 decoder (https://github.com/lieff/minimp3)
// Добавить в platformio.ini: https://github.com/lieff/minimp3.git
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include <minimp3.h>

static inline void restoreTouch() { rtc_gpio_deinit(GPIO_NUM_25); }

#define AUDIO_SR      22050      // sample rate для тонов
#define WAV_I2S       I2S_NUM_0
#define WAV_DMA_BUFS  8
#define WAV_DMA_LEN   512

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
    char  path[128];    // для AUDIO_WAV (до 120 символов пути)
    Note  notes[8];     // для AUDIO_TONE
    int   noteCount;
};

static AudioReq      _req;
static volatile bool _busy    = false;
static volatile bool _stopReq = false;
static TaskHandle_t  _task    = nullptr;
static bool          _sdSounds = false;


// ── DMA буфер (моно, статический — не занимает стек задачи) ───
static uint16_t _i2sBuf[WAV_DMA_LEN];
static bool     _i2sInstalled = false;  // отслеживаем состояние драйвера

// ── MP3 статические буферы (в DRAM, не на стеке задачи) ───────
// mp3dec_t ≈ 4 KB; inBuf 4 KB; pcmBuf 4.5 KB — итого ~12.5 KB DRAM
static mp3dec_t  _mp3dec;
static uint8_t   _mp3In[4096];
static mp3d_sample_t _mp3Pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];  // 2304 × int16

// ── I2S старт/стоп (общий для WAV и тонов) ────────────────────
static bool i2sStart(uint32_t sr) {
    ledcDetachPin(AUDIO_PIN);
    pinMode(AUDIO_PIN, INPUT);
    if (_i2sInstalled) { i2s_driver_uninstall(WAV_I2S); _i2sInstalled = false; }
    restoreTouch();

    i2s_config_t cfg = {};
    cfg.mode             = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate      = sr;
    cfg.bits_per_sample  = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format   = I2S_CHANNEL_FMT_ONLY_LEFT;   // GPIO26 = DAC2 = LEFT
    cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    cfg.dma_buf_count    = WAV_DMA_BUFS;
    cfg.dma_buf_len      = WAV_DMA_LEN;
    cfg.use_apll         = true;   // точный clock для аудио (APLL)
    cfg.intr_alloc_flags = 0;

    if (i2s_driver_install(WAV_I2S, &cfg, 0, nullptr) != ESP_OK) return false;
    _i2sInstalled = true;
    i2s_set_pin(WAV_I2S, nullptr);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
    i2s_zero_dma_buffer(WAV_I2S);
    return true;
}

static void i2sStop() {
    if (!_i2sInstalled) return;
    // Заполняем буферы тишиной (центр DAC = 0x8000) и ждём дренажа DMA
    for (int i = 0; i < WAV_DMA_LEN; i++) _i2sBuf[i] = 0x8000;
    size_t w;
    i2s_write(WAV_I2S, _i2sBuf, WAV_DMA_LEN * 2, &w, pdMS_TO_TICKS(100));
    vTaskDelay(pdMS_TO_TICKS(80));
    i2s_driver_uninstall(WAV_I2S);
    _i2sInstalled = false;
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

    if (!i2sStart(sampleRate * 10 / 17)) { f.close(); return; }

    const int frameBytes = ((bitsPerSample == 8) ? 1 : 2) * (int)channels;
    uint8_t readBuf[512];
    uint32_t pos = 0;
    bool ok = true;
    const int vol = settings.volume > 100 ? 100 : (int)settings.volume;

    while (ok && pos < dataSize && f.available() && !_stopReq) {
        int toRead = (int)min((uint32_t)sizeof(readBuf), dataSize - pos);
        toRead = (toRead / frameBytes) * frameBytes;
        if (toRead <= 0) break;
        int got = f.read(readBuf, toRead);
        if (got <= 0) break;

        int samples = got / frameBytes;
        if (samples > WAV_DMA_LEN) samples = WAV_DMA_LEN;
        for (int i = 0; i < samples; i++) {
            uint16_t dacWord;
            if (bitsPerSample == 8) {
                dacWord = (uint16_t)readBuf[i * frameBytes] << 8;
            } else {
                int16_t s = (int16_t)(readBuf[i * frameBytes] |
                                     (readBuf[i * frameBytes + 1] << 8));
                dacWord = (uint16_t)((uint8_t)((s + 32768) >> 8)) << 8;
            }
            if (vol < 100) {
                int centered = (int)(dacWord >> 8) - 128;
                dacWord = (uint16_t)(uint8_t)(centered * vol / 100 + 128) << 8;
            }
            _i2sBuf[i] = dacWord;
        }
        size_t written;
        esp_err_t err = i2s_write(WAV_I2S, _i2sBuf, (size_t)(samples * 2),
                                  &written, pdMS_TO_TICKS(200));
        if (err != ESP_OK) ok = false;
        pos += got;
    }

    f.close();
    i2sStop();
}

// ── Воспроизведение MP3 файла (minimp3) ──────────────────────────────────────
// Декодирует фрейм за фреймом (1152 сэмпла/фрейм), стерео → моно для DAC.
static void playMp3File(const char *path) {
    File f = SD.open(path, FILE_READ);
    if (!f) return;

    mp3dec_init(&_mp3dec);

    int inLen = 0;
    const int vol = settings.volume > 100 ? 100 : (int)settings.volume;
    uint32_t curSr = 0;
    bool i2sOk = false;

    while (!_stopReq) {
        // Дозаполняем входной буфер из SD
        if (inLen < (int)sizeof(_mp3In) && f.available()) {
            int got = f.read(_mp3In + inLen, sizeof(_mp3In) - inLen);
            if (got > 0) inLen += got;
        }
        if (inLen == 0) break;

        mp3dec_frame_info_t fi;
        int samples = mp3dec_decode_frame(&_mp3dec, _mp3In, inLen, _mp3Pcm, &fi);

        // Сдвигаем буфер на обработанный фрейм
        if (fi.frame_bytes > 0) {
            if (fi.frame_bytes < inLen)
                memmove(_mp3In, _mp3In + fi.frame_bytes, inLen - fi.frame_bytes);
            inLen -= fi.frame_bytes;
        } else if (samples == 0) {
            // Недостаточно данных и файл закончился — выходим
            if (!f.available()) break;
            continue;
        }
        if (samples <= 0) continue;

        // Перезапускаем I2S если sample rate изменился (редко, но бывает)
        if (!i2sOk || fi.hz != curSr) {
            if (i2sOk) i2sStop();
            if (!i2sStart(fi.hz * 10 / 17)) break;
            i2sOk = true; curSr = fi.hz;
        }

        // Количество фреймов (mono = samples, stereo = samples/2)
        int frames = (fi.channels > 1) ? samples / 2 : samples;

        // Пишем в I2S чанками WAV_DMA_LEN
        int out = 0;
        while (out < frames && !_stopReq) {
            int chunk = min(frames - out, (int)WAV_DMA_LEN);
            for (int i = 0; i < chunk; i++) {
                int16_t s;
                if (fi.channels > 1) {
                    s = (int16_t)(((int32_t)_mp3Pcm[(out + i) * 2] +
                                   (int32_t)_mp3Pcm[(out + i) * 2 + 1]) / 2);
                } else {
                    s = _mp3Pcm[out + i];
                }
                uint16_t dacWord = (uint16_t)((uint8_t)((s + 32768) >> 8)) << 8;
                if (vol < 100) {
                    int centered = (int)(dacWord >> 8) - 128;
                    dacWord = (uint16_t)(uint8_t)(centered * vol / 100 + 128) << 8;
                }
                _i2sBuf[i] = dacWord;
            }
            size_t w;
            i2s_write(WAV_I2S, _i2sBuf, (size_t)(chunk * 2), &w, pdMS_TO_TICKS(200));
            out += chunk;
        }
    }

    f.close();
    if (i2sOk) i2sStop();
}

// ── Синтез синус-волны (I2S DAC) ──────────────────────────────
// Вместо LEDC меандра: фазовый аккумулятор → синус-таблица → I2S DAC.
// Результат: чистый тон без гармонических искажений.
static void playSineSeq(const Note *notes, int count) {
    if (!i2sStart(AUDIO_SR)) return;

    bool ok = true;
    const int vol = settings.volume > 100 ? 100 : (int)settings.volume;
    for (int ni = 0; ni < count && ok && !_stopReq; ni++) {
        const Note &note = notes[ni];
        if (note.ms == 0) break;

        uint32_t totalSamples = (uint32_t)AUDIO_SR * note.ms / 1000;
        float phase = 0.0f;
        float step = (note.freq > 0)
                     ? (256.0f * (float)note.freq / (float)AUDIO_SR)
                     : 0.0f;

        for (uint32_t pos = 0; pos < totalSamples && ok; ) {
            uint32_t chunk = min((uint32_t)128, totalSamples - pos);
            for (uint32_t i = 0; i < chunk; i++) {
                uint8_t dacVal = (note.freq == 0)
                    ? 128
                    : _sineTable[(int)phase & 0xFF];
                int scaled = (vol < 100) ? ((int)dacVal - 128) * vol / 100 + 128 : (int)dacVal;
                _i2sBuf[i] = (uint16_t)(uint8_t)scaled << 8;
                phase += step;
                if (phase >= 256.0f) phase -= 256.0f;
            }
            size_t written;
            esp_err_t err = i2s_write(WAV_I2S, _i2sBuf, chunk * 2,
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

        if (_req.type == AUDIO_WAV) {
            const char *dot = strrchr(_req.path, '.');
            bool isMp3 = dot && (dot[1]=='m'||dot[1]=='M') && (dot[2]=='p'||dot[2]=='P') && dot[3]=='3' && dot[4]=='\0';
            if (isMp3) playMp3File(_req.path);
            else                                       playWavFile(_req.path);
        } else if (_req.type == AUDIO_TONE) {
            playSineSeq(_req.notes, _req.noteCount);
        }

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

    xTaskCreatePinnedToCore(audioTask, "aud", 32768, nullptr, 5, &_task, 1);
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

bool audioIsBusy() { return _busy; }

void soundStop() {
    if (!_busy) return;
    _stopReq = true;
    uint32_t t0 = millis();
    while (_busy && (millis() - t0 < 1000)) vTaskDelay(1);
    _stopReq = false;
}

void soundPlayPath(const char *path) {
    if (!_task || _busy) return;
    strncpy(_req.path, path, sizeof(_req.path) - 1);
    _req.path[sizeof(_req.path) - 1] = '\0';
    _req.type = AUDIO_WAV;
    _busy = true;
    _stopReq = false;
    xTaskNotifyGive(_task);
}
