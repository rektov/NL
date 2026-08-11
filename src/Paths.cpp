#include "Paths.h"
#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <KnownFolders.h>
#include <ShlObj.h>
#include <objbase.h>
#endif

namespace nl {
namespace {

std::filesystem::path PlatformDataRoot() {
#ifdef _WIN32
    PWSTR raw = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw);
    std::filesystem::path base;
    if (SUCCEEDED(hr) && raw) base = raw;
    if (raw) CoTaskMemFree(raw);
    return base;
#else
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
        return std::filesystem::path(xdg);
    if (const char* home = std::getenv("HOME"); home && *home)
        return std::filesystem::path(home) / ".local" / "share";
    return {};
#endif
}

} // namespace

std::filesystem::path UserDataDir() {
    if (!UserDataDirOverride().empty()) return UserDataDirOverride();

    const std::filesystem::path base = PlatformDataRoot();
    const std::filesystem::path dir =
        base.empty() ? std::filesystem::path("NLLoader") : base / "NLLoader";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

} // namespace nl
