// ota_manager.cpp — OTA update via GitHub Releases for RetroESP
#include "network/ota_manager.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

// ── GitHub repo info ──────────────────────────────────────────────────────
#define GH_OWNER  "kalibert801-ctrl"
#define GH_REPO   "AleksOS"
#define GH_API    "https://api.github.com/repos/" GH_OWNER "/" GH_REPO "/releases/latest"

// ── Version comparison helper ──────────────────────────────────────────────
// Strips leading "AleksOS BETA " prefix and compares numeric parts.
static bool isNewerVersion(const String &latest) {
    // latest from GitHub tag, e.g. "v14.1"
    // FIRMWARE_VERSION is "AleksOS BETA v14.0"
    int curV = String(FIRMWARE_VERSION).lastIndexOf('v');
    String curNum = (curV >= 0) ? String(FIRMWARE_VERSION).substring(curV + 1) : "0";
    String latNum = latest.startsWith("v") ? latest.substring(1) : latest;

    float cur = curNum.toFloat();
    float lat = latNum.toFloat();
    return lat > cur;
}

bool otaCheckUpdate(OTAInfo &out) {
    out.available     = false;
    out.latestVersion = "";
    out.downloadUrl   = "";
    out.picoUrl       = "";
    out.picoAvailable = false;

    WiFiClientSecure client;
    client.setInsecure();   // skip cert check (acceptable for OTA check)

    HTTPClient http;
    http.begin(client, GH_API);
    http.addHeader("User-Agent", "RetroESP-OTA/1.0");
    http.addHeader("Accept", "application/vnd.github.v3+json");
    http.setTimeout(15000);

    int code = http.GET();
    Serial.printf("[OTA] GitHub API HTTP code: %d\n", code);

    if (code != HTTP_CODE_OK) {
        Serial.printf("[OTA] API error: HTTP %d\n", code);
        http.end();
        return false;
    }

    // ── JSON фильтр — читаем ТОЛЬКО нужные поля ─────────────────────────
    // GitHub API возвращает 15-50KB JSON, из которого нам нужны 3 поля.
    // Фильтрация снижает потребность в RAM с ~32KB до ~1KB.
    StaticJsonDocument<128> filter;
    filter["tag_name"] = true;
    filter["assets"][true]["name"] = true;
    filter["assets"][true]["browser_download_url"] = true;

    // Буфер для отфильтрованных данных: tag + 2 ассета × ~120 байт
    DynamicJsonDocument doc(2048);

    DeserializationError err = deserializeJson(
        doc, http.getStream(),
        DeserializationOption::Filter(filter)
    );
    http.end();

    if (err) {
        Serial.printf("[OTA] JSON parse error: %s\n", err.c_str());
        return false;
    }

    out.latestVersion = doc["tag_name"].as<String>();
    Serial.printf("[OTA] Latest release tag: %s  Current ESP32: %s\n",
                  out.latestVersion.c_str(), FIRMWARE_VERSION);

    // ── Ищем firmware.bin и pico_firmware.bin в ассетах ──────────────────
    JsonArray assets = doc["assets"].as<JsonArray>();
    int assetCount = 0;
    for (JsonObject asset : assets) {
        String name = asset["name"].as<String>();
        String url  = asset["browser_download_url"].as<String>();
        Serial.printf("[OTA] Asset: %s\n", name.c_str());
        assetCount++;
        if (name == "firmware.bin")      out.downloadUrl = url;
        if (name == "pico_firmware.bin") { out.picoUrl = url; out.picoAvailable = true; }
    }
    Serial.printf("[OTA] Total assets found: %d\n", assetCount);

    // Fallback URL для ESP32 если не нашли в ассетах
    if (out.downloadUrl.isEmpty()) {
        out.downloadUrl = String("https://github.com/") + GH_OWNER + "/" + GH_REPO +
                          "/releases/download/" + out.latestVersion + "/firmware.bin";
        Serial.println("[OTA] ESP32 URL not found in assets, using fallback URL");
    }

    out.available = isNewerVersion(out.latestVersion);

    Serial.printf("[OTA] ESP32 update: %s  Pico in release: %s\n",
                  out.available ? "YES" : "no (already latest)",
                  out.picoAvailable ? "YES" : "no");
    return true;
}

bool otaDownloadAndFlash(const char *url, void (*progressCb)(int pct)) {
    WiFiClientSecure client;
    client.setInsecure();

    if (progressCb) {
        httpUpdate.onProgress([progressCb](int cur, int total) {
            if (total > 0) progressCb((int)(cur * 100LL / total));
        });
    }

    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpUpdate.rebootOnUpdate(true);

    Serial.printf("[OTA] Downloading: %s\n", url);
    t_httpUpdate_return ret = httpUpdate.update(client, url);

    switch (ret) {
        case HTTP_UPDATE_OK:
            Serial.println("[OTA] Update OK — rebooting...");
            return true;
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] Update FAILED: %s\n",
                          httpUpdate.getLastErrorString().c_str());
            return false;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] Server says no update available.");
            return false;
    }
    return false;
}
