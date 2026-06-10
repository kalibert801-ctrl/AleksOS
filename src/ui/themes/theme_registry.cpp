// theme_registry.cpp — реестр тем AleksOS
// Implements ThemeRegistry + getTheme()
#include "ui/theme_plugin.h"
#include <string.h>

// ── Construct-on-first-use: гарантирует корректную инициализацию
//    до любого статического инициализатора из theme_*.cpp ──────────
struct ThemeStore {
    const ThemePlugin* plugins[ThemeRegistry::MAX];
    int  count;
    int  activeIdx;

    ThemeStore() : count(0), activeIdx(0) {
        for (int i = 0; i < ThemeRegistry::MAX; i++) plugins[i] = nullptr;
    }
};

static ThemeStore& store() {
    static ThemeStore s;
    return s;
}

// ── ThemeRegistry implementation ─────────────────────────────────

void ThemeRegistry::reg(const ThemePlugin* p) {
    ThemeStore& s = store();
    if (!p || s.count >= MAX) return;
    s.plugins[s.count++] = p;
}

int ThemeRegistry::count() {
    return store().count;
}

const ThemePlugin* ThemeRegistry::get(int idx) {
    ThemeStore& s = store();
    if (idx < 0 || idx >= s.count) return nullptr;
    return s.plugins[idx];
}

const ThemePlugin* ThemeRegistry::getByName(const char* name) {
    if (!name) return nullptr;
    ThemeStore& s = store();
    for (int i = 0; i < s.count; i++) {
        if (s.plugins[i] && strcmp(s.plugins[i]->name, name) == 0)
            return s.plugins[i];
    }
    return nullptr;
}

const ThemePlugin* ThemeRegistry::active() {
    ThemeStore& s = store();
    if (s.count == 0) return nullptr;
    return s.plugins[s.activeIdx];
}

int ThemeRegistry::activeIndex() {
    return store().activeIdx;
}

bool ThemeRegistry::setActive(int idx) {
    ThemeStore& s = store();
    if (idx < 0 || idx >= s.count) return false;
    s.activeIdx = idx;
    return true;
}

bool ThemeRegistry::setActiveByName(const char* name) {
    if (!name || name[0] == '\0') return false;
    ThemeStore& s = store();
    for (int i = 0; i < s.count; i++) {
        if (s.plugins[i] && strcmp(s.plugins[i]->name, name) == 0) {
            s.activeIdx = i;
            return true;
        }
    }
    // Тема не найдена — оставляем текущую (или 0)
    return false;
}

const char* ThemeRegistry::activeName() {
    const ThemePlugin* p = active();
    return p ? p->name : "Dark";
}

// ── getTheme() — совместимость с существующим кодом ──────────────
const Theme565& getTheme() {
    const ThemePlugin* p = ThemeRegistry::active();
    if (p) return p->colors;
    // Fallback — тёмная палитра (не должно случаться при нормальной работе)
    static const Theme565 fallback = {
        0x0863, 0x10C4, 0x08A4, 0x0884, 0x1A7B,
        0xE75E, 0x63B1, 0x3DFF, 0xFB8E, 0x4EF0,
        0x0000, 0x0000, 0xFFFF, 0x00, 0
    };
    return fallback;
}
