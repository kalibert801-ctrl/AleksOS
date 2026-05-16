#pragma once
// wifi_manager.h — WiFi scan / connect / disconnect helper

#include <Arduino.h>
#include <WiFi.h>

class WiFiManager {
public:
    // ── lifecycle ──────────────────────────────────────────────────────────
    // Call once in setup() — loads saved credentials, auto-connects if enabled.
    void init();

    // ── connection ────────────────────────────────────────────────────────
    bool connect(const char *ssid, const char *pass, uint32_t timeoutMs = 12000);
    void disconnect();
    bool isConnected() const;
    String getSSID() const;
    int   getRSSI()  const;

    // ── scan ──────────────────────────────────────────────────────────────
    // Blocking scan (~2-3 s). Returns number of found networks.
    int  scan();
    int  getScanCount() const { return _scanCount; }
    String getScanSSID(int i) const;
    int    getScanRSSI(int i) const;
    bool   getScanEncrypted(int i) const;

private:
    int _scanCount = 0;
};

extern WiFiManager wifiMgr;
