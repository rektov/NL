#include <doctest/doctest.h>
#include "Format.h"
#include <ctime>

namespace {

time_t MakeUtc(int year, int month, int day, int hour, int minute, int second = 0) {
    tm t{};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
#ifdef _WIN32
    return _mkgmtime(&t);
#else
    return timegm(&t);
#endif
}

} // namespace

TEST_CASE("FormatDate: sentinel and calendar buckets") {
    const time_t now = MakeUtc(2026, 8, 10, 12, 0);

    CHECK(nl::FormatDate(0, now) == "Never");
    CHECK(nl::FormatDate(now - 3 * nl::kSecondsPerHour, now) == "Today");
    CHECK(nl::FormatDate(MakeUtc(2026, 8, 9, 23, 0), MakeUtc(2026, 8, 10, 1, 0)) == "Yesterday");
    CHECK(nl::FormatDate(MakeUtc(2026, 8, 8, 15, 30), now) == "08.08.2026 15:30");
}

TEST_CASE("FormatDate: yesterday across month and year boundaries") {
    CHECK(nl::FormatDate(MakeUtc(2026, 7, 31, 10, 0), MakeUtc(2026, 8, 1, 10, 0)) == "Yesterday");
    CHECK(nl::FormatDate(MakeUtc(2025, 12, 31, 10, 0), MakeUtc(2026, 1, 1, 10, 0)) == "Yesterday");
}

TEST_CASE("FormatRelativeTime buckets") {
    const time_t now = MakeUtc(2026, 8, 10, 12, 0);

    CHECK(nl::FormatRelativeTime(0, now) == "Never");
    CHECK(nl::FormatRelativeTime(now - 30, now) == "Just now");
    CHECK(nl::FormatRelativeTime(now + 100, now) == "Just now");
    CHECK(nl::FormatRelativeTime(now - 60, now) == "1 minute ago");
    CHECK(nl::FormatRelativeTime(now - 5 * nl::kSecondsPerMinute, now) == "5 minutes ago");
    CHECK(nl::FormatRelativeTime(now - nl::kSecondsPerHour, now) == "1 hour ago");
    CHECK(nl::FormatRelativeTime(now - 3 * nl::kSecondsPerHour, now) == "3 hours ago");
    CHECK(nl::FormatRelativeTime(now - 25 * nl::kSecondsPerHour, now) == "Yesterday");
    CHECK(nl::FormatRelativeTime(MakeUtc(2026, 8, 1, 9, 15), now) == "01.08.2026 09:15");
}

TEST_CASE("FormatValidUntil") {
    CHECK(nl::FormatValidUntil(true, 0) == "Lifetime");
    CHECK(nl::FormatValidUntil(true, 123456) == "Lifetime");
    CHECK(nl::FormatValidUntil(false, 0) == "Unknown");
    CHECK(nl::FormatValidUntil(false, MakeUtc(2026, 12, 31, 23, 59)) == "31.12.2026 23:59");
}

TEST_CASE("FormatSubscriptionStatus") {
    const time_t now = MakeUtc(2026, 8, 10, 12, 0);

    CHECK(nl::FormatSubscriptionStatus(false, false, 0, now) == "You don't have a subscription");
    CHECK(nl::FormatSubscriptionStatus(true, true, 0, now) == "Lifetime");
    CHECK(nl::FormatSubscriptionStatus(true, false, now, now) == "Expired");
    CHECK(nl::FormatSubscriptionStatus(true, false, now - 1, now) == "Expired");
    CHECK(nl::FormatSubscriptionStatus(true, false, now + nl::kSecondsPerDay, now) ==
          "Expires in 1 day");
    CHECK(nl::FormatSubscriptionStatus(true, false, now + 2 * nl::kSecondsPerDay, now) ==
          "Expires in 2 days");
    CHECK(nl::FormatSubscriptionStatus(true, false, now + 3 * nl::kSecondsPerHour, now) ==
          "Expires in 3h");
    CHECK(nl::FormatSubscriptionStatus(true, false, now + 5 * nl::kSecondsPerMinute, now) ==
          "Expires in 5m");
    CHECK(nl::FormatSubscriptionStatus(true, false, now + 30, now) == "Expires in 30s");
}
