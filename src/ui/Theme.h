#pragma once
#include "imgui.h"

namespace nl::ui {

struct ThemeSpace {
    float padXs = 8.f;
    float padSm = 12.f;
    float padMd = 18.f;
    float padLg = 20.f;
    float gapSm = 8.f;
    float gapMd = 16.f;
    float gapLg = 35.f;
    float cardRounding = 10.f;
    float popupRounding = 5.f;
    float childRounding = 8.f;
    float frameRounding = 6.f;
    float modalRounding = 10.f;
};

struct Theme {
    ImVec4 bg = ImVec4(0.f, 0.01f, 0.01f, 1.f);
    ImVec4 accent = ImVec4(93 / 255.f, 123 / 255.f, 200 / 255.f, 1.f);
    ImVec4 cardBg = ImVec4(0.016f, 0.035f, 0.059f, 1.f);
    ImVec4 cardOutline = ImVec4(0.067f, 0.075f, 0.09f, 1.f);
    ImVec4 text = ImVec4(1.f, 1.f, 1.f, 1.f);
    ImVec4 textMuted = ImVec4(0.6f, 0.6f, 0.6f, 1.f);
    ImVec4 textDim = ImVec4(0.7f, 0.7f, 0.7f, 1.f);
    ImVec4 textSoft = ImVec4(0.85f, 0.85f, 0.85f, 1.f);
    ImVec4 textHover = ImVec4(230 / 255.f, 230 / 255.f, 230 / 255.f, 1.f);
    ImVec4 caption = ImVec4(0.45f, 0.45f, 0.48f, 1.f);
    ImVec4 progressTrack = ImVec4(1.f, 1.f, 1.f, 0.06f);
    ImVec4 progressFill = ImVec4(0.93f, 0.56f, 0.20f, 1.f);
    ImVec4 link = ImVec4(108 / 255.f, 132 / 255.f, 186 / 255.f, 1.f);
    ImVec4 button = ImVec4(72 / 255.f, 118 / 255.f, 214 / 255.f, 1.f);
    ImVec4 branchIcon = ImVec4(0.78f, 0.80f, 0.86f, 1.f);
    ImVec4 branchLabel = ImVec4(0.92f, 0.92f, 0.94f, 1.f);
    ImVec4 popupBorder = ImVec4(0.04f, 0.05f, 0.07f, 1.f);

    ThemeSpace space{};

    ImFont* fontDefault = nullptr;
    ImFont* fontSmall = nullptr;
    ImFont* fontLogo = nullptr;
    ImFont* fontTitle = nullptr;
    ImFont* fontModalTitle = nullptr;

    void ClearFonts() { fontDefault = fontSmall = fontLogo = fontTitle = fontModalTitle = nullptr; }
};

inline void ApplyTheme(ImGuiStyle& style, const Theme& theme) {
    style.WindowRounding = 0.f;
    style.ChildRounding = theme.space.childRounding;
    style.FrameRounding = theme.space.frameRounding;
    style.PopupRounding = theme.space.popupRounding;
    style.ScrollbarRounding = 4.f;
    style.GrabRounding = 4.f;
    style.WindowBorderSize = 0.f;
    style.FrameBorderSize = 0.f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg] = theme.bg;
    c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg] = theme.cardBg;
    c[ImGuiCol_Border] = theme.cardOutline;
    c[ImGuiCol_Text] = theme.text;
    c[ImGuiCol_TextDisabled] = theme.textMuted;
    c[ImGuiCol_Button] = theme.button;
    c[ImGuiCol_ButtonHovered] =
        ImVec4(theme.button.x * 1.1f, theme.button.y * 1.1f, theme.button.z * 1.1f, 1.f);
    c[ImGuiCol_ButtonActive] = theme.accent;
    c[ImGuiCol_FrameBg] = theme.cardBg;
    c[ImGuiCol_FrameBgHovered] = theme.cardOutline;
    c[ImGuiCol_Header] = theme.cardBg;
    c[ImGuiCol_HeaderHovered] = theme.cardOutline;
    c[ImGuiCol_HeaderActive] = theme.accent;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = theme.accent;
    c[ImGuiCol_ScrollbarGrabHovered] = theme.link;
    c[ImGuiCol_ScrollbarGrabActive] = theme.link;
    c[ImGuiCol_CheckMark] = theme.accent;
    c[ImGuiCol_SliderGrab] = theme.accent;
    c[ImGuiCol_SliderGrabActive] = theme.link;
}

} // namespace nl::ui
