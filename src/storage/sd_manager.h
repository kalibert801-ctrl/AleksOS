#pragma once
#include <Arduino.h>
#include <vector>

struct ROMInfo {
    String name;   // имя без расширения
    String path;   // полный путь /roms/game.nes
    uint32_t size;
};

class SDManager {
public:
    bool init();
    void scan();
    int  count() const { return (int)_roms.size(); }

    // Возвращает ROM по индексу.
    // При выходе за границы (off-by-one, гонка после removeROM) возвращает
    // пустую заглушку вместо UB/crash. API (const ROMInfo&) не изменился.
    const ROMInfo& get(int i) const {
        if (i < 0 || i >= (int)_roms.size()) {
            static const ROMInfo _empty{};
            return _empty;
        }
        return _roms[i];
    }

    // File manager operations
    bool removeROM(int idx);                       // delete file + remove from list
    bool renameROM(int idx, const char *newName);  // rename on SD + update list

private:
    std::vector<ROMInfo> _roms;
};

extern SDManager sdMgr;
