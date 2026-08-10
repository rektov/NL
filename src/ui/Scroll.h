#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nl::ui {

struct ScrollState {
    float target = 0.f;
    float current = 0.f;
    float velocity = 0.f;
    float overscroll = 0.f;
    float scrollbarAlpha = 0.f;
    bool isScrolling = false;
    std::uint64_t lastScrollTime = 0;

    void Reset() { *this = {}; }
    float ContentOffset() const { return -current + overscroll * 0.5f; }
    bool IsMoving() const {
        return isScrolling || fabsf(velocity) > 0.01f || fabsf(current - target) > 0.35f;
    }
};

float SmoothScroll(float current, float target, float& velocity, float dt, float stiffness = 200.f,
                   float damping = 25.f);

void TickScroll(ScrollState& s, float maxScroll, float wheelDelta, bool allowWheel, float dt,
                std::uint64_t now, float wheelSpeed = 60.f);

}
