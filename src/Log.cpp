#include "Log.h"
#include "Paths.h"
#include <cstdio>
#include <memory>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace nl {
namespace {

constexpr std::string_view kPrefix = "[NL] ";

#ifdef _WIN32
std::string SystemMessage(HRESULT hr) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, static_cast<DWORD>(hr), 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (length == 0 || !buffer) {
        if (buffer) LocalFree(buffer);
        return "unknown error";
    }

    std::wstring_view wide(buffer, length);
    while (!wide.empty() && (wide.back() == L'\r' || wide.back() == L'\n'))
        wide.remove_suffix(1);

    std::string narrow;
    narrow.reserve(wide.size());
    for (const wchar_t ch : wide)
        narrow += (ch >= 0x20 && ch < 0x7f) ? static_cast<char>(ch) : '?';

    LocalFree(buffer);
    return narrow;
}
#endif

#if defined(_DEBUG) && defined(_WIN32)
using FilePtr = std::unique_ptr<FILE, decltype(&std::fclose)>;

void AppendDebugFile(std::string_view line) {
    static const FilePtr file = [] {
        const std::filesystem::path path = UserDataDir() / L"nl_debug.log";
        FILE* handle = nullptr;
        if (_wfopen_s(&handle, path.c_str(), L"a") != 0) handle = nullptr;
        return FilePtr(handle, &std::fclose);
    }();
    if (!file) return;
    std::fwrite(line.data(), 1, line.size(), file.get());
    std::fputc('\n', file.get());
    std::fflush(file.get());
}
#endif

} // namespace

void LogWrite(std::string_view line) {
    std::string message;
    message.reserve(kPrefix.size() + line.size() + 1);
    message += kPrefix;
    message += line;
    message += '\n';
#ifdef _WIN32
    OutputDebugStringA(message.c_str());
#else
    std::fputs(message.c_str(), stderr);
#endif
#if defined(_DEBUG) && defined(_WIN32)
    AppendDebugFile(line);
#endif
}

#ifdef _WIN32
void LogHr(std::string_view what, long hr) {
    Log("{} failed: HRESULT=0x{:08X} ({})", what, static_cast<unsigned long>(hr),
        SystemMessage(static_cast<HRESULT>(hr)));
}
#endif

} // namespace nl
