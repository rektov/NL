#include <doctest/doctest.h>
#include "ui/Anim.h"
#include "ui/Scroll.h"
#include <cstdint>

using nl::ui::AnimState;
using nl::ui::Phase;
using nl::ui::ScrollState;

namespace {

constexpr float kDt = 1.f / 60.f;

void Settle(ScrollState& s, float maxScroll, float seconds, std::uint64_t& now) {
    const int steps = static_cast<int>(seconds / kDt) + 1;
    for (int i = 0; i < steps; ++i) {
        now += 16;
        nl::ui::TickScroll(s, maxScroll, 0.f, true, kDt, now);
    }
}

} // namespace

TEST_CASE("Clamp01 and easing endpoints") {
    CHECK(nl::ui::Clamp01(-1.f) == 0.f);
    CHECK(nl::ui::Clamp01(0.5f) == 0.5f);
    CHECK(nl::ui::Clamp01(2.f) == 1.f);

    CHECK(nl::ui::EaseInOutCubic(0.f) == doctest::Approx(0.f));
    CHECK(nl::ui::EaseInOutCubic(0.5f) == doctest::Approx(0.5f));
    CHECK(nl::ui::EaseInOutCubic(1.f) == doctest::Approx(1.f));

    CHECK(nl::ui::CubicBezierEase(0.f, 0.22f, 1.f, 0.36f, 1.f) == doctest::Approx(0.f));
    CHECK(nl::ui::CubicBezierEase(1.f, 0.22f, 1.f, 0.36f, 1.f) == doctest::Approx(1.f));

    CHECK(nl::ui::EaseOutExpand(0.f) == doctest::Approx(0.f));
    CHECK(nl::ui::EaseOutExpand(1.f) == doctest::Approx(1.f).epsilon(0.01));
}

TEST_CASE("EaseOutExpand is monotonic and stays in range") {
    float prev = 0.f;
    for (int i = 0; i <= 100; ++i) {
        const float t = static_cast<float>(i) / 100.f;
        const float v = nl::ui::EaseOutExpand(t);
        CHECK(v >= prev - 1e-4f);
        CHECK(v >= -1e-4f);
        CHECK(v <= 1.f + 1e-3f);
        prev = v;
    }
}

TEST_CASE("Boot animation walks Splash -> Expanding -> Ready") {
    AnimState a;
    a.targetW = 500;
    a.targetH = 400;

    CHECK(a.phase == Phase::Splash);
    float elapsed = 0.f;
    while (a.phase == Phase::Splash && elapsed < 10.f) {
        nl::ui::TickAnim(a, kDt);
        elapsed += kDt;
        CHECK(a.windowAlpha >= 0.f);
        CHECK(a.windowAlpha <= 1.f);
    }
    CHECK(a.phase == Phase::Expanding);
    CHECK(elapsed == doctest::Approx(nl::ui::kSplashSec).epsilon(0.05));

    while (a.phase == Phase::Expanding && elapsed < 10.f) {
        nl::ui::TickAnim(a, kDt);
        elapsed += kDt;
        CHECK(a.winW <= a.targetW);
        CHECK(a.winH <= a.targetH);
    }
    CHECK(a.phase == Phase::Ready);
    CHECK(a.winW == a.targetW);
    CHECK(a.winH == a.targetH);
    CHECK(a.loaderAlpha == 0.f);

    for (int i = 0; i < 60; ++i)
        nl::ui::TickAnim(a, kDt);
    CHECK(a.contentAlpha == doctest::Approx(1.f));
    CHECK(a.windowAlpha == doctest::Approx(1.f));
}

TEST_CASE("Exit animation fades out and completes") {
    AnimState a;
    a.phase = Phase::Ready;
    a.windowAlpha = 1.f;

    nl::ui::BeginExit(a);
    CHECK(a.phase == Phase::Exiting);
    CHECK_FALSE(a.exitDone);

    float elapsed = 0.f;
    float prevAlpha = 1.f;
    while (!a.exitDone && elapsed < 5.f) {
        nl::ui::TickAnim(a, kDt);
        elapsed += kDt;
        CHECK(a.windowAlpha <= prevAlpha + 1e-4f);
        prevAlpha = a.windowAlpha;
    }
    CHECK(a.exitDone);
    CHECK(a.windowAlpha == 0.f);
    CHECK(elapsed == doctest::Approx(nl::ui::kExitFade).epsilon(0.15));
}

TEST_CASE("SmoothScroll snaps when close and converges otherwise") {
    float velocity = 0.f;
    CHECK(nl::ui::SmoothScroll(100.f, 100.2f, velocity, kDt) == 100.2f);
    CHECK(velocity == 0.f);

    float current = 0.f;
    velocity = 0.f;
    for (int i = 0; i < 600 && current != 100.f; ++i)
        current = nl::ui::SmoothScroll(current, 100.f, velocity, kDt);
    CHECK(current == doctest::Approx(100.f));
}

TEST_CASE("TickScroll follows the wheel and clamps back into range") {
    ScrollState s;
    std::uint64_t now = 0;
    const float maxScroll = 300.f;

    nl::ui::TickScroll(s, maxScroll, -1.f, true, kDt, now);
    CHECK(s.target == doctest::Approx(60.f));
    CHECK(s.isScrolling);

    Settle(s, maxScroll, 1.5f, now);
    CHECK(s.current == doctest::Approx(60.f).epsilon(0.02));
    CHECK_FALSE(s.isScrolling);
    CHECK(s.overscroll == 0.f);

    for (int i = 0; i < 20; ++i) {
        now += 16;
        nl::ui::TickScroll(s, maxScroll, -1.f, true, kDt, now);
    }
    Settle(s, maxScroll, 2.5f, now);
    CHECK(s.current == doctest::Approx(maxScroll).epsilon(0.02));
    CHECK(s.overscroll == 0.f);

    for (int i = 0; i < 40; ++i) {
        now += 16;
        nl::ui::TickScroll(s, maxScroll, 1.f, true, kDt, now);
    }
    Settle(s, maxScroll, 2.5f, now);
    CHECK(s.current == doctest::Approx(0.f).epsilon(0.02));

    const float before = s.target;
    nl::ui::TickScroll(s, maxScroll, -1.f, false, kDt, now);
    CHECK(s.target == doctest::Approx(before));
}

TEST_CASE("Scrollbar alpha rises while scrolling and fades when idle") {
    ScrollState s;
    std::uint64_t now = 0;
    const float maxScroll = 300.f;

    for (int i = 0; i < 10; ++i) {
        now += 16;
        nl::ui::TickScroll(s, maxScroll, -1.f, true, kDt, now);
    }
    CHECK(s.scrollbarAlpha > 0.5f);

    Settle(s, maxScroll, 3.f, now);
    CHECK(s.scrollbarAlpha < 0.05f);
}

TEST_CASE("ScrollState helpers") {
    ScrollState s;
    CHECK_FALSE(s.IsMoving());
    s.velocity = 5.f;
    CHECK(s.IsMoving());

    s = {};
    s.current = 10.f;
    s.overscroll = 4.f;
    CHECK(s.ContentOffset() == doctest::Approx(-8.f));

    s.Reset();
    CHECK(s.current == 0.f);
    CHECK(s.ContentOffset() == 0.f);
}
