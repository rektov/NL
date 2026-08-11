#pragma once
#include <format>
#include <string_view>
#include <utility>

namespace nl {

void LogWrite(std::string_view line);

#ifdef _WIN32
void LogHr(std::string_view what, long hr);
#endif

template <class... Args> void Log(std::format_string<Args...> fmt, Args&&... args) {
    LogWrite(std::format(fmt, std::forward<Args>(args)...));
}

} // namespace nl
