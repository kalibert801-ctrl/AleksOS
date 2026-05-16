#pragma once
// ota_manager.h — OTA firmware update via GitHub Releases

#include <Arduino.h>

struct OTAInfo {
    String latestVersion;   // tag_name from GitHub, e.g. "v14.1"
    String downloadUrl;     // direct URL to firmware.bin asset
    bool   available;       // true if latestVersion != FIRMWARE_VERSION
};

// Query GitHub releases API for the latest version.
// Requires active WiFi connection.
// Returns true on success (even if no update is available).
bool otaCheckUpdate(OTAInfo &out);

// Download and flash the firmware from the given HTTPS URL.
// Shows progress via progressCb(0..100). Reboots on success.
// Returns false if the download/flash failed.
bool otaDownloadAndFlash(const char *url,
                         void (*progressCb)(int pct) = nullptr);
