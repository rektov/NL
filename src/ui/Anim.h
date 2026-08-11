#pragma once
#include <algorithm>
#include <cmath>

namespace nl::ui {

enum class Phase {
    Splash,
    Expanding,
    Ready,
    Exiting,
};

struct AnimState {
    Phase phase = Phase::Splash;
    float splashT = 0.f;
    float expandT = 0.f;
    float contentAlpha = 0.f;
    float contentT = 0.f;
    float loaderAlpha = 1.f;
    float windowAlpha = 0.f;
    float hideT = 0.f;
    float bootFadeT = 0.f;
    float exitFromAlpha = 1.f;
    int winW = 200;
    int winH = 200;
    int targetW = 500;
    int targetH = 400;
    bool regionDirty = true;
    bool exitDone = false;
};

inline float Clamp01(float t) {
    return t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
}

inline float EaseInOutCubic(float t) {
    t = Clamp01(t);
    return t < 0.5f ? 4.f * t * t * t : 1.f - powf(-2.f * t + 2.f, 3.f) / 2.f;
}

inline float Bezier1D(float t, float a, float b) {
    float u = 1.f - t;
    return 3.f * u * u * t * a + 3.f * u * t * t * b + t * t * t;
}

inline float Bezier1DDeriv(float t, float a, float b) {
    float u = 1.f - t;
    return 3.f * u * u * a + 6.f * u * t * (b - a) + 3.f * t * t * (1.f - b);
}

inline float CubicBezierEase(float x, float x1, float y1, float x2, float y2) {
    x = Clamp01(x);
    if (x <= 0.f) return 0.f;
    if (x >= 1.f) return 1.f;
    float t = x;
    for (int i = 0; i < 8; ++i) {
        float xEst = Bezier1D(t, x1, x2) - x;
        float dx = Bezier1DDeriv(t, x1, x2);
        if (fabsf(dx) < 1e-6f) break;
        t = Clamp01(t - xEst / dx);
    }
    return Bezier1D(t, y1, y2);
}

inline float EaseOutExpand(float t) {
    t = Clamp01(t);
    float e = CubicBezierEase(t, 0.22f, 1.0f, 0.36f, 1.0f);
    if (t <= 0.55f) return e;
    float midT = 0.55f;
    float mid = CubicBezierEase(midT, 0.22f, 1.0f, 0.36f, 1.0f);
    float u = (t - midT) / (1.f - midT);
    u = u * u * (3.f - 2.f * u);
    return mid + (1.f - mid) * u;
}

constexpr float kSplashSec = 2.00f;
constexpr float kExpandSec = 0.32f;
constexpr float kContentFade = 0.40f;
constexpr float kBootFade = 0.20f;
constexpr float kExitFade = 0.28f;
constexpr int kSplashSize = 200;
constexpr int kCornerRadius = 15;

inline void ApplyExpandSize(AnimState& a, float t) {
    int nw = static_cast<int>(
        lroundf(static_cast<float>(kSplashSize) + static_cast<float>(a.targetW - kSplashSize) * t));
    int nh = static_cast<int>(
        lroundf(static_cast<float>(kSplashSize) + static_cast<float>(a.targetH - kSplashSize) * t));
    if (nw != a.winW || nh != a.winH) {
        a.winW = nw;
        a.winH = nh;
        a.regionDirty = true;
    }
}

inline void TickAnim(AnimState& a, float dt) {
    if (a.phase == Phase::Exiting) {
        a.hideT = Clamp01(a.hideT + dt / kExitFade);
        a.windowAlpha = a.exitFromAlpha * (1.f - EaseInOutCubic(a.hideT));
        if (a.hideT >= 1.f) {
            a.windowAlpha = 0.f;
            a.exitDone = true;
        }
        return;
    }

    if (a.phase == Phase::Splash || a.phase == Phase::Expanding) {
        a.bootFadeT = Clamp01(a.bootFadeT + dt / kBootFade);
        a.windowAlpha = EaseInOutCubic(a.bootFadeT);
    } else if (a.phase == Phase::Ready) {
        a.windowAlpha = 1.f;
    }

    if (a.phase == Phase::Splash) {
        a.splashT = Clamp01(a.splashT + dt / kSplashSec);
        a.loaderAlpha = 1.f;
        a.contentAlpha = 0.f;
        a.contentT = 0.f;
        if (a.winW != kSplashSize || a.winH != kSplashSize) {
            a.winW = kSplashSize;
            a.winH = kSplashSize;
            a.regionDirty = true;
        }
        if (a.splashT >= 1.f) {
            a.phase = Phase::Expanding;
            a.expandT = 0.f;
        }
    } else if (a.phase == Phase::Expanding) {
        a.expandT = Clamp01(a.expandT + dt / kExpandSec);
        float t = EaseOutExpand(a.expandT);
        ApplyExpandSize(a, t);
        a.loaderAlpha = 1.f - t;
        if (t > 0.88f) {
            a.contentT = Clamp01((t - 0.88f) / 0.12f) * 0.08f;
            a.contentAlpha = EaseInOutCubic(a.contentT);
        } else {
            a.contentT = 0.f;
            a.contentAlpha = 0.f;
        }
        if (a.expandT >= 1.f) {
            a.winW = a.targetW;
            a.winH = a.targetH;
            a.loaderAlpha = 0.f;
            a.regionDirty = true;
            a.phase = Phase::Ready;
            if (a.contentT < 0.08f) a.contentT = 0.08f;
            a.contentAlpha = EaseInOutCubic(a.contentT);
        }
    } else if (a.phase == Phase::Ready) {
        a.contentT = Clamp01(a.contentT + dt / kContentFade);
        a.contentAlpha = EaseInOutCubic(a.contentT);
        a.loaderAlpha = 0.f;
    }
}

inline void BeginExit(AnimState& a) {
    a.phase = Phase::Exiting;
    a.hideT = 0.f;
    a.exitDone = false;
    a.exitFromAlpha = a.windowAlpha;
}

} // namespace nl::ui
