#pragma once
#include <iterator>
#include <string_view>

namespace nl::ui {

enum class GameArt {
    Cs2,
    Csgo,
    Valorant,
    Apex,
};

struct GameCatalogEntry {
    const char* id;
    const char* fallbackName;
    GameArt art;
};

inline constexpr GameCatalogEntry kGameCatalog[] = {
    {"cs2", "Counter-Strike 2", GameArt::Cs2},
    {"csgo", "Counter-Strike: Global Offensive", GameArt::Csgo},
    {nullptr, "Valorant", GameArt::Valorant},
    {nullptr, "Apex Legends", GameArt::Apex},
};

inline constexpr int kGameCatalogCount = static_cast<int>(std::size(kGameCatalog));

inline GameArt ArtForGameId(std::string_view id) {
    for (const auto& e : kGameCatalog) {
        if (e.id && id == e.id) return e.art;
    }
    return GameArt::Cs2;
}

}
