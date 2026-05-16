#include "storage/config_manager.h"
#include "settings.h"
#include "config.h"
#include <SD.h>
#include <ArduinoJson.h>

void cfgLoad() {
    File f = SD.open(CFG_FILE);
    if (!f) return;
    StaticJsonDocument<1536> doc;
    if (deserializeJson(doc, f)) { f.close(); return; }
    f.close();

    settings.brightness     = doc["brightness"]  | 80;
    settings.volume         = doc["volume"]       | 70;
    settings.emuVolume      = doc["emuvol"]       | 80;
    settings.vibroEnabled   = doc["vibro"]        | 1;
    settings.vibroStrength  = doc["vibrostr"]     | 70;
    settings.theme          = (Theme)   (doc["theme"]   | 0);
    settings.language       = (Language)(doc["lang"]    | 0);
    settings.scale          = (Scale)   (doc["scale"]   | 1);
    settings.showFPS        = doc["fps"]          | false;
    settings.autoSave       = doc["save"]         | true;
    settings.autoBrightness = doc["autobrght"]    | false;
    settings.soundEnabled   = doc["sound"]        | true;
    settings.soundType      = (SoundType)(doc["sndtype"] | 1);
    settings.timeH          = doc["timeh"]        | 12;
    settings.timeM          = doc["timem"]        | 0;
    settings.autoScroll     = doc["autoscroll"]   | 1;
    settings.diagButtons    = doc["diagbtn"]      | 0;
    settings.diagFPS        = doc["diagfps"]      | 0;
    settings.diagEmu        = doc["diagemu"]      | 0;
    settings.diagTouch      = doc["diagtch"]      | 0;

    // ── WiFi ────────────────────────────────────────────────────────────────
    settings.wifiEnabled = doc["wifi_en"] | 0;
    const char *ssid = doc["wifi_ssid"] | "";
    const char *pass = doc["wifi_pass"] | "";
    strncpy(settings.wifiSSID, ssid, sizeof(settings.wifiSSID) - 1);
    settings.wifiSSID[sizeof(settings.wifiSSID) - 1] = '\0';
    strncpy(settings.wifiPass, pass, sizeof(settings.wifiPass) - 1);
    settings.wifiPass[sizeof(settings.wifiPass) - 1] = '\0';

    // Load btnMap only from configs saved by v13.4+ firmware (remap_ver >= 2).
    // remap_ver absent   → very old config (before remap existed)  → skip
    // remap_ver == 1     → v13.3 intermediate build, may have swapped map → skip
    // remap_ver >= 2     → v13.4+, clean mapping saved → load it
    if ((doc["remap_ver"] | 0) >= 2) {
        if (doc.containsKey("btnmap")) {
            JsonArray arr = doc["btnmap"].as<JsonArray>();
            for (int i = 0; i < BTN_MAP_COUNT && i < (int)arr.size(); i++) {
                uint8_t v = arr[i];
                if (v == 0x01 || v == 0x02 || v == 0x04 || v == 0x08 ||
                    v == 0x10 || v == 0x20 || v == 0x40 || v == 0x80 || v == 0x00) {
                    settings.btnMap[i] = v;
                }
            }
        }
    }
    // else: old / intermediate config → keep identity default from settingsDefault()
}

void cfgSave() {
    SD.remove(CFG_FILE);
    File f = SD.open(CFG_FILE, FILE_WRITE);
    if (!f) return;
    StaticJsonDocument<1536> doc;

    doc["brightness"] = settings.brightness;
    doc["volume"]     = settings.volume;
    doc["emuvol"]     = settings.emuVolume;
    doc["vibro"]      = settings.vibroEnabled;
    doc["vibrostr"]   = settings.vibroStrength;
    doc["theme"]      = (int)settings.theme;
    doc["lang"]       = (int)settings.language;
    doc["scale"]      = (int)settings.scale;
    doc["fps"]        = settings.showFPS;
    doc["save"]       = settings.autoSave;
    doc["autobrght"]  = settings.autoBrightness;
    doc["sound"]      = settings.soundEnabled;
    doc["sndtype"]    = (int)settings.soundType;
    doc["timeh"]      = settings.timeH;
    doc["timem"]      = settings.timeM;
    doc["autoscroll"] = settings.autoScroll;
    doc["diagbtn"]    = settings.diagButtons;
    doc["diagfps"]    = settings.diagFPS;
    doc["diagemu"]    = settings.diagEmu;
    doc["diagtch"]    = settings.diagTouch;

    // ── WiFi ───────────────────────────────────────────────────────────────
    doc["wifi_en"]   = settings.wifiEnabled;
    doc["wifi_ssid"] = settings.wifiSSID;
    doc["wifi_pass"] = settings.wifiPass;

    doc["remap_ver"] = 2;   // v13.4+: clean identity-based remap, safe to load
    JsonArray arr = doc.createNestedArray("btnmap");
    for (int i = 0; i < BTN_MAP_COUNT; i++) arr.add(settings.btnMap[i]);

    serializeJson(doc, f);
    f.flush(); f.close();
}
