#include "Widgets.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace nl::ui {

void DrawSpinner(ImDrawList* drawList, ImVec2 center, float radius, float thickness, ImU32 color,
                 float timeSec) {
    constexpr int kSegments = 64;
    constexpr float kRotationSpeed = 17.f;
    constexpr float kFullCircle = 2.f * std::numbers::pi_v<float>;
    constexpr float kFadeLength = 0.75f * kFullCircle;

    const float rotationAngle = timeSec * kRotationSpeed;
    const ImVec4 base = ImGui::ColorConvertU32ToFloat4(color);

    const auto pointAt = [&](float t) {
        const float angle = rotationAngle - t * kFadeLength;
        return ImVec2(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
    };

    ImVec2 previous = pointAt(0.f);
    for (int i = 1; i <= kSegments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSegments);
        const ImVec2 current = pointAt(t);
        drawList->AddLine(previous, current, ColorA(base, base.w * (1.f - t)), thickness);
        previous = current;
    }
}

void DrawScrollBar(ImDrawList* dl, const ScrollState& s, float maxScroll, float contentH,
                   float areaX, float areaY, float areaH, float areaW, ImVec4 accent,
                   float alphaMul) {
    if (!dl || s.scrollbarAlpha < 0.01f || maxScroll <= 0.f || contentH <= 0.f || areaH <= 0.f)
        return;
    constexpr float kWidth = 4.f;
    constexpr float kMargin = 2.f;
    constexpr float kMinThumb = 24.f;
    constexpr float kRounding = 2.f;

    const float thumbH = std::min(std::max(kMinThumb, areaH * (areaH / contentH)), areaH);
    const float ratio = std::clamp(s.current, 0.f, maxScroll) / maxScroll;
    const float x = areaX + areaW - kWidth - kMargin;
    const float y = areaY + ratio * (areaH - thumbH);

    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + kWidth, y + thumbH),
                      ColorA(accent, s.scrollbarAlpha * alphaMul), kRounding);
}

} // namespace nl::ui
