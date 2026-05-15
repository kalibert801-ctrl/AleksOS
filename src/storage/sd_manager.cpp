#include "storage/sd_manager.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>
#include <algorithm>

static SPIClass hspi(HSPI);
SDManager sdMgr;

bool SDManager::init() {
    hspi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, hspi)) {
        Serial.println("SD: FAILED");
        return false;
    }
    Serial.printf("SD: OK, type=%d\n", SD.cardType());
    scan();
    return true;
}

static void scanDir(std::vector<ROMInfo> &roms, const char *dir) {
    File root = SD.open(dir);
    if (!root || !root.isDirectory()) {
        Serial.printf("SD: '%s' not found\n", dir);
        return;
    }
    Serial.printf("SD: scanning '%s'\n", dir);

    File f = root.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            String n = String(f.name());
            String nl = n; nl.toLowerCase();
            if (nl.endsWith(".nes")) {
                ROMInfo info;
                // Убираем расширение и пробелы
                info.name = n.substring(0, n.length() - 4);
                info.name.trim();
                // Строим путь
                info.path = String(dir) + "/" + n;
                info.size = f.size();
                roms.push_back(info);
            }
        }
        f = root.openNextFile();
    }
    Serial.printf("SD: found %d ROMs in '%s'\n", (int)roms.size(), dir);
}

void SDManager::scan() {
    _roms.clear();

    // Сначала пробуем ROM_DIR (/FomiCon по умолчанию)
    scanDir(_roms, ROM_DIR);

    // Если там пусто — пробуем /roms
    if (_roms.empty()) {
        Serial.println("SD: trying /roms");
        scanDir(_roms, "/roms");
    }

    // Если всё ещё пусто — сканируем корень
    if (_roms.empty()) {
        Serial.println("SD: trying root /");
        scanDir(_roms, "/");
    }

    // Сортировка по имени (без учёта регистра)
    std::sort(_roms.begin(), _roms.end(),
        [](const ROMInfo &a, const ROMInfo &b) {
            String an = a.name; an.toLowerCase();
            String bn = b.name; bn.toLowerCase();
            return an < bn;
        });

    Serial.printf("SD: total %d ROMs\n", (int)_roms.size());
}
