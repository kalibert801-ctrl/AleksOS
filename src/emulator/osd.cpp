// osd.cpp — nofrendo OSD implementation for RetroESP / CYD board
// Display: LovyanGFX ST7789 320x240 @ 80MHz SPI DMA
// Audio:   I2S internal DAC, GPIO26 (DAC2 / left channel)
// Input:   Pico UART controller via emu_setController()

#include "display/display_manager.h"
#include "input/button_handler.h"
#include "network/web_console.h"
#include "config.h"
#include "settings.h"
#include "driver/i2s.h"
#include "driver/rtc_io.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "esp_attr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <SD.h>
#include "nes_palettes.h"
#include "game_shark.h"

extern "C" {
#include "nofrendo/nofrendo.h"
#include "nofrendo/noftypes.h"
#include "nofrendo/osd.h"
#include "nofrendo/bitmap.h"
#include "nofrendo/vid_drv.h"
#include "nofrendo/nes/nes.h"
#include "nofrendo/nes/nes_pal.h"
#include "nofrendo/nes/nesinput.h"
#include "nofrendo/nes/nesstate.h"
#include "nofrendo/event.h"
#include "nofrendo/log.h"
}

// ─── constants ─────────────────────────────────────────────────────────────
#define NES_SAMPLE_RATE  44100
#define NES_FRAG_SAMPLES 128
#define I2S_PORT         I2S_NUM_0

// Defaults for 1:1 mode (pixel-perfect, centred)
#define VID_X_DEFAULT  ((SCREEN_W - NES_SCREEN_WIDTH) / 2)    // 32
#define VID_Y_DEFAULT  ((SCREEN_H - NES_VISIBLE_HEIGHT) / 2)  //  8

// ── Scale parameters ────────────────────────────────────────────────────────
// Computed once per blit from settings.scale.
// outX/outY — top-left corner on display; outW/outH — output size.
struct ScaleParams { int outX, outY, outW, outH; };

static ScaleParams getScaleParams() {
    switch (settings.scale) {
        case SCALE_FIT:  // Full stretch — fill entire 320×240
            return { 0, 0, SCREEN_W, SCREEN_H };
        case SCALE_43:   // Stretch width to 320, keep NES height (224) centred
            return { 0, VID_Y_DEFAULT, SCREEN_W, NES_VISIBLE_HEIGHT };
        case SCALE_11:   // Pixel-perfect 256×224, centred
        default:
            return { VID_X_DEFAULT, VID_Y_DEFAULT, NES_SCREEN_WIDTH, NES_VISIBLE_HEIGHT };
    }
}

// ─── controller state (set from main task via emu_setController) ───────────
static volatile uint8_t _pad = 0;

extern "C" void emu_setController(uint8_t state) { _pad = state; }

// ─── путь текущего ROM (устанавливается в osd_rom_load) ───────────────────────
static char _romPath[PATH_MAX + 1] = {};

// ─── чистый выход из эмулятора ────────────────────────────────────────────────
// Флаг выставляется из osd_getinput() когда пользователь выбрал "Выход" в меню
// паузы или удержал HOME+SELECT.
// main_loop() в nofrendo.c проверяет его после возврата из nes_emulate()
// и вызывает main_eject() — уже вне стека frame-callback, без краша.
static volatile bool _emuExitReq = false;
extern "C" bool osd_exit_requested(void) { return _emuExitReq; }

// ─── OSD громкости (показывается поверх игры при HOME+UP/DOWN) ────────────────
static int     _volShowFrames = 0;   // кол-во кадров для отображения
static uint32_t _volAdjLastMs = 0;   // rate-limit: не чаще 200 мс

// ─── счётчики osd_getinput ────────────────────────────────────────────────────
// Файловая область видимости — сбрасываются в osd_init() при каждом запуске.
// Раньше были static-локальными → не сбрасывались между сессиями:
//   frameCount уже > 90 → защита первых кадров не работала,
//   homeFrames/exitFrames могли начинать не с нуля.
static int     _inp_frameCount = 0;
static int     _inp_homeFrames = 0;
static int     _inp_exitFrames = 0;
static uint8_t _inp_prevPad    = 0;

// ─── audio ─────────────────────────────────────────────────────────────────
//
// Core 1 (эмулятор): audio_frame() → APU → DSP → PSRAM mono ring
// Core 0 (i2s_out):  audioOutputTask → ring → DRAM stereo buf → i2s_write
//
// DSP: vol → DC-блокер (fc≈7Гц) → TPDF ±63 → soft-knee limiter ±28000 → normalize /120 → 8-bit DAC
// Кольцо МОНО: 4096 × uint16_t = 8 КБ PSRAM (вдвое меньше чем стерео-пары).
// SPSC lockless: Core 1 пишет _rHead ПОСЛЕ данных (release),
//                Core 0 делает барьер ПЕРЕД чтением _rHead (acquire).

static void (*_audio_cb)(void *buf, int len) = nullptr;

extern "C" void osd_setsound(void (*playfunc)(void *buf, int len)) {
    _audio_cb = playfunc;
}

static uint16_t _audio_buf[NES_FRAG_SAMPLES];  // APU пишет сюда (signed int16)

// ── DSP состояние (сбрасывается в osd_init при каждом старте игры) ────────────
static int32_t _dcX = 0, _dcY = 0;     // DC-блокер: убирает смещение от DMC-канала

// ── TPDF дизеринг ────────────────────────────────────────────────────────────
static uint32_t _nes_lfsr = 0xDEADBEEFu;
static inline int32_t nes_tpdf() {
    _nes_lfsr ^= _nes_lfsr << 13;
    _nes_lfsr ^= _nes_lfsr >> 17;
    _nes_lfsr ^= _nes_lfsr << 5;
    int32_t a = (int32_t)(_nes_lfsr & 0x3F);
    _nes_lfsr ^= _nes_lfsr << 13;
    _nes_lfsr ^= _nes_lfsr >> 17;
    _nes_lfsr ^= _nes_lfsr << 5;
    return a - (int32_t)(_nes_lfsr & 0x3F);  // -63..+63
}

// ── Кольцевой буфер МОНО (PSRAM) ──────────────────────────────────────────────
#define RING_SAMPLES 8192u  // степень двойки; 8192 × 2 байт = 16 КБ PSRAM

static uint16_t         *_ring  = nullptr;   // ps_malloc в audio_init
static volatile uint32_t _rHead = 0;         // Core 1 пишет
static volatile uint32_t _rTail = 0;         // Core 0 читает
static TaskHandle_t      _audioOutTask = nullptr;

// ── Двуядерный рендер (Core 0) ────────────────────────────────────────────────
// Core 1 строит _frame → даёт _renderReady → уходит на следующий кадр NES.
// Core 0 получает _renderReady → пушит _frame в SPI → даёт _renderDone.
// Core 1 берёт _renderDone в начале следующего blit (ждёт если Core 0 ещё не закончил).
static SemaphoreHandle_t _renderReady    = nullptr;
static SemaphoreHandle_t _renderDone     = nullptr;
static TaskHandle_t      _renderTask     = nullptr;
static ScaleParams       _renderSp       = {};
static int               _renderVolFrames = 0;

// Forward declarations — полные определения ниже (после структур nofrendo).
static uint16_t *_frame        = nullptr;
static size_t    _framePixels  = 0;
static bool      _frameInPsram = false;   // true → bounce via _line_buf, false → DMA
static uint16_t  _line_buf[SCREEN_W];     // DRAM bounce buffer (320×2 = 640 B)

// ── Core 0: рендер кадра в SPI дисплей ───────────────────────────────────────
// Ждёт _renderReady от Core 1, пушит _frame в LCD, даёт _renderDone.
static void renderOutputTask(void*) {
    while (true) {
        xSemaphoreTake(_renderReady, portMAX_DELAY);

        const ScaleParams sp = _renderSp;
        lcd.startWrite();
        lcd.setAddrWindow(sp.outX, sp.outY, sp.outW, sp.outH);
        if (_frameInPsram) {
            // ESP32 SPI DMA не может читать PSRAM.
            // Копируем строку PSRAM→DRAM (_line_buf) и шлём через DMA — правильный byte-swap.
            // Core 1 в fast-path не трогает _line_buf, поэтому здесь безопасно.
            const uint16_t *src = _frame;
            for (int y = 0; y < sp.outH; y++) {
                memcpy(_line_buf, src, sp.outW * sizeof(uint16_t));
                lcd.writePixels(_line_buf, sp.outW, true);
                src += sp.outW;
            }
        } else {
            lcd.writePixels(_frame, (size_t)sp.outW * sp.outH, true);
        }
        lcd.endWrite();

        if (_renderVolFrames > 0) {
            const int bx = SCREEN_W - 82, by = 4, bw = 78, bh = 20;
            lcd.fillRect(bx - 1, by - 1, bw + 2, bh + 2, 0x0000);
            lcd.drawRect(bx, by, bw, bh, 0xFFFF);
            int fill = (bw - 2) * settings.emuVolume / 100;
            if (fill > 0) lcd.fillRect(bx + 1, by + 1, fill, bh - 2, 0x07E0);
            char vbuf[12];
            snprintf(vbuf, sizeof(vbuf), "%d%%", settings.emuVolume);
            lcd.setFont(&lgfx::fonts::DejaVu9);
            lcd.setTextDatum(MC_DATUM);
            lcd.setTextColor(settings.emuVolume > 50 ? 0x0000 : 0xFFFF);
            lcd.drawString(vbuf, bx + bw / 2, by + bh / 2 + 1);
        }

        xSemaphoreGive(_renderDone);
    }
}

// ── Core 0: вывод в I2S (4× oversample с линейной интерполяцией) ─────────────
// Кольцо заполняется при 44100 Гц (audio_frame); I2S работает при 176400 Гц.
// Для каждого входного сэмпла строим 4 выходных через линейную интерполяцию:
// это убирает ступенчатость zero-order hold и сдвигает imaging-артефакты выше 88кГц,
// где SC8002B уже их отфильтровывает своим RC на входе.
static void audioOutputTask(void*) {
    static uint16_t buf[NES_FRAG_SAMPLES * 4 * 2];  // 4× oversample, стерео (BSS = DMA-safe)
    uint16_t prev = 0x8000;  // стартуем с DC mid-point
    while (true) {
        // ACQUIRE: барьер ПЕРЕД чтением _rHead — гарантирует что данные
        // записанные Core 1 в PSRAM видны этому ядру до проверки счётчика.
        __sync_synchronize();
        uint32_t avail = _rHead - _rTail;

        if (avail >= (uint32_t)NES_FRAG_SAMPLES) {
            uint32_t t = _rTail;
            for (int i = 0; i < NES_FRAG_SAMPLES; i++) {
                uint16_t curr = _ring[(t + (uint32_t)i) & (RING_SAMPLES - 1u)];
                // Линейная интерполяция prev→curr за 4 шага (os=0..3).
                // os=0: prev; os=1: prev+¼Δ; os=2: prev+½Δ; os=3: prev+¾Δ.
                // Следующий цикл: os=0 стартует с curr (т.к. prev=curr).
                for (int os = 0; os < 4; os++) {
                    uint16_t s = (uint16_t)(((uint32_t)prev * (4u - (uint32_t)os) +
                                             (uint32_t)curr * (uint32_t)os) >> 2);
                    buf[(i * 4 + os) * 2]     = s;  // L = GPIO26 (DAC2)
                    buf[(i * 4 + os) * 2 + 1] = s;  // R = заглушка
                }
                prev = curr;
            }
            __sync_synchronize();
            _rTail = t + NES_FRAG_SAMPLES;
        } else {
            // Underrun: плавное затухание к тишине (7/8 за шаг, ~1.5 мс до тишины).
            // Предотвращает щелчок при резком сбросе в 0x8000 и при последующем
            // возобновлении воспроизведения (prev остаётся вблизи 0x8000).
            for (int i = 0; i < NES_FRAG_SAMPLES; i++) {
                prev = (uint16_t)(((uint32_t)prev * 7u + 0x8000u) >> 3);
                uint16_t s = prev;
                buf[(i * 4)     * 2]     = s; buf[(i * 4)     * 2 + 1] = s;
                buf[(i * 4 + 1) * 2]     = s; buf[(i * 4 + 1) * 2 + 1] = s;
                buf[(i * 4 + 2) * 2]     = s; buf[(i * 4 + 2) * 2 + 1] = s;
                buf[(i * 4 + 3) * 2]     = s; buf[(i * 4 + 3) * 2 + 1] = s;
            }
        }
        size_t w;
        i2s_write(I2S_PORT, buf, NES_FRAG_SAMPLES * 4 * 2 * 2, &w, pdMS_TO_TICKS(100));
    }
}

// ── Core 1: генерация PCM ─────────────────────────────────────────────────────
static void audio_frame(void) {
    if (!_audio_cb) return;

    // Держим кольцо на уровне ≥ 5 кадров (~83 мс) — снижает риск заиканий при
    // тяжёлых NES-кадрах (спрайты, сложная логика, медленный ROM-кадр, WiFi-burst).
    // Макс. 6 кадров за раз — быстро восстанавливаемся после дренажа.
    const int ONE_FRAME = NES_SAMPLE_RATE / NES_REFRESH_RATE;  // 735
    // ACQUIRE: читаем актуальный _rTail, который обновляет Core 0 (audioOutputTask)
    __sync_synchronize();
    int fill = (int)(_rHead - _rTail);
    int need = ONE_FRAME * 5 - fill;
    if (need < ONE_FRAME)     need = ONE_FRAME;
    if (need > ONE_FRAME * 6) need = ONE_FRAME * 6;
    int free_space = (int)RING_SAMPLES - fill;
    if (need > free_space) need = free_space;
    if (need <= 0) return;

    uint8_t vol = settings.emuVolume > 100 ? 100 : settings.emuVolume;

    while (need > 0) {
        int n = need > NES_FRAG_SAMPLES ? NES_FRAG_SAMPLES : need;
        _audio_cb(_audio_buf, n);

        for (int i = 0; i < n; i++) {
            int32_t s = (int32_t)(int16_t)_audio_buf[i];  // signed int16 из APU
            s = s * (int32_t)vol / 100;
            // DC-блокер: y[n] = x[n] − x[n-1] + 0.999·y[n-1], fc ≈ 7 Гц.
            // Убирает DC-смещение от DMC-канала и асимметрии меандров.
            {
                int32_t ny = s - _dcX + _dcY * 999 / 1000;
                _dcX = s;
                _dcY = ny;
                s = ny;
            }
            s += nes_tpdf();  // TPDF дизеринг ±63

            // Soft-knee limiter: до ±22000 — линейно, выше — 3:1 компрессия.
            {
                const int32_t KNEE = 22000, CEIL = 28000;
                int32_t a = s < 0 ? -s : s;
                if (a > KNEE) {
                    a = KNEE + (a - KNEE) / 3;
                    if (a > CEIL) a = CEIL;
                    s = s < 0 ? -a : a;
                }
            }
            // Нормируем к 8-bit DAC. Делитель 120: ±15000 → ±125 → DAC 3..253 (98%),
            // лимитер выше защищает от перегрузки. Было (s+0x8000)&0xFF00 — только 46%.
            int32_t dv = s / 120 + 128;
            if (dv < 0) dv = 0; else if (dv > 255) dv = 255;
            uint16_t out = (uint16_t)dv << 8;

            // SPSC запись в моно-кольцо
            uint32_t h = _rHead;
            if ((int)(RING_SAMPLES - (h - _rTail)) > 0) {
                _ring[h & (RING_SAMPLES - 1u)] = out;
                __sync_synchronize();  // RELEASE: данные в PSRAM ДО обновления _rHead
                _rHead = h + 1u;
            }
        }
        need -= n;
    }
}

static bool _i2sNesInstalled = false;  // avoid redundant uninstall (esp. after soundStop)

static int audio_init(void) {
    // Uninstall only if we installed it before (prevents harmless but noisy error log
    // when soundStop() in main.cpp already called i2s_driver_uninstall before us).
    if (_i2sNesInstalled) {
        i2s_driver_uninstall(I2S_PORT);
        _i2sNesInstalled = false;
    }
    // Release pin from any LEDC usage before I2S takes it
    ledcDetachPin(AUDIO_PIN);
    pinMode(AUDIO_PIN, INPUT);

    i2s_config_t cfg = {};
    cfg.mode              = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    // 4× oversample: APU генерирует при 44100 Гц; I2S работает при 176400 Гц.
    // audioOutputTask строит 4 выходных сэмпла (линейная интерполяция) на каждый входной.
    cfg.sample_rate       = NES_SAMPLE_RATE * 4;  // 176400 Гц
    cfg.bits_per_sample   = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format    = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    cfg.intr_alloc_flags  = 0;
    // DMA 4×256 @ 176400 Гц = 5.8 мс задержки; стерео 4 байт → 4×256×4 = 4 KB (vs 16 KB).
    // Уменьшено с 8×512 чтобы DMA аллокация проходила при активном WiFi (~100 KB занято).
    cfg.dma_buf_count     = 4;    // 4 буфера
    cfg.dma_buf_len       = 256;  // 256 сэмплов × 4 = 5.8 мс каждый
    // rate-limiter: i2s_write блокируется когда все 4 буфера заполнены.
    // APLL даёт точный 176400 Гц из кварца.
    // Если WiFi активен — APLL недоступен, откатываемся на APB (погрешность ~1.2%, незаметна).
    {
        wifi_mode_t wmode = WIFI_MODE_NULL;
        esp_wifi_get_mode(&wmode);
        cfg.use_apll = (wmode == WIFI_MODE_NULL);
        printf("[OSD] I2S clock: %s\n", cfg.use_apll ? "APLL (exact)" : "APB (WiFi active)");
    }
    printf("[OSD] DMA free: %u bytes\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));

    if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) return -1;
    _i2sNesInstalled = true;
    i2s_set_pin(I2S_PORT, nullptr);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN); // GPIO26 = DAC2 = left channel

    // Кольцо в PSRAM — МОНО, выделяется один раз и переиспользуется между сессиями.
    if (!_ring) {
        _ring = (uint16_t *)ps_malloc((size_t)RING_SAMPLES * sizeof(uint16_t));
        if (!_ring) {
            printf("[OSD] FATAL: audio ring alloc failed (%u KB PSRAM)\n",
                   (unsigned)(RING_SAMPLES * sizeof(uint16_t) / 1024));
            i2s_driver_uninstall(I2S_PORT);
            _i2sNesInstalled = false;
            return -1;
        }
        printf("[OSD] Audio ring: %u KB in PSRAM (mono)\n",
               (unsigned)(RING_SAMPLES * sizeof(uint16_t) / 1024));
    }

    // Pre-fill: 5 кадров тишины (~83 мс) — Core 0 сразу пишет в I2S.
    {
        const uint32_t preFill = (uint32_t)(NES_SAMPLE_RATE / 60) * 5; // 3675 сэмплов
        for (uint32_t i = 0; i < preFill && i < RING_SAMPLES; i++) {
            _ring[i] = 0x8000;
        }
        _rHead = preFill;
        _rTail = 0;
    }

    // Приоритет 20: выше системных задач (FreeRTOS timer=1, main=1), ниже esp_timer (22)
    // и WiFi (23). Это гарантирует ответ на TX_DONE в течение <1 мс,
    // иначе низкоприоритетные задачи вытесняли Core 0 на несколько мс → DMA underrun.
    xTaskCreatePinnedToCore(audioOutputTask, "i2s_out", 2048, nullptr, 20, &_audioOutTask, 0);
    return 0;
}

static void audio_deinit(void) {
    // Fade-out: заполняем кольцо тишиной и даём Core 0 воспроизвести её (~30 мс).
    // Без этого vTaskDelete убивает задачу посреди звука → клацок на выходе из игры.
    if (_ring && _audioOutTask) {
        for (uint32_t i = 0; i < RING_SAMPLES; i++) _ring[i] = 0x8000;
        __sync_synchronize();
        _rHead = _rTail + (RING_SAMPLES / 2);  // 4096 семплов тишины вперёд
        vTaskDelay(pdMS_TO_TICKS(30));          // ждём пока DMA пропустит
    }
    if (_audioOutTask) { vTaskDelete(_audioOutTask); _audioOutTask = nullptr; }
    _rHead = 0;
    _rTail = 0;
    /* _ring intentionally NOT freed — see audio_init for explanation. */
    i2s_driver_uninstall(I2S_PORT);
    _i2sNesInstalled = false;
    // GPIO25 (XPT2046 touch CLK) уходит в RTC-режим при dac_output_disable.
    rtc_gpio_deinit(GPIO_NUM_25);
    // INPUT (high-Z): не тянем GPIO26 к GND (было INPUT_PULLDOWN = клацок в усилителе).
    pinMode(AUDIO_PIN, INPUT);
}

extern "C" void osd_getsoundinfo(sndinfo_t *info) {
    info->sample_rate = NES_SAMPLE_RATE;
    info->bps         = 16;
}

// ─── video ─────────────────────────────────────────────────────────────────
static uint16_t _pal[256];
// 8-bit indexed NES framebuffer — allocated in PSRAM to keep internal DRAM free.
// nofrendo renders palette indices here; drv_custom_blit converts to RGB565.
// PSRAM access is fine: the CPU reads it line-by-line, no DMA involved.
static uint8_t  *_fb  = nullptr;   // ps_malloc'd in osd_init()
static bitmap_t *_bmp = nullptr;
// Ссылка на primary_buffer nofrendo — обновляется в drv_custom_blit каждый кадр.
// PPU рендерит именно в primary_buffer (bmp_create), а не в _bmp (bmp_createhw).
// Используем для скриншота вместо _bmp.
static bitmap_t *_blitBmp = nullptr;

// RGB565 полный кадровый буфер — выделяется из внутренней DRAM heap (не PSRAM).
// ESP32 SPI DMA может читать только из внутренней DRAM, поэтому именно heap.
// 256×224×2 = 112 КБ; heap_caps_malloc с MALLOC_CAP_INTERNAL гарантирует DRAM.
// _frame и _framePixels объявлены выше (перед renderOutputTask) для видимости.

static int  drv_init(int w, int h) {
    // _frame is allocated in osd_init(); if it failed we run in fallback mode
    return 0;
}
static void drv_shutdown(void) {
    /* vid_shutdown() calls bmp_destroy(&primary_buffer) which frees the
    ** bitmap_t struct that _bmp also points to.  Clear _bmp here so that
    ** osd_shutdown() does not attempt a second bmp_destroy on freed memory
    ** (which reads garbage, may call free() on _fb PSRAM → CORRUPT HEAP). */
    _bmp = nullptr;
}
static int  drv_set_mode(int w, int h) { return 0; }
static void drv_clear(uint8 color)     {}

static void drv_set_palette(rgb_t *pal) {
    const rgb_t *src = nes_palette_get(settings.nesPalette);

    if (src) {
        for (int i = 0; i < 64; i++) {
            uint16_t c = ((uint16_t)(src[i].r >> 3) << 11) |
                         ((uint16_t)(src[i].g >> 2) << 5)  |
                          (uint16_t)(src[i].b >> 3);
            for (int rep = 0; rep < 4; rep++) _pal[i + rep * 64] = c;
        }
    } else {
        for (int i = 0; i < 256; i++) {
            uint16_t r = ((uint16_t)pal[i].r >> 3) & 0x1F;
            uint16_t g = ((uint16_t)pal[i].g >> 2) & 0x3F;
            uint16_t b = ((uint16_t)pal[i].b >> 3) & 0x1F;
            _pal[i] = (r << 11) | (g << 5) | b;
        }
    }
}

static bitmap_t *drv_lock_write(void) {
    if (!_bmp && _fb)
        _bmp = bmp_createhw(_fb, NES_SCREEN_WIDTH, NES_SCREEN_HEIGHT, NES_SCREEN_WIDTH);
    return _bmp;
}

static void drv_free_write(int nd, rect_t *dr) {}

// NES renders lines 0..239; lines 0..7 and 232..239 are off-screen.
// We display lines 8..231 (224 visible lines) centred on the 240-line screen.
#define VID_FIRST_LINE  ((NES_SCREEN_HEIGHT - NES_VISIBLE_HEIGHT) / 2)  // 8

// ── FPS / resource diagnostics ──────────────────────────────────────────────
static void blit_print_stats(void) {
    static uint32_t _frameCount = 0;
    static uint32_t _lastMs     = 0;
    if (_lastMs == 0) { _lastMs = millis(); return; } // пропустить первое неполное окно
    _frameCount++;
    uint32_t now     = millis();
    uint32_t elapsed = now - _lastMs;
    if (elapsed >= 3000) {
        if (settings.diagFPS) {
            float fps      = _frameCount * 1000.0f / elapsed;
            uint32_t heap  = ESP.getFreeHeap()  / 1024;
            uint32_t psram = ESP.getFreePsram() / 1024;
            webConsolePrintf("[FPS] %.1f  heap=%u KB  psram=%u KB\n", fps, heap, psram);
        }
        _frameCount = 0;
        _lastMs     = now;
    }
}

// _line_buf объявлен выше (перед renderOutputTask) для видимости обоим контекстам.

static void drv_custom_blit(bitmap_t *bmp, int nd, rect_t *dr) {
    _blitBmp = bmp;  // сохраняем primary_buffer для скриншота
    // ── Тактирование: nofrendo_ticks (60 Гц FreeRTOS таймер) ─────────────────
    // FPS кап УБРАН (v14.52) — он был причиной дёрганья картинки.
    //
    // Причина: vTaskDelay/busy-wait внутри blit блокировал Core 1 на 8–16 мс.
    // За это время FreeRTOS 60 Гц таймер успевал сработать ещё раз →
    // в nes_emulate: frames_to_render ≥ 2 → autoframeskip пропускал кадр →
    // каждые 2–3 кадра рывок изображения.
    //
    // Правильное тактирование обеспечивает nofrendo_ticks: nes_emulate() ждёт
    // тик таймера перед рендером следующего кадра. Аудио-кольцо 4096 сэмплов
    // (≈93 мс) защищает от переполнения без капа.

    // ── Game Shark: пишем коды в RAM NES каждый кадр ────────────────────────
    if (settings.gsEnabled) {
        nes_t *_nesCtx = nes_getcontextptr();
        if (_nesCtx && _nesCtx->cpu) {
            for (int _gi = 0; _gi < 8; _gi++) {
                if (!settings.gsCodes[_gi][0]) continue;
                uint16_t _addr; uint8_t _val;
                if (gs_decode(settings.gsCodes[_gi], &_addr, &_val)) {
                    int _bank = _addr >> NES6502_BANKSHIFT;
                    if (_bank < NES6502_NUMBANKS && _nesCtx->cpu->mem_page[_bank])
                        _nesCtx->cpu->mem_page[_bank][_addr & NES6502_BANKMASK] = _val;
                }
            }
        }
    }

    // ── Звук ПЕРВЫМ: Core 0 начинает писать в I2S пока Core 1 рисует дисплей ──
    // audio_frame генерирует сэмплы и сигналит Core 0. Core 0 тут же стартует
    // i2s_write. Core 1 уходит в blit. Оба ядра работают параллельно.
    audio_frame();

    const ScaleParams sp = getScaleParams();

    // ── Fixed-point шаги масштабирования ─────────────────────────────────────
    // Заменяем деление в каждом пикселе (35 тактов на ESP32) на сложение+сдвиг (3 такта).
    // Для SCALE_43 (320×224): было 71680 делений ≈10мс → теперь <1мс.
    const uint32_t xStep = ((uint32_t)NES_SCREEN_WIDTH  << 12) / (uint32_t)sp.outW;
    const uint32_t yStep = ((uint32_t)NES_VISIBLE_HEIGHT << 12) / (uint32_t)sp.outH;

    if (_frame && (size_t)sp.outW * sp.outH <= _framePixels) {
        // ── Быстрый путь: строим _frame → сигнал Core 0 ─────────────────────
        // Frame skip: если Core 0 ещё толкает предыдущий кадр в SPI — не ждём,
        // а пропускаем рендер (до 2 кадров подряд). Core 1 уходит в NES сразу.
        // На 3-й подряд — всё равно ждём чтобы не морозить экран.
        static int _skipCount = 0;
        if (_skipCount < 2) {
            if (xSemaphoreTake(_renderDone, 0) == pdFALSE) {
                _skipCount++;
                blit_print_stats();
                return;
            }
        } else {
            if (xSemaphoreTake(_renderDone, portMAX_DELAY) == pdFALSE) { return; }
        }
        _skipCount = 0;

        uint16_t *dst = _frame;
        uint32_t yFp  = 0;
        for (int oy = 0; oy < sp.outH; oy++) {
            const uint8_t *src = bmp->line[(yFp >> 12) + VID_FIRST_LINE];
            uint32_t xFp = 0;
            for (int ox = 0; ox < sp.outW; ox++) {
                *dst++ = _pal[src[xFp >> 12]];
                xFp += xStep;
            }
            yFp += yStep;
        }
        if (settings.scanlines) {
            for (int y = 1; y < sp.outH; y += 2)
                for (int x = 0; x < sp.outW; x++)
                    _frame[y * sp.outW + x] = ~((~_frame[y * sp.outW + x] & 0xF7DE) >> 1);
        }

        // Передаём параметры Core 0 ДО сигнала (happens-before через семафор)
        _renderSp        = sp;
        _renderVolFrames = _volShowFrames;
        if (_volShowFrames > 0) _volShowFrames--;

        // Сигнал Core 0 — Core 1 немедленно уходит на следующий кадр NES!
        xSemaphoreGive(_renderReady);

    } else {
        // ── Резервный путь: строка за строкой на Core 1 ──────────────────────
        xSemaphoreTake(_renderDone, portMAX_DELAY);

        lcd.startWrite();
        lcd.setAddrWindow(sp.outX, sp.outY, sp.outW, sp.outH);
        uint32_t yFp = 0;
        for (int oy = 0; oy < sp.outH; oy++) {
            const uint8_t *src = bmp->line[(yFp >> 12) + VID_FIRST_LINE];
            uint32_t xFp = 0;
            for (int ox = 0; ox < sp.outW; ox++) {
                _line_buf[ox] = _pal[src[xFp >> 12]];
                xFp += xStep;
            }
            lcd.writePixels(_line_buf, sp.outW, true);
            yFp += yStep;
        }
        lcd.endWrite();

        if (_volShowFrames > 0) {
            _volShowFrames--;
            const int bx = SCREEN_W - 82, by = 4, bw = 78, bh = 20;
            lcd.fillRect(bx - 1, by - 1, bw + 2, bh + 2, 0x0000);
            lcd.drawRect(bx, by, bw, bh, 0xFFFF);
            int fill = (bw - 2) * settings.emuVolume / 100;
            if (fill > 0) lcd.fillRect(bx + 1, by + 1, fill, bh - 2, 0x07E0);
            char vbuf[12];
            snprintf(vbuf, sizeof(vbuf), "%d%%", settings.emuVolume);
            lcd.setFont(&lgfx::fonts::DejaVu9);
            lcd.setTextDatum(MC_DATUM);
            lcd.setTextColor(settings.emuVolume > 50 ? 0x0000 : 0xFFFF);
            lcd.drawString(vbuf, bx + bw / 2, by + bh / 2 + 1);
        }

        xSemaphoreGive(_renderDone);
    }

    blit_print_stats();
}

static viddriver_t _driver = {
    "RetroESP",
    drv_init,
    drv_shutdown,
    drv_set_mode,
    drv_set_palette,
    drv_clear,
    drv_lock_write,
    drv_free_write,
    drv_custom_blit,
    false
};

extern "C" void osd_getvideoinfo(vidinfo_t *info) {
    info->default_width  = NES_SCREEN_WIDTH;
    info->default_height = NES_VISIBLE_HEIGHT;
    info->driver         = &_driver;
}

// ─── timer ─────────────────────────────────────────────────────────────────
static TimerHandle_t _nesTimer = nullptr;   // BUG-1 fix: track handle to avoid leak

extern "C" int osd_installtimer(int frequency, void *func, int funcsize,
                                void *counter, int countersize) {
    // Stop and delete any timer from the previous emulator session.
    if (_nesTimer) {
        xTimerStop(_nesTimer, 0);
        xTimerDelete(_nesTimer, portMAX_DELAY);
        _nesTimer = nullptr;
    }
    // Округлённое деление: (1000 + 30) / 60 = 17 тиков вместо 16.
    // Без округления: 1000/60 = 16 → 62.5 Гц; с округлением: 17 → 58.8 Гц.
    // Оба варианта имеют погрешность ~4%, т.к. 60 Гц не делится нацело на 1000 Гц.
    // Округление вверх предпочтительнее: NES работает чуть медленнее реального
    // времени (58.8 вместо 62.5) → аудиокольцо не переполняется, нет пропусков.
    _nesTimer = xTimerCreate("nes",
        (configTICK_RATE_HZ + frequency/2) / frequency, pdTRUE, nullptr,
        (TimerCallbackFunction_t)func);
    xTimerStart(_nesTimer, 0);
    return 0;
}

// ─── Helper: извлечь имя ROM из пути ─────────────────────────────────────────
static void _romNameFromPath(const char *path, char *name, size_t maxLen) {
    const char *slash = strrchr(path, '/');
    const char *base  = slash ? slash + 1 : path;
    strncpy(name, base, maxLen - 1);
    name[maxLen - 1] = '\0';
    char *dot = strrchr(name, '.');
    if (dot) *dot = '\0';
}

// ─── Скриншот: сохраняем текущий кадр на SD как RAW файл ────────────────────
// Формат: 4 байта заголовка (w_lo, w_hi, h_lo, h_hi) + RGB565 пиксели.
//
// Файл сохраняется как /Screenshots/<romname>_NNN.bmp (24-bit BMP, открывается на ПК/телефоне).
// Обложка /Screenshots/<romname>.raw НЕ перезаписывается, если уже существует
// (первый скриншот устанавливает обложку автоматически, остальные — только через галерею).
static void _takeScreenshot(void) {
    const ScaleParams sp = getScaleParams();
    uint16_t w = (uint16_t)sp.outW;
    uint16_t h = (uint16_t)sp.outH;

    if (!SD.exists("/Screenshots")) SD.mkdir("/Screenshots");
    char romName[64];
    _romNameFromPath(_romPath, romName, sizeof(romName));

    // ── Находим следующий свободный номер ─────────────────────────────────────
    char path[96];
    int shotNum = 1;
    while (shotNum <= 999) {
        snprintf(path, sizeof(path), "/Screenshots/%s_%03d.bmp", romName, shotNum);
        if (!SD.exists(path)) break;
        shotNum++;
    }
    if (shotNum > 999) {
        printf("[EMU] Screenshot: all 999 slots used for %s\n", romName);
        return;
    }

    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        printf("[EMU] Screenshot: cannot create %s\n", path);
        return;
    }

    // ── BMP заголовок (54 байта, 24-bit RGB, top-down) ───────────────────────
    const uint32_t rowBytes = ((uint32_t)w * 3 + 3) & ~3u;  // выравнивание до 4 байт
    const uint32_t fileSize = 54 + rowBytes * h;
    uint8_t hdr[54] = {};
    hdr[0]='B'; hdr[1]='M';
    hdr[2]=(uint8_t)( fileSize        & 0xFF);
    hdr[3]=(uint8_t)((fileSize >>  8) & 0xFF);
    hdr[4]=(uint8_t)((fileSize >> 16) & 0xFF);
    hdr[5]=(uint8_t)((fileSize >> 24) & 0xFF);
    hdr[10]=54;                             // offset до пикселей
    hdr[14]=40;                             // размер BITMAPINFOHEADER
    hdr[18]=(uint8_t)(w & 0xFF); hdr[19]=(uint8_t)(w >> 8);
    // Отрицательная высота = top-down (строки сверху вниз, как в _frame)
    int32_t negH = -(int32_t)h;
    hdr[22]=(uint8_t)( negH        & 0xFF);
    hdr[23]=(uint8_t)((negH >>  8) & 0xFF);
    hdr[24]=(uint8_t)((negH >> 16) & 0xFF);
    hdr[25]=(uint8_t)((negH >> 24) & 0xFF);
    hdr[26]=1;                              // color planes
    hdr[28]=24;                             // bits per pixel
    f.write(hdr, 54);

    // ── Построчная запись: RGB565 → BGR888 ────────────────────────────────────
    uint8_t *rowBuf = (uint8_t *)malloc(rowBytes);
    if (!rowBuf) { f.close(); SD.remove(path); return; }
    memset(rowBuf, 0, rowBytes);

    if (_frame && _framePixels >= (size_t)w * h) {
        for (int oy = 0; oy < h; oy++) {
            const uint16_t *src = _frame + oy * w;
            for (int ox = 0; ox < w && rowBuf; ox++) {
                uint16_t px = src[ox];  // lcd.color565() format
                rowBuf[ox*3+0] = (uint8_t)((px << 3) & 0xF8);   // B
                rowBuf[ox*3+1] = (uint8_t)((px >> 3) & 0xFC);   // G
                rowBuf[ox*3+2] = (uint8_t)((px >> 8) & 0xF8);   // R
            }
            f.write(rowBuf, rowBytes);
        }
    } else if (_blitBmp) {
        const uint32_t xStep = ((uint32_t)NES_SCREEN_WIDTH  << 12) / (uint32_t)w;
        const uint32_t yStep = ((uint32_t)NES_VISIBLE_HEIGHT << 12) / (uint32_t)h;
        uint32_t yFp = 0;
        for (int oy = 0; oy < h; oy++) {
            const uint8_t *src = _blitBmp->line[(yFp >> 12) + VID_FIRST_LINE];
            uint32_t xFp = 0;
            for (int ox = 0; ox < w && rowBuf; ox++) {
                uint16_t px = _pal[src[xFp >> 12]];
                rowBuf[ox*3+0] = (uint8_t)((px << 3) & 0xF8);   // B
                rowBuf[ox*3+1] = (uint8_t)((px >> 3) & 0xFC);   // G
                rowBuf[ox*3+2] = (uint8_t)((px >> 8) & 0xF8);   // R
                xFp += xStep;
            }
            f.write(rowBuf, rowBytes);
            yFp += yStep;
        }
        printf("[EMU] Screenshot: fallback render %dx%d\n", w, h);
    } else {
        free(rowBuf);
        f.close(); SD.remove(path);
        printf("[EMU] Screenshot: no frame data\n");
        return;
    }

    free(rowBuf);
    f.flush(); f.close();
    printf("[EMU] Screenshot saved: %s (%dx%d)\n", path, w, h);

    // ── Если обложка ещё не существует — пишем .raw из текущего кадра ─────────
    char coverPath[96];
    snprintf(coverPath, sizeof(coverPath), "/Screenshots/%s.raw", romName);
    if (!SD.exists(coverPath)) {
        File dst = SD.open(coverPath, FILE_WRITE);
        if (dst) {
            uint8_t ohdr[4] = { (uint8_t)(w & 0xFF), (uint8_t)(w >> 8),
                                 (uint8_t)(h & 0xFF), (uint8_t)(h >> 8) };
            dst.write(ohdr, 4);
            if (_frame && _framePixels >= (size_t)w * h) {
                dst.write((uint8_t *)_frame, (size_t)w * h * 2);
            } else if (_blitBmp) {
                const uint32_t xStep = ((uint32_t)NES_SCREEN_WIDTH  << 12) / (uint32_t)w;
                const uint32_t yStep = ((uint32_t)NES_VISIBLE_HEIGHT << 12) / (uint32_t)h;
                uint32_t yFp = 0;
                for (int oy = 0; oy < h; oy++) {
                    const uint8_t *src2 = _blitBmp->line[(yFp >> 12) + VID_FIRST_LINE];
                    uint32_t xFp = 0;
                    for (int ox = 0; ox < w; ox++) {
                        _line_buf[ox] = _pal[src2[xFp >> 12]];
                        xFp += xStep;
                    }
                    dst.write((uint8_t *)_line_buf, (size_t)w * 2);
                    yFp += yStep;
                }
            }
            dst.flush(); dst.close();
            printf("[EMU] Cover art set: %s\n", coverPath);
        }
    }
}

// ─── Remote view: expose current NES framebuffer ─────────────────────────────
const void* osdFramePtr()  { return _frame; }
size_t      osdFrameSize() { return _frame ? _framePixels * 2 : 0; }
uint16_t    osdFrameW()    { return (uint16_t)_renderSp.outW; }
uint16_t    osdFrameH()    { return (uint16_t)_renderSp.outH; }

// ─── In-game меню паузы ────────────────────────────────────────────────────────
// Вызывается из osd_getinput() при удержании HOME.
// Блокирует выполнение (эмулятор приостанавливается), рисует оверлей,
// обрабатывает ввод, выполняет выбранное действие.
static void _ingameMenu(void) {
    // Wait for Core 0 to finish its current SPI push before we touch LCD directly.
    // RAII guard gives _renderDone back on every exit (return/throw) so
    // drv_custom_blit can take it again when NES resumes.
    if (_renderDone) xSemaphoreTake(_renderDone, portMAX_DELAY);
    struct _RRGuard { ~_RRGuard() { if (_renderDone) xSemaphoreGive(_renderDone); } } _guard;

    // ── Рисуем затемнённый фон поверх игры ──────────────────────────────
    const int MX = 35, MY = 50, MW = 250, MH = 140;
    lcd.fillRect(MX, MY, MW, MH, 0x0820);         // тёмно-синий фон
    lcd.drawRoundRect(MX, MY, MW, MH, 8, 0xAD55); // жёлтая рамка
    lcd.drawRoundRect(MX+1, MY+1, MW-2, MH-2, 7, 0x630C); // внутренняя тень

    // Заголовок
    lcd.setFont(&lgfx::fonts::DejaVu12);
    lcd.setTextDatum(MC_DATUM);
    lcd.setTextColor(0xFD20);  // золотой
    lcd.drawString("|| PAUSE ||", SCREEN_W / 2, MY + 14);
    lcd.drawFastHLine(MX + 8, MY + 24, MW - 16, 0xAD55);

    const char *items[] = {
        "Continue",
        "Save State",
        "Load State",
        "Screenshot",
        "Exit to Menu"
    };
    const int N = 5;
    int sel  = 0;

    auto drawItems = [&]() {
        for (int i = 0; i < N; i++) {
            int iy = MY + 32 + i * 20;
            bool s = (i == sel);
            lcd.fillRect(MX + 2, iy, MW - 4, 19, s ? 0x001F : 0x0820);
            lcd.setFont(&lgfx::fonts::DejaVu9);
            lcd.setTextDatum(MC_DATUM);
            lcd.setTextColor(s ? 0xFFFF : 0xC618);
            lcd.drawString(items[i], SCREEN_W / 2, iy + 9);
        }
    };
    drawItems();

    // ── Дебаунс: ждём ПОЛНОГО отпускания HOME (до 3 с) перед опросом ──────────
    // Без этого: если HOME ещё зажата на входе в цикл, level-triggered проверка
    // немедленно закроет меню → меню мигает в цикле.
    {
        uint32_t to = millis() + 3000;
        while ((buttons.readSysCurrent() & BTN_SYS_HOME) && millis() < to) {
            buttons.update();
            vTaskDelay(pdMS_TO_TICKS(16));
        }
        vTaskDelay(pdMS_TO_TICKS(80));  // короткий debounce после отпускания
    }

    bool    homePrev = false;   // для edge-detect повторного нажатия HOME
    uint8_t prevPad  = 0xFF;
    while (true) {
        buttons.update();
        uint8_t raw    = buttons.readCurrent();
        uint8_t curPad = buttons.applyBtnMap(raw);
        uint8_t newBtn = curPad & ~prevPad;
        prevPad        = curPad;

        bool homeNow = (buttons.readSysCurrent() & BTN_SYS_HOME) != 0;

        if (newBtn & BTN_UP)   { sel = (sel - 1 + N) % N; drawItems(); }
        if (newBtn & BTN_DOWN) { sel = (sel + 1)     % N; drawItems(); }

        bool confirm = (newBtn & (BTN_STA | BTN_SEL));  // START или SELECT подтверждает
        bool cancel  = (newBtn & BTN_B);                 // B = продолжить

        if (cancel) { sel = 0; confirm = true; }         // B → Continue

        // ── Значок-уведомление под меню (вместо невидимого текста) ─────────────
        // ok=true → зелёный круг, ok=false → красный; shape: 0=дискета, 1=камера, 2=X
        auto showIcon = [&](bool ok, int shape) {
            const int icx = SCREEN_W / 2;
            const int icy = MY + MH + 24;   // ниже меню (190+24=214)
            uint16_t col  = ok ? (uint16_t)0x07E0 : (uint16_t)0xF800;
            lcd.fillCircle(icx, icy, 16, 0x0000);   // чёрное кольцо
            lcd.fillCircle(icx, icy, 14, col);        // цветная заливка
            if (shape == 0) {
                // Дискета (белый прямоугольник с окошком и шторкой)
                lcd.fillRect(icx - 6, icy - 6, 12, 12, 0xFFFF);
                lcd.fillRect(icx - 4, icy - 6, 8,  5, 0x39C7);  // верхний ярлык (серый)
                lcd.fillRect(icx - 1, icy + 0, 4,  5, 0x39C7);  // нижняя шторка
            } else if (shape == 1) {
                // Камера (тело + видоискатель + объектив)
                lcd.fillRoundRect(icx - 7, icy - 2, 14, 9, 1, 0xFFFF);  // корпус
                lcd.fillRect(icx - 3, icy - 5, 6, 4, 0xFFFF);            // видоискатель
                lcd.fillCircle(icx, icy + 2, 3, col);   // отверстие объектива
                lcd.fillCircle(icx, icy + 2, 1, 0xFFFF); // блик
            } else {
                // X (ошибка)
                lcd.drawLine(icx-5, icy-5, icx+5, icy+5, 0xFFFF);
                lcd.drawLine(icx-5, icy+5, icx+5, icy-5, 0xFFFF);
                lcd.drawLine(icx-4, icy-5, icx+5, icy+4, 0xFFFF);
                lcd.drawLine(icx-5, icy-4, icx+4, icy+5, 0xFFFF);
                lcd.drawLine(icx-4, icy+5, icx+5, icy-4, 0xFFFF);
                lcd.drawLine(icx-5, icy+4, icx+4, icy-5, 0xFFFF);
            }
            delay(900);
        };

        if (confirm) {
            switch (sel) {
                case 0:  // Continue
                    break;
                case 1:  // Save State
                    state_setslot(0);
                    printf("[SS] Saving to: %s.ss0\n", nes_getcontextptr()->rominfo->filename);
                    {
                        int r = state_save();
                        showIcon(r == 0, r == 0 ? 0 : 2);  // дискета или X
                    }
                    break;
                case 2:  // Load State
                    state_setslot(0);
                    printf("[SS] Loading from: %s.ss0\n", nes_getcontextptr()->rominfo->filename);
                    {
                        int r = state_load();
                        showIcon(r == 0, r == 0 ? 0 : 2);  // дискета или X
                    }
                    break;
                case 3:  // Screenshot
                    _takeScreenshot();
                    showIcon(true, 1);  // камера
                    break;
                case 4:  // Exit to Menu
                    _emuExitReq = true;
                    nes_poweroff();
                    return;
            }
            return;
        }

        // HOME повторно нажат (edge: не было → стало) → Continue
        // Edge-trigger гарантирует: нужно отпустить HOME и нажать снова,
        // а не просто «HOME сейчас зажата» (level-trigger вызывал мгновенное закрытие).
        if (!homePrev && homeNow) { return; }
        homePrev = homeNow;

        vTaskDelay(pdMS_TO_TICKS(16));
    }
}

// ─── input ─────────────────────────────────────────────────────────────────
//
// Физическая разводка Pico → биты пакета (подтверждено диагностикой):
//   бит 0 = START  (BTN_A   = 0x01)
//   бит 1 = SELECT (BTN_B   = 0x02)
//   бит 2 = B      (BTN_STA = 0x08)  ← физически B, не A
//   бит 3 = A      (BTN_SEL = 0x04)  ← физически A, не B
//   бит 4 = UP  / бит 5 = DOWN / бит 6 = LEFT / бит 7 = RIGHT
//
// Комбо выхода: SELECT + START (биты 0+1) удерживать 3 секунды.
//
extern "C" void osd_getinput(void) {
    // Опрашиваем Pico по Serial2 каждый кадр (loop() заблокирован в emu_run).
    buttons.update();

    // Кормим watchdog каждый кадр (~60 Гц).
    // Если ROM завис и osd_getinput() перестала вызываться, WDT перезагрузит ESP32.
    esp_task_wdt_reset();

    // Применяем таблицу ремапа: физический бит i → settings.btnMap[i]
    // webCurrentMask() добавляет биты от браузера (веб-управление) без side effects.
    uint8_t pad = buttons.applyBtnMap(buttons.readCurrent() | buttons.webCurrentMask());
    bool    homeHeld = (buttons.readSysCurrent() & BTN_SYS_HOME) != 0;

    _inp_frameCount++;

    // ── HOME + UP/DOWN → управление громкостью в игре ─────────────────────────
    // Rate-limit: не чаще одного раза в 200 мс.
    if (homeHeld && _inp_frameCount > 90) {
        uint32_t now = millis();
        if (now - _volAdjLastMs >= 200) {
            uint8_t rawDir = buttons.readCurrent();  // физ. кнопки (до ремапа)
            if (rawDir & BTN_UP) {
                if (settings.emuVolume < 100) {
                    settings.emuVolume = (uint8_t)min(100, (int)settings.emuVolume + 5);
                }
                _volShowFrames = 90;
                _volAdjLastMs  = now;
                _inp_homeFrames = 0;  // не открываем меню пока регулируем громкость
                return;
            }
            if (rawDir & BTN_DOWN) {
                if (settings.emuVolume > 0) {
                    settings.emuVolume = (uint8_t)max(0, (int)settings.emuVolume - 5);
                }
                _volShowFrames = 90;
                _volAdjLastMs  = now;
                _inp_homeFrames = 0;
                return;
            }
        } else if (buttons.readCurrent() & (BTN_UP | BTN_DOWN)) {
            // Кнопка зажата но rate-limit ещё не истёк — просто не выходим
            _inp_homeFrames = 0;
            return;
        }
    }

    // ── Кнопка HOME → открытие меню паузы ────────────────────────────────────────
    // Первые 90 кадров игнорируем (Pico инициализируется).
    // Порог 3 кадра (~50 мс) = достаточно для уверенного нажатия,
    // но ощущается как простой тап — не нужно удерживать.
    if (_inp_frameCount > 90 && homeHeld) {
        if (++_inp_homeFrames >= 3) {
            _inp_homeFrames = 0;
            printf("[EMU] HOME → pause menu\n");
            _ingameMenu();  // блокирует до выбора пользователем
            if (_emuExitReq) return;
            // ── Защита от повторного открытия: ждём отпускания HOME ──────────
            // _ingameMenu() уже ждёт отпускания в начале, но если пользователь
            // нажал HOME для закрытия и ещё держит — сбрасываем счётчик здесь.
            {
                uint32_t to = millis() + 1000;
                while ((buttons.readSysCurrent() & BTN_SYS_HOME) && millis() < to) {
                    buttons.update();
                    vTaskDelay(pdMS_TO_TICKS(16));
                }
            }
            _inp_homeFrames = 0;
            _inp_exitFrames = 0;
            return;
        }
    } else {
        _inp_homeFrames = 0;
    }

    // ── Комбо выхода: SELECT + START ~3 секунды ─────────────────────────────────
    // Резервный вариант (без кнопки HOME). Та же безопасная механика.
    if (_inp_frameCount > 90 && (pad & (BTN_A | BTN_B)) == (BTN_A | BTN_B)) {
        if (++_inp_exitFrames >= 60) {
            _inp_exitFrames = 0;
            printf("[EMU] SELECT+START held → exit\n");
            _emuExitReq = true;
            nes_poweroff();
            return;
        }
    } else {
        _inp_exitFrames = 0;
    }

    // ── Турбо-кнопки: rapid-fire на назначенных кнопках ───────────────────────
    // Турбо переключает нажатые кнопки каждые 2 кадра (30 Гц) для имитации
    // быстрого нажатия. Применяется только к кнопкам в turboMask.
    if (settings.turboEnabled && settings.turboMask) {
        if (_inp_frameCount & 1) {
            // На нечётных кадрах "отпускаем" турбо-кнопки
            pad &= ~settings.turboMask;
        }
    }

    // ── Передаём изменения состояния кнопок в nofrendo ───────────────────────
    //
    // АППАРАТНАЯ РАЗВОДКА контроллера (подтверждена диагностикой):
    //   бит 0 (физ. STA / кнопка START)  → NES START
    //   бит 1 (физ. SEL / кнопка SELECT) → NES SELECT
    //   бит 2 (физ. B)                   → NES B   ← физически B
    //   бит 3 (физ. A)                   → NES A   ← физически A
    //
    static const int ev[8] = {
        event_joypad1_start,   // бит 0  физ. START  → NES START
        event_joypad1_select,  // бит 1  физ. SELECT → NES SELECT
        event_joypad1_b,       // бит 2  физ. B      → NES B
        event_joypad1_a,       // бит 3  физ. A      → NES A
        event_joypad1_up,      // бит 4  UP
        event_joypad1_down,    // бит 5  DOWN
        event_joypad1_left,    // бит 6  LEFT
        event_joypad1_right,   // бит 7  RIGHT
    };

    uint8_t changed = pad ^ _inp_prevPad;
    _inp_prevPad = pad;

    // Диагностика нажатий в эмуляторе
    if (settings.diagButtons && changed) {
        uint8_t raw = buttons.readCurrent();
        printf("[EMU] raw=0x%02X map=0x%02X |", raw, pad);
        if (pad & 0x01) printf(" STA");
        if (pad & 0x02) printf(" SEL");
        if (pad & 0x04) printf(" B");
        if (pad & 0x08) printf(" A");
        if (pad & 0x10) printf(" UP");
        if (pad & 0x20) printf(" DOWN");
        if (pad & 0x40) printf(" LEFT");
        if (pad & 0x80) printf(" RIGHT");
        if (pad == 0)   printf(" (release)");
        printf("\n");
    }

    for (int i = 0; i < 8; i++) {
        if (!(changed & (1 << i))) continue;
        event_t h = event_get(ev[i]);
        if (h) h((pad & (1 << i)) ? INP_STATE_MAKE : INP_STATE_BREAK);
    }

    // Синхронизируем глобальный _pad (используется emu_setController извне)
    _pad = pad;
}

extern "C" void osd_getmouse(int *x, int *y, int *button) {}

// ─── file helpers ──────────────────────────────────────────────────────────
extern "C" void osd_fullname(char *fullname, const char *shortname) {
    // Добавляем префикс /sd для VFS: Arduino SD.begin() монтирует карту в /sd,
    // поэтому стандартный fopen("/sd/FomiCon/x.ss0") работает через newlib VFS.
    // Это позволяет libsnss (который использует fopen/fwrite) писать сейв-стейты на SD.
    if (shortname && shortname[0] == '/') {
        snprintf(fullname, PATH_MAX, "/sd%s", shortname);
    } else {
        strncpy(fullname, shortname, PATH_MAX - 1);
        fullname[PATH_MAX - 1] = '\0';
    }
}

extern "C" char *osd_newextension(char *str, char *ext) {
    char *dot = strrchr(str, '.');
    if (!dot) return str;
    // nofrendo выделяет пути как char[PATH_MAX]. Ограничиваем запись реальным
    // остатком буфера от позиции точки, чтобы strcpy не вышел за границу если
    // новое расширение длиннее старого (напр. ".n" → ".sav").
    size_t remain = (size_t)(PATH_MAX - 1) - (size_t)(dot - str);
    strncpy(dot, ext, remain);
    dot[remain] = '\0';
    return str;
}

extern "C" int osd_makesnapname(char *filename, int len) { return -1; }

// ─── ROM data (loaded into PSRAM before main_loop is called) ───────────────
static uint8_t *_rom_buf  = nullptr;
static size_t   _rom_size = 0;

// Called by nofrendo's nes_rom.c to get ROM bytes
extern "C" char *osd_getromdata() {
    return (char *)_rom_buf;
}

// Called by emu_runner.cpp before main_loop to apply Game Genie patches.
// Returns writable pointer to ROM buffer and sets *sizeOut to buffer size.
uint8_t *osd_getrom(uint32_t *sizeOut) {
    if (sizeOut) *sizeOut = (uint32_t)_rom_size;
    return _rom_buf;
}

// Load ROM file from SD into PSRAM. Called by emu_runner before main_loop.
bool osd_rom_load(const char *path) {
    // Сохраняем путь для скриншотов, сейв-стейтов и статистики
    strncpy(_romPath, path, sizeof(_romPath) - 1);
    _romPath[sizeof(_romPath) - 1] = '\0';

    if (_rom_buf) { free(_rom_buf); _rom_buf = nullptr; _rom_size = 0; }
    File f = SD.open(path, FILE_READ);
    if (!f) { printf("[OSD] Cannot open: %s\n", path); return false; }
    _rom_size = f.size();
    _rom_buf  = (uint8_t *)ps_malloc(_rom_size);
    if (!_rom_buf) { f.close(); printf("[OSD] No PSRAM for ROM (%u bytes)\n", (unsigned)_rom_size); return false; }
    size_t got = f.read(_rom_buf, _rom_size);
    f.close();
    if (got != _rom_size) {
        printf("[OSD] ROM read incomplete: %u/%u\n", (unsigned)got, (unsigned)_rom_size);
        free(_rom_buf); _rom_buf = nullptr; _rom_size = 0;
        return false;
    }
    printf("[OSD] ROM loaded: %u bytes\n", (unsigned)_rom_size);
    return true;
}

void osd_rom_free() {
    if (_rom_buf) { free(_rom_buf); _rom_buf = nullptr; _rom_size = 0; }
}

// ─── osd_main (required by linker, we call main_loop directly) ─────────────
extern "C" int osd_main(int argc, char *argv[]) { return 0; }

// ─── init / shutdown ───────────────────────────────────────────────────────
static int logprint(const char *s) { return printf("%s", s); }

extern "C" int osd_init(void) {
    _emuExitReq     = false;  // сбрасываем флаг при каждом запуске эмулятора
    _inp_frameCount = 0;      // сброс счётчиков ввода — иначе защита первых 90 кадров
    _inp_homeFrames = 0;      // не работает при повторном запуске эмулятора
    _inp_exitFrames = 0;
    _inp_prevPad    = 0;
    _volShowFrames  = 0;      // скрываем OSD громкости
    _volAdjLastMs   = 0;
    _dcX = 0; _dcY = 0;       // DC-блокер: чистый старт в каждой игровой сессии
    log_chain_logfunc(logprint);

    // ── NES 8-bit framebuffer → PSRAM (frees ~60 KB of internal DRAM) ──────
    // nofrendo writes palette indices here; no DMA, CPU-only access → PSRAM OK.
    if (!_fb) {
        _fb = (uint8_t *)ps_malloc((size_t)NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT);
        if (!_fb) {
            printf("[OSD] FATAL: cannot allocate NES framebuffer in PSRAM!\n");
            return -1;
        }
        memset(_fb, 0, (size_t)NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT);
        printf("[OSD] NES fb: %u KB in PSRAM\n",
               (unsigned)(NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT / 1024));
    }

    // ── RGB565 frame buffer: пробуем DRAM/DMA, при неудаче — PSRAM ─────────────
    // Постоянный буфер (не освобождается в shutdown) — избегаем фрагментации heap.
    // Приоритет: DRAM DMA (быстрее) → PSRAM (Core 0 pushImage, всё равно параллельно).
    if (!_frame) {
        size_t need_full = (size_t)SCREEN_W * SCREEN_H * sizeof(uint16_t);         // 150 KB
        size_t need_min  = (size_t)NES_SCREEN_WIDTH * NES_VISIBLE_HEIGHT * sizeof(uint16_t); // 112 KB
        size_t largest   = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
        printf("[OSD] DMA heap largest: %u KB, want: %u KB\n",
               (unsigned)(largest / 1024), (unsigned)(need_full / 1024));

        _frame = (uint16_t *)heap_caps_malloc(need_full, MALLOC_CAP_DMA);
        if (_frame) {
            _framePixels = need_full / sizeof(uint16_t); _frameInPsram = false;
            printf("[OSD] DMA frame: 150 KB (all modes, DMA burst)\n");
        } else {
            _frame = (uint16_t *)heap_caps_malloc(need_min, MALLOC_CAP_DMA);
            if (_frame) {
                _framePixels = need_min / sizeof(uint16_t); _frameInPsram = false;
                printf("[OSD] DMA frame: 112 KB (SCALE_11 fast path)\n");
            } else {
                // DRAM исчерпана — используем PSRAM; Core 0 будет использовать pushImage
                _frame = (uint16_t *)ps_malloc(need_full);
                if (_frame) {
                    _framePixels = need_full / sizeof(uint16_t); _frameInPsram = true;
                    printf("[OSD] PSRAM frame: 150 KB (dual-core pushImage, no DMA)\n");
                } else {
                    _framePixels = 0;
                    printf("[OSD] WARNING: frame alloc failed — line-by-line fallback\n");
                }
            }
        }
    }
    // ── Двуядерный рендер: задача Core 0 создаётся один раз, живёт всегда ──────
    // Семафор _renderDone предвыдан (count=1) → первый blit берёт сразу.
    // _renderReady пустой → Core 0 блокирует до первого готового кадра.
    if (!_renderTask) {
        _renderReady = xSemaphoreCreateBinary();
        _renderDone  = xSemaphoreCreateBinary();
        xSemaphoreGive(_renderDone);  // первый drv_custom_blit не ждёт
        xTaskCreatePinnedToCore(renderOutputTask, "render", 8192, nullptr, 5,
                                &_renderTask, 0);
        printf("[OSD] Dual-core render task on Core 0\n");
    }

    // ── Заливаем весь экран чёрным ────────────────────────────────────────────
    // Бордюры вокруг игры (32px слева/справа при 1:1, 8px сверху/снизу при 4:3)
    // остаются чёрными на протяжении всей игровой сессии — больше не "вырви глаз".
    lcd.fillScreen(TFT_BLACK);

    return audio_init();
}

extern "C" void osd_shutdown(void) {
    // Stop timer before tearing down audio/video (timer callback touches ring buf)
    if (_nesTimer) {
        xTimerStop(_nesTimer, 0);
        xTimerDelete(_nesTimer, portMAX_DELAY);
        _nesTimer = nullptr;
    }
    if (_bmp)   { bmp_destroy(&_bmp);    _bmp   = nullptr; }

    // Ждём завершения текущего SPI-буста Core 0 перед освобождением _frame.
    // После этого Core 0 блокирует на _renderReady — не трогает _frame.
    // Даём _renderDone обратно: следующий osd_init/drv_custom_blit не зависнет.
    if (_renderDone) {
        xSemaphoreTake(_renderDone, portMAX_DELAY);
        xSemaphoreGive(_renderDone);
    }

    /* _frame (RGB565 frame buffer) is intentionally NOT freed — persistent like _fb.
    ** Freeing and re-allocating each session causes DRAM fragmentation, making the
    ** next allocation fail (only 45 KB contiguous free after nofrendo runs).
    ** Allocated once early, reused across all sessions. osd_init checks !_frame. */
    /* _fb (PSRAM NES framebuffer) is intentionally NOT freed:
    ** its PSRAM heap block tail may be corrupted by the emulator.
    ** free(_fb) would trigger heap-poisoning panic → reboot.
    ** It is allocated once and reused across sessions (osd_init checks !_fb). */
    audio_deinit();
}
