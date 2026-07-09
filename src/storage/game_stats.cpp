// game_stats.cpp — хранение игровой статистики: недавние, избранное, время игры.

#include "storage/game_stats.h"
#include <SD.h>
#include <ArduinoJson.h>

#define STATS_FILE    "/game_stats.json"
#define RECENT_MAX    10    // максимум недавних игр

static std::vector<String> _recent;
static std::vector<String> _favs;

// Время игры: хранение в map String→uint32_t реализуем через параллельные векторы
static std::vector<String>   _ptKeys;
static std::vector<uint32_t> _ptVals;

// ─── internal helpers ──────────────────────────────────────────────────────────
static int _ptFind(const char *path) {
    String p(path);
    for (int i = 0; i < (int)_ptKeys.size(); i++)
        if (_ptKeys[i] == p) return i;
    return -1;
}

// ─── Recent ───────────────────────────────────────────────────────────────────
void GameStats::recentAdd(const char *path) {
    String p(path);
    // Удаляем существующую запись (перемещаем вверх)
    for (auto it = _recent.begin(); it != _recent.end(); ++it) {
        if (*it == p) { _recent.erase(it); break; }
    }
    _recent.insert(_recent.begin(), p);
    if ((int)_recent.size() > RECENT_MAX)
        _recent.resize(RECENT_MAX);
}

const std::vector<String>& GameStats::recentList() { return _recent; }

// ─── Favorites ────────────────────────────────────────────────────────────────
void GameStats::favToggle(const char *path) {
    String p(path);
    for (auto it = _favs.begin(); it != _favs.end(); ++it) {
        if (*it == p) { _favs.erase(it); return; }  // убираем если уже в избранном
    }
    _favs.push_back(p);  // добавляем
}

bool GameStats::favCheck(const char *path) {
    String p(path);
    for (const auto &f : _favs) if (f == p) return true;
    return false;
}

const std::vector<String>& GameStats::favList() { return _favs; }

// ─── Play Time ────────────────────────────────────────────────────────────────
void GameStats::playTimeAdd(const char *path, uint32_t seconds) {
    int idx = _ptFind(path);
    if (idx >= 0) {
        _ptVals[idx] += seconds;
    } else {
        _ptKeys.push_back(String(path));
        _ptVals.push_back(seconds);
    }
}

uint32_t GameStats::playTimeGet(const char *path) {
    int idx = _ptFind(path);
    return (idx >= 0) ? _ptVals[idx] : 0;
}

// ─── Persistence ──────────────────────────────────────────────────────────────
void GameStats::load() {
    _recent.clear(); _favs.clear(); _ptKeys.clear(); _ptVals.clear();

    File f = SD.open(STATS_FILE);
    if (!f) return;

    // Файл может быть большим из-за времени игры — используем Dynamic doc
    DynamicJsonDocument doc(8192);  // 4096 могло молча усекать при большом числе ROM
    if (deserializeJson(doc, f)) { f.close(); return; }
    f.close();

    // Recent
    JsonArray ra = doc["recent"].as<JsonArray>();
    for (JsonVariant v : ra) _recent.push_back(v.as<String>());

    // Favs
    JsonArray fa = doc["favs"].as<JsonArray>();
    for (JsonVariant v : fa) _favs.push_back(v.as<String>());

    // Play time
    JsonObject pt = doc["playtime"].as<JsonObject>();
    for (JsonPair kv : pt) {
        _ptKeys.push_back(String(kv.key().c_str()));
        _ptVals.push_back(kv.value().as<uint32_t>());
    }
}

void GameStats::save() {
    DynamicJsonDocument doc(8192);  // 4096 могло молча усекать при большом числе ROM

    JsonArray ra = doc.createNestedArray("recent");
    for (const auto &r : _recent) ra.add(r);

    JsonArray fa = doc.createNestedArray("favs");
    for (const auto &f : _favs) fa.add(f);

    JsonObject pt = doc.createNestedObject("playtime");
    for (int i = 0; i < (int)_ptKeys.size(); i++)
        pt[_ptKeys[i]] = _ptVals[i];

    // Atomic write через temp файл
    const char *tmp = "/game_stats.tmp";
    if (SD.exists(tmp)) SD.remove(tmp);
    File f = SD.open(tmp, FILE_WRITE);
    if (!f) return;
    serializeJson(doc, f);
    f.flush(); f.close();
    SD.remove(STATS_FILE);
    SD.rename(tmp, STATS_FILE);
}
