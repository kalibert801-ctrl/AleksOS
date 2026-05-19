// osd.cpp — nofrendo OSD implementation for RetroESP / CYD board
// Display: LovyanGFX ST7789 320x240 @ 80MHz SPI DMA
// Audio:   I2S internal DAC, GPIO26 (DAC2 / left channel)
// Input:   Pico UART controller via emu_setController()

#include "display/display_manager.h"
#include "input/button_handler.h"
#include "config.h"
#include "settings.h"
#include "driver/i2s.h"
#include "driver/rtc_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_attr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <SD.h>

extern "C" {
#include "nofrendo/nofrendo.h"
#include "nofrendo/noftypes.h"
#include "nofrendo/osd.h"
#include "nofrendo/bitmap.h"
#include "nofrendo/vid_drv.h"
#include "nofrendo/nes/nes.h"
#include "nofrendo/nes/nesinput.h"
#include "nofrendo/event.h"
#include "nofrendo/log.h"
}

// ─── constants ─────────────────────────────────────────────────────────────
#define NES_SAMPLE_RATE  22050
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

// ─── audio ─────────────────────────────────────────────────────────────────
static void (*_audio_cb)(void *buf, int len) = nullptr;

extern "C" void osd_setsound(void (*playfunc)(void *buf, int len)) {
    _audio_cb = playfunc;
}

static uint16_t _audio_buf[NES_FRAG_SAMPLES * 2]; // mono → stereo

static void audio_frame(void) {
    if (!_audio_cb) return;
    int left = NES_SAMPLE_RATE / NES_REFRESH_RATE; // ~368 samples
    // Clamp emuVolume to 0-100 and precompute multiplier once per frame
    uint8_t vol = settings.emuVolume;
    if (vol > 100) vol = 100;

    while (left > 0) {
        int n = (left > NES_FRAG_SAMPLES) ? NES_FRAG_SAMPLES : left;
        _audio_cb(_audio_buf, n);

        // Apply emulator volume:
        // nofrendo DAC output is unsigned 16-bit, centered at 0x8000.
        // Scale distance from center by emuVolume/100, then mono→stereo.
        for (int i = n - 1; i >= 0; i--) {
            int32_t s = (int32_t)_audio_buf[i] - 0x8000; // signed distance from center
            s = (s * vol) / 100;                          // scale by volume
            uint16_t out = (uint16_t)(s + 0x8000);        // back to unsigned
            _audio_buf[i * 2 + 1] = out;
            _audio_buf[i * 2]     = out;
        }
        size_t written;
        i2s_write(I2S_PORT, _audio_buf, (size_t)(4 * n), &written, pdMS_TO_TICKS(10));
        left -= n;
    }
}

static int audio_init(void) {
    // Ensure no stale I2S driver from a previous (failed) session
    i2s_driver_uninstall(I2S_PORT); // safe to call even if not installed
    // Release pin from any LEDC usage before I2S takes it
    ledcDetachPin(AUDIO_PIN);
    pinMode(AUDIO_PIN, INPUT);

    i2s_config_t cfg = {};
    cfg.mode              = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate       = NES_SAMPLE_RATE;
    cfg.bits_per_sample   = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format    = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_MSB;
    cfg.intr_alloc_flags  = 0;
    cfg.dma_buf_count     = 6;
    cfg.dma_buf_len       = NES_FRAG_SAMPLES;
    cfg.use_apll          = false;  // APLL несумісний з WiFi на ESP32 → нестабільний clock

    if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) return -1;
    i2s_set_pin(I2S_PORT, nullptr);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN); // GPIO26 = DAC2 = left channel
    return 0;
}

static void audio_deinit(void) {
    i2s_driver_uninstall(I2S_PORT);
    // Відновлюємо GPIO25 (XPT2046 touch CLK) у цифровий режим.
    // i2s_driver_uninstall в DAC-режимі викликає dac_output_disable для GPIO25
    // → переводить його в RTC/аналоговий режим → touch перестає відповідати.
    rtc_gpio_deinit(GPIO_NUM_25);
    pinMode(AUDIO_PIN, INPUT_PULLDOWN);
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

// RGB565 полный кадровый буфер — выделяется из внутренней DRAM heap (не PSRAM).
// ESP32 SPI DMA может читать только из внутренней DRAM, поэтому именно heap.
// 256×224×2 = 112 КБ; heap_caps_malloc с MALLOC_CAP_INTERNAL гарантирует DRAM.
static uint16_t *_frame = nullptr;

static int  drv_init(int w, int h) {
    // _frame is allocated in osd_init(); if it failed we run in fallback mode
    return 0;
}
static void drv_shutdown(void)         {}
static int  drv_set_mode(int w, int h) { return 0; }
static void drv_clear(uint8 color)     {}

static void drv_set_palette(rgb_t *pal) {
    // display_manager.h sets pc.invert=true (INVON). LovyanGFX compensates in its own
    // drawing calls, but writePixels() is raw — the display hardware will invert every
    // pixel we write. Pre-invert here so that display-invert(~color) == color.
    for (int i = 0; i < 256; i++) {
        uint16_t r = ((uint16_t)pal[i].r >> 3) & 0x1F;
        uint16_t g = ((uint16_t)pal[i].g >> 2) & 0x3F;
        uint16_t b = ((uint16_t)pal[i].b >> 3) & 0x1F;
        _pal[i] = ~((r << 11) | (g << 5) | b); // pre-invert to cancel INVON
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
            printf("[EMU] FPS=%.1f  heap=%u KB  psram=%u KB\n", fps, heap, psram);
        }
        _frameCount = 0;
        _lastMs     = now;
    }
}

// Line buffer широкий на весь дисплей — используется при масштабировании.
// 320 px × 2 байт = 640 байт, статический — не грузит стек каждый кадр.
static uint16_t _line_buf[SCREEN_W];

static void drv_custom_blit(bitmap_t *bmp, int nd, rect_t *dr) {
    const ScaleParams sp = getScaleParams();
    const bool pixel_perfect = (sp.outW == NES_SCREEN_WIDTH &&
                                 sp.outH == NES_VISIBLE_HEIGHT);

    if (pixel_perfect && _frame) {
        // ── Быстрый путь: 1:1, DMA burst ─────────────────────────────────
        uint16_t *dst = _frame;
        for (int y = 0; y < NES_VISIBLE_HEIGHT; y++) {
            const uint8_t *src = bmp->line[y + VID_FIRST_LINE];
            for (int x = 0; x < NES_SCREEN_WIDTH; x++)
                *dst++ = _pal[src[x]];
        }
        lcd.startWrite();
        lcd.setAddrWindow(sp.outX, sp.outY, sp.outW, sp.outH);
        lcd.writePixels(_frame, sp.outW * sp.outH, true);
        lcd.endWrite();

    } else {
        // ── Масштабирование: ближайший сосед, строка за строкой ───────────
        // Работает как для SCALE_43 (320×224), так и для SCALE_FIT (320×240).
        // При pixel_perfect без DMA-буфера тоже использует этот путь.
        lcd.startWrite();
        lcd.setAddrWindow(sp.outX, sp.outY, sp.outW, sp.outH);
        for (int oy = 0; oy < sp.outH; oy++) {
            // Исходная строка NES (ближайший сосед по Y)
            int sy = (oy * NES_VISIBLE_HEIGHT) / sp.outH;
            const uint8_t *src = bmp->line[sy + VID_FIRST_LINE];
            // Масштаб по X
            for (int ox = 0; ox < sp.outW; ox++) {
                int sx = (ox * NES_SCREEN_WIDTH) / sp.outW;
                _line_buf[ox] = _pal[src[sx]];
            }
            lcd.writePixels(_line_buf, sp.outW, true);
        }
        lcd.endWrite();
    }

    blit_print_stats();
    audio_frame();
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
extern "C" int osd_installtimer(int frequency, void *func, int funcsize,
                                void *counter, int countersize) {
    TimerHandle_t t = xTimerCreate("nes",
        configTICK_RATE_HZ / frequency, pdTRUE, nullptr,
        (TimerCallbackFunction_t)func);
    xTimerStart(t, 0);
    return 0;
}

// ─── input ─────────────────────────────────────────────────────────────────
//
// Физическая разводка Pico → биты пакета (подтверждено диагностикой):
//   бит 0 = START  (BTN_A   = 0x01)
//   бит 1 = SELECT (BTN_B   = 0x02)
//   бит 2 = A      (BTN_SEL = 0x04)
//   бит 3 = B      (BTN_STA = 0x08)
//   бит 4 = UP  / бит 5 = DOWN / бит 6 = LEFT / бит 7 = RIGHT
//
// Комбо выхода: SELECT + START (биты 0+1) удерживать 3 секунды.
//
extern "C" void osd_getinput(void) {
    // Опрашиваем Pico по Serial2 каждый кадр (loop() заблокирован в emu_run).
    buttons.update();

    // Применяем таблицу ремапа: физический бит i → settings.btnMap[i]
    uint8_t pad = buttons.applyBtnMap(buttons.readCurrent());

    // ── Комбо выхода: SELECT + START удерживаются ~3 секунды (180 кадров) ──────
    // Физические SELECT (бит 1 = BTN_B) + START (бит 0 = BTN_A).
    // Первые 90 кадров игнорируем — Pico ещё инициализируется.
    static int frameCount = 0;
    static int exitFrames = 0;
    frameCount++;
    if (frameCount > 90 && (pad & (BTN_A | BTN_B)) == (BTN_A | BTN_B)) {
        if (++exitFrames >= 180) {
            exitFrames = 0;
            frameCount = 0;
            event_t h = event_get(event_quit);
            if (h) h(0);
            return;
        }
    } else {
        exitFrames = 0;
    }

    // ── Передаём изменения состояния кнопок в nofrendo ───────────────────────
    //
    // АППАРАТНАЯ РАЗВОДКА контроллера (подтверждена диагностикой):
    //   бит 0 (физ. STA / кнопка START)  → NES START
    //   бит 1 (физ. SEL / кнопка SELECT) → NES SELECT
    //   бит 2 (физ. A)                   → NES A
    //   бит 3 (физ. B)                   → NES B
    //
    static const int ev[8] = {
        event_joypad1_start,   // бит 0  физ. START  → NES START
        event_joypad1_select,  // бит 1  физ. SELECT → NES SELECT
        event_joypad1_a,       // бит 2  физ. A      → NES A
        event_joypad1_b,       // бит 3  физ. B      → NES B
        event_joypad1_up,      // бит 4  UP
        event_joypad1_down,    // бит 5  DOWN
        event_joypad1_left,    // бит 6  LEFT
        event_joypad1_right,   // бит 7  RIGHT
    };

    static uint8_t prev = 0;
    uint8_t changed = pad ^ prev;
    prev = pad;

    // Диагностика нажатий в эмуляторе
    if (settings.diagButtons && changed) {
        uint8_t raw = buttons.readCurrent();
        printf("[EMU] raw=0x%02X map=0x%02X |", raw, pad);
        if (pad & 0x01) printf(" STA");
        if (pad & 0x02) printf(" SEL");
        if (pad & 0x04) printf(" A");
        if (pad & 0x08) printf(" B");
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
    strncpy(fullname, shortname, PATH_MAX - 1);
    fullname[PATH_MAX - 1] = '\0';
}

extern "C" char *osd_newextension(char *str, char *ext) {
    char *dot = strrchr(str, '.');
    if (dot) strcpy(dot, ext);
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

// Load ROM file from SD into PSRAM. Called by emu_runner before main_loop.
bool osd_rom_load(const char *path) {
    if (_rom_buf) { free(_rom_buf); _rom_buf = nullptr; _rom_size = 0; }
    File f = SD.open(path, FILE_READ);
    if (!f) { printf("[OSD] Cannot open: %s\n", path); return false; }
    _rom_size = f.size();
    _rom_buf  = (uint8_t *)ps_malloc(_rom_size);
    if (!_rom_buf) { f.close(); printf("[OSD] No PSRAM for ROM (%u bytes)\n", (unsigned)_rom_size); return false; }
    f.read(_rom_buf, _rom_size);
    f.close();
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

    // ── RGB565 DMA frame buffer → internal DRAM (SPI DMA requires DRAM) ────
    if (!_frame) {
        size_t needed  = (size_t)NES_SCREEN_WIDTH * NES_VISIBLE_HEIGHT * sizeof(uint16_t);
        size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
        printf("[OSD] DMA heap largest block: %u KB, need: %u KB\n",
               (unsigned)(largest / 1024), (unsigned)(needed / 1024));
        _frame = (uint16_t *)heap_caps_malloc(needed, MALLOC_CAP_DMA);
        if (!_frame) {
            printf("[OSD] WARNING: DMA frame buffer alloc failed — falling back to line-by-line blit\n");
        }
    }
    // ── Заливаем весь экран чёрным ────────────────────────────────────────────
    // Бордюры вокруг игры (32px слева/справа при 1:1, 8px сверху/снизу при 4:3)
    // остаются чёрными на протяжении всей игровой сессии — больше не "вырви глаз".
    lcd.fillScreen(TFT_BLACK);

    return audio_init();
}

extern "C" void osd_shutdown(void) {
    if (_bmp)   { bmp_destroy(&_bmp);    _bmp   = nullptr; }
    if (_frame) { heap_caps_free(_frame); _frame = nullptr; }
    if (_fb)    { free(_fb);              _fb    = nullptr; }
    audio_deinit();
}
