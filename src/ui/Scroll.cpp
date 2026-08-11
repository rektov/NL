#include "Scroll.h"
#include <algorithm>
#include <cmath>

namespace nl::ui {
namespace {

constexpr float kSnapDistance = 0.35f;
constexpr float kSnapVelocity = 8.f;
constexpr float kOvershootVelocity = 40.f;
constexpr float kRestVelocity = 0.01f;
constexpr std::uint64_t kScrollIdleMs = 600;

bool Settled(const ScrollState& s) {
    return std::fabs(s.velocity) < kRestVelocity && std::fabs(s.current - s.target) < kSnapDistance;
}

} // namespace

float SmoothScroll(float current, float target, float& velocity, float dt, float stiffness,
                   float damping) {
    const float diff = target - current;
    if (std::fabs(diff) < kSnapDistance && std::fabs(velocity) < kSnapVelocity) {
        velocity = 0.f;
        return target;
    }

    velocity += (stiffness * diff - damping * velocity) * dt;
    const float next = current + velocity * dt;

    const bool overshot = (diff > 0.f && next > target) || (diff < 0.f && next < target);
    if (overshot && std::fabs(velocity) < kOvershootVelocity) {
        velocity = 0.f;
        return target;
    }
    return next;
}

void TickScroll(ScrollState& s, float maxScroll, float wheelDelta, bool allowWheel, float dt,
                std::uint64_t now, float wheelSpeed) {
    if (allowWheel && wheelDelta != 0.f) {
        s.target -= wheelDelta * wheelSpeed;
        s.isScrolling = true;
        s.lastScrollTime = now;
    } else if (wheelDelta == 0.f) {
        if (s.current < 0.f)
            s.target = 0.f;
        else if (s.current > maxScroll)
            s.target = maxScroll;
        else
            s.target = std::clamp(s.target, 0.f, maxScroll);
    }

    s.current = SmoothScroll(s.current, s.target, s.velocity, dt);
    s.overscroll = 0.f;
    if (s.current < 0.f)
        s.overscroll = s.current;
    else if (s.current > maxScroll)
        s.overscroll = s.current - maxScroll;

    if (Settled(s)) {
        s.current = std::clamp(s.current, 0.f, maxScroll);
        s.overscroll = 0.f;
    }

    if (s.isScrolling && ((now - s.lastScrollTime) > kScrollIdleMs || Settled(s)))
        s.isScrolling = false;

    float targetSb = s.isScrolling ? 1.f : 0.f;
    s.scrollbarAlpha += (targetSb - s.scrollbarAlpha) * 10.f * dt;
}

} // namespace nl::ui
