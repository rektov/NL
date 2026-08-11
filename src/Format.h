#pragma once
#include <ctime>
#include <format>
#include <string>

namespace nl {

constexpr time_t kSecondsPerMinute = 60;
constexpr time_t kSecondsPerHour = 3600;
constexpr time_t kSecondsPerDay = 86'400;

inline bool LocalTime(time_t t, tm& out) {
#ifdef _WIN32
    return localtime_s(&out, &t) == 0;
#else
    return localtime_r(&t, &out) != nullptr;
#endif
}

inline bool SameCalendarDay(const tm& a, const tm& b) {
    return a.tm_year == b.tm_year && a.tm_mon == b.tm_mon && a.tm_mday == b.tm_mday;
}

inline std::string FormatDate(time_t timestamp, time_t now) {
    if (timestamp == 0) return "Never";

    tm nowTm{}, tsTm{};
    if (!LocalTime(now, nowTm) || !LocalTime(timestamp, tsTm)) return "Date formatting error";

    if (SameCalendarDay(nowTm, tsTm)) return "Today";

    tm yTm = nowTm;
    yTm.tm_mday -= 1;
    yTm.tm_isdst = -1;
    if (mktime(&yTm) != static_cast<time_t>(-1) && SameCalendarDay(yTm, tsTm)) return "Yesterday";

    char buffer[80];
    if (strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M", &tsTm) == 0)
        return "Date formatting error";
    return buffer;
}

inline std::string FormatDate(time_t timestamp) {
    return FormatDate(timestamp, time(nullptr));
}

inline std::string FormatValidUntil(bool lifetime, time_t licenseTimestamp) {
    if (lifetime) return "Lifetime";
    if (licenseTimestamp == 0) return "Unknown";
    tm timeinfo{};
    if (!LocalTime(licenseTimestamp, timeinfo)) return "Date formatting error";
    char buffer[80];
    if (strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M", &timeinfo) == 0)
        return "Date formatting error";
    return buffer;
}

inline std::string FormatRelativeTime(time_t timestamp, time_t now) {
    if (timestamp == 0) return "Never";
    long long diff = static_cast<long long>(now) - static_cast<long long>(timestamp);
    if (diff < 0) diff = 0;

    if (diff < kSecondsPerMinute) return "Just now";
    if (diff < kSecondsPerHour) {
        const int m = static_cast<int>(diff / kSecondsPerMinute);
        return std::format("{} minute{} ago", m, m == 1 ? "" : "s");
    }
    if (diff < kSecondsPerDay) {
        const int h = static_cast<int>(diff / kSecondsPerHour);
        return std::format("{} hour{} ago", h, h == 1 ? "" : "s");
    }
    if (diff < kSecondsPerDay * 2) return "Yesterday";
    return FormatDate(timestamp, now);
}

inline std::string FormatRelativeTime(time_t timestamp) {
    return FormatRelativeTime(timestamp, time(nullptr));
}

inline std::string FormatSubscriptionStatus(bool active, bool lifetime, time_t license,
                                            time_t now) {
    if (!active) return "You don't have a subscription";
    if (lifetime) return "Lifetime";

    const time_t rem = license - now;
    if (rem <= 0) return "Expired";
    if (rem >= kSecondsPerDay) {
        const long long d = rem / kSecondsPerDay;
        return std::format("Expires in {} day{}", d, d == 1 ? "" : "s");
    }
    if (rem >= kSecondsPerHour) return std::format("Expires in {}h", rem / kSecondsPerHour);
    if (rem >= kSecondsPerMinute) return std::format("Expires in {}m", rem / kSecondsPerMinute);
    return std::format("Expires in {}s", rem);
}

} // namespace nl
