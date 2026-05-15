// osd.cpp — nofrendo OSD implementation for RetroESP / CYD board
// Display: LovyanGFX ST7789 320x240 @ 80MHz SPI DMA
// Audio:   I2S internal DAC, GPIO26 (DAC2 / left channel)
// Input:   Pico UART controller via emu_setController()

#include "display/display_manager.h"
#include "input/button_handler.h"
#include "config.h"
#include "settings.h"
#include "driver/i2s.h"
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

// NES image centered on 320×240 screen
#define VID_X  ((SCREEN_W - NES_SCREEN_WIDTH) / 2)   // 32
#define VID_Y  ((SCREEN_H - NES_VISIBLE_HEIGHT) / 2) //  8

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
    cfg.dma_buf_count     = 4;
    cfg.dma_buf_len       = NES_FRAG_SAMPLES;
    cfg.use_apll          = false;

    if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) return -1;
    i2s_set_pin(I2S_PORT, nullptr);
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN); // GPIO26 = DAC2 = left channel
    return 0;
}

static void audio_deinit(void) {
    i2s_driver_uninstall(I2S_PORT);
    pinMode(AUDIO_PIN, INPUT_PULLDOWN);
}

extern "C" void osd_getsoundinfo(sndinfo_t *info) {
    info->sample_rate = NES_SAMPLE_RATE;
    info->bps         = 16;
}

// ─── video ─────────────────────────────────────────────────────────────────
static uint16_t _pal[256];
// 8-bit indexed NES framebuffer in DRAM (nofrendo renders here, needs 240 lines)
static uint8_t  _fb[NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT];
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
    if (!_bmp)
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
        float fps      = _frameCount * 1000.0f / elapsed;
        uint32_t heap  = ESP.getFreeHeap()  / 1024;
        uint32_t psram = ESP.getFreePsram() / 1024;
        printf("[EMU] FPS=%.1f  heap=%u KB  psram=%u KB\n", fps, heap, psram);
        _frameCount = 0;
        _lastMs     = now;
    }
}

static void drv_custom_blit(bitmap_t *bmp, int nd, rect_t *dr) {
    if (_frame) {
        // Fast path: palette-convert into DMA SRAM buffer, then single DMA burst
        uint16_t *dst = _frame;
        for (int y = 0; y < NES_VISIBLE_HEIGHT; y++) {
            const uint8_t *src = bmp->line[y + VID_FIRST_LINE];
            for (int x = 0; x < NES_SCREEN_WIDTH; x++)
                *dst++ = _pal[src[x]];
        }
        lcd.startWrite();
        lcd.setAddrWindow(VID_X, VID_Y, NES_SCREEN_WIDTH, NES_VISIBLE_HEIGHT);
        lcd.writePixels(_frame, NES_SCREEN_WIDTH * NES_VISIBLE_HEIGHT, true);
        lcd.endWrite();
    } else {
        // Fallback: line-by-line blit using a small stack buffer (~512 B)
        static uint16_t line_buf[NES_SCREEN_WIDTH];
        lcd.startWrite();
        lcd.setAddrWindow(VID_X, VID_Y, NES_SCREEN_WIDTH, NES_VISIBLE_HEIGHT);
        for (int y = 0; y < NES_VISIBLE_HEIGHT; y++) {
            const uint8_t *src = bmp->line[y + VID_FIRST_LINE];
            for (int x = 0; x < NES_SCREEN_WIDTH; x++)
                line_buf[x] = _pal[src[x]];
            lcd.writePixels(line_buf, NES_SCREEN_WIDTH, true);
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
// Bit layout of _pad matches NES pad exactly (same as BTN_* / INP_PAD_* bitmasks)
extern "C" void osd_getinput(void) {
    // Poll Pico controller over Serial2 — loop() is blocked in emu_run(),
    // so we must call buttons.update() here every frame.
    buttons.update();
    _pad = buttons.readCurrent();

    // ── Exit combo: hold SELECT + START for ~120 frames (~2 seconds) ──────────
    // Skip the first 90 frames after start to ignore Pico init noise.
    static int frameCount = 0;
    static int exitFrames = 0;
    frameCount++;
    if (frameCount > 90 && (_pad & (BTN_SEL | BTN_STA)) == (BTN_SEL | BTN_STA)) {
        if (++exitFrames >= 120) {
            exitFrames = 0;
            frameCount = 0;
            // Fire the quit event — causes nes_emulate() to exit cleanly
            event_t h = event_get(event_quit);
            if (h) h(0);
            return;
        }
    } else {
        exitFrames = 0;
    }

    static uint8_t prev = 0;
    uint8_t cur     = _pad;
    uint8_t changed = cur ^ prev;
    prev = cur;

    static const int ev[8] = {
        event_joypad1_a,
        event_joypad1_b,
        event_joypad1_select,
        event_joypad1_start,
        event_joypad1_up,
        event_joypad1_down,
        event_joypad1_left,
        event_joypad1_right,
    };

    for (int i = 0; i < 8; i++) {
        if (!(changed & (1 << i))) continue;
        event_t h = event_get(ev[i]);
        if (h) h((cur & (1 << i)) ? INP_STATE_MAKE : INP_STATE_BREAK);
    }
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
    // Allocate the DMA frame buffer first — before nofrendo and audio claim heap.
    // MALLOC_CAP_DMA guarantees SPI DMA-accessible internal SRAM.
    if (!_frame) {
        size_t needed = NES_SCREEN_WIDTH * NES_VISIBLE_HEIGHT * sizeof(uint16_t);
        size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
        printf("[OSD] DMA heap largest block: %u KB, need: %u KB\n",
               (unsigned)(largest / 1024), (unsigned)(needed / 1024));
        _frame = (uint16_t *)heap_caps_malloc(needed, MALLOC_CAP_DMA);
        if (!_frame) {
            printf("[OSD] WARNING: DMA frame buffer alloc failed — falling back to line-by-line blit\n");
        }
    }
    return audio_init();
}

extern "C" void osd_shutdown(void) {
    if (_bmp) { bmp_destroy(&_bmp); _bmp = nullptr; }
    if (_frame) { heap_caps_free(_frame); _frame = nullptr; }
    audio_deinit();
}
