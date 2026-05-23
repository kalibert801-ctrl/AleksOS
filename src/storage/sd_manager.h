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
    const ROMInfo& get(int i) const { return _roms[i]; }

    // File manager operations
    bool removeROM(int idx);                       // delete file + remove from list
    bool renameROM(int idx, const char *newName);  // rename on SD + update list

private:
    std::vector<ROMInfo> _roms;
};

extern SDManager sdMgr;
