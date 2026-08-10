#pragma once
#include <filesystem>
#include <utility>

namespace nl {

inline std::filesystem::path& UserDataDirOverride() {
    static std::filesystem::path testOverride;
    return testOverride;
}

inline void SetUserDataDirOverride(std::filesystem::path path) {
    UserDataDirOverride() = std::move(path);
}

[[nodiscard]] std::filesystem::path UserDataDir();

[[nodiscard]] inline std::filesystem::path UserCheatsPath() {
    return UserDataDir() / "cheats.json";
}

}
