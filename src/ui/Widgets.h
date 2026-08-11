#pragma once
#include "Scroll.h"
#include "imgui.h"
#include <algorithm>

namespace nl::ui {

void DrawSpinner(ImDrawList* drawList, ImVec2 center, float radius, float thickness, ImU32 color,
                 float timeSec);

void DrawScrollBar(ImDrawList* dl, const ScrollState& s, float maxScroll, float contentH,
                   float areaX, float areaY, float areaH, float areaW, ImVec4 accent,
                   float alphaMul = 1.f);

inline void HoverLerp(float& v, bool hovered, float dt, float speed = 10.f) {
    v = std::clamp(hovered ? v + speed * dt : v - speed * dt, 0.f, 1.f);
}

inline ImVec4 Lerp4(const ImVec4& a, const ImVec4& b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                  a.w + (b.w - a.w) * t);
}

inline ImVec4 WithAlpha(const ImVec4& c, float alpha) {
    return ImVec4(c.x, c.y, c.z, alpha);
}

inline ImU32 ColorA(const ImVec4& c, float alpha) {
    return ImGui::GetColorU32(WithAlpha(c, alpha));
}

class ScopedFont {
  public:
    explicit ScopedFont(ImFont* font) : pushed_(font != nullptr) {
        if (pushed_) ImGui::PushFont(font);
    }
    ~ScopedFont() {
        if (pushed_) ImGui::PopFont();
    }
    ScopedFont(const ScopedFont&) = delete;
    ScopedFont& operator=(const ScopedFont&) = delete;
    ScopedFont(ScopedFont&&) = delete;
    ScopedFont& operator=(ScopedFont&&) = delete;

  private:
    bool pushed_;
};

class ScopedTextColor {
  public:
    ScopedTextColor(const ImVec4& c, float alpha) {
        ImGui::PushStyleColor(ImGuiCol_Text, WithAlpha(c, alpha));
    }
    ~ScopedTextColor() { ImGui::PopStyleColor(); }
    ScopedTextColor(const ScopedTextColor&) = delete;
    ScopedTextColor& operator=(const ScopedTextColor&) = delete;
    ScopedTextColor(ScopedTextColor&&) = delete;
    ScopedTextColor& operator=(ScopedTextColor&&) = delete;
};

} // namespace nl::ui
