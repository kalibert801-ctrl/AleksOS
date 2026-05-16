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
    // Extract the version number after the last 'v'
    int curV = String(FIRMWARE_VERSION).lastIndexOf('v');
    String curNum = (curV >= 0) ? String(FIRMWARE_VERSION).substring(curV + 1) : "0";
    String latNum = latest.startsWith("v") ? latest.substring(1) : latest;

    // Simple numeric comparison: parse major.minor
    float cur = curNum.toFloat();
    float lat = latNum.toFloat();
    return lat > cur;
}

bool otaCheckUpdate(OTAInfo &out) {
    out.available = false;
    out.latestVersion = "";
    out.downloadUrl = "";

    WiFiClientSecure client;
    client.setInsecure();   // skip cert check (acceptable for OTA check)

    HTTPClient http;
    http.begin(client, GH_API);
    http.addHeader("User-Agent", "RetroESP-OTA/1.0");
    http.addHeader("Accept", "application/vnd.github.v3+json");
    http.setTimeout(10000);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[OTA] API error: HTTP %d\n", code);
        http.end();
        return false;
    }

    // Parse JSON response
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();

    if (err) {
        Serial.printf("[OTA] JSON parse error: %s\n", err.c_str());
        return false;
    }

    out.latestVersion = doc["tag_name"].as<String>();
    Serial.printf("[OTA] Latest version: %s  Current: %s\n",
                  out.latestVersion.c_str(), FIRMWARE_VERSION);

    // Find the firmware.bin asset
    JsonArray assets = doc["assets"].as<JsonArray>();
    for (JsonObject asset : assets) {
        String name = asset["name"].as<String>();
        if (name == "firmware.bin") {
            out.downloadUrl = asset["browser_download_url"].as<String>();
            break;
        }
    }
    if (out.downloadUrl.isEmpty()) {
        // Fall back to direct URL pattern
        out.downloadUrl = String("https://github.com/") + GH_OWNER + "/" + GH_REPO +
                          "/releases/download/" + out.latestVersion + "/firmware.bin";
    }

    out.available = isNewerVersion(out.latestVersion);
    return true;
}

bool otaDownloadAndFlash(const char *url, void (*progressCb)(int pct)) {
    WiFiClientSecure client;
    client.setInsecure();

    // Report progress via callback
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
            return true;  // won't reach here: rebootOnUpdate=true
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
