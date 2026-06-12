#pragma once
// game_stats.h — статистика игр: недавние запуски, избранное, время игры.
// Хранится в /game_stats.json на SD карте (отдельно от config.json).
//
// Структура JSON:
// {
//   "recent": ["game1.nes","game2.nes",...],   // до 10 последних
//   "favs":   ["game3.nes"],                    // список избранных
//   "playtime": {"game1.nes": 3600, ...}        // суммарное время в секундах
// }

#include <Arduino.h>
#include <vector>

struct GameStats {
    // Недавно запущенные ROM (пути /FomiCon/name.nes), новые впереди
    static void recentAdd(const char *path);
    static const std::vector<String>& recentList();

    // Избранное
    static void favToggle(const char *path);
    static bool favCheck(const char *path);
    static const std::vector<String>& favList();

    // Время игры
    static void playTimeAdd(const char *path, uint32_t seconds);
    static uint32_t playTimeGet(const char *path);

    // Сохранение/загрузка
    static void load();
    static void save();
};
