#include "UiState.h"
#include "UiGpuResources.h"
#include "GameCatalog.h"
#include "Widgets.h"
#include "Anim.h"
#include "../Format.h"
#include "imgui.h"
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <sstream>

namespace nl::ui {
namespace {

void DrawGitBranchIcon(ImDrawList* drawList, ImVec2 center, float size, ImU32 color) {
    const float radius = size * 0.13f;
    const float thickness = 1.25f;
    ImVec2 bot(center.x - size * 0.08f, center.y + size * 0.34f);
    ImVec2 top(center.x - size * 0.08f, center.y - size * 0.34f);
    ImVec2 fork(center.x - size * 0.08f, center.y + size * 0.02f);
    ImVec2 side(center.x + size * 0.34f, center.y - size * 0.26f);
    drawList->AddLine(bot, top, color, thickness);
    ImVec2 elbow(center.x + size * 0.10f, center.y - size * 0.08f);
    drawList->AddLine(fork, elbow, color, thickness);
    drawList->AddLine(elbow, side, color, thickness);
    drawList->AddCircleFilled(bot, radius, color, 16);
    drawList->AddCircleFilled(top, radius, color, 16);
    drawList->AddCircleFilled(side, radius, color, 16);
}

void RequestModalClose(UiState& ui, std::uint64_t now) {
    if (ui.inject.active || ui.modal.closing) return;
    ui.modal.branchMenu = false;
    ui.modal.branchMenuT = 0.f;
    ui.modal.closing = true;
    ui.modal.closeTime = now;
}

void BeginBranchContentSwap(UiState& ui) {
    ui.modal.contentFadingOut = true;
    ui.modal.contentFadingIn = false;
    ui.modal.contentSwapPending = true;
}

void TickBranchContentFade(UiState& ui, const data::CheatInfo& info, float dt, time_t now) {
    constexpr float kSpeed = 6.5f;
    if (ui.modal.contentFadingOut) {
        ui.modal.contentFade = std::max(0.f, ui.modal.contentFade - kSpeed * dt);
        if (ui.modal.contentFade <= 0.001f) {
            ui.modal.contentFade = 0.f;
            ui.modal.contentFadingOut = false;
            if (ui.modal.contentSwapPending) {
                FillDisplayedBranch(ui, info, now);
                ui.modal.contentSwapPending = false;
                ui.textScroll.Reset();
            }
            ui.modal.contentFadingIn = true;
        }
    } else if (ui.modal.contentFadingIn) {
        ui.modal.contentFade = std::min(1.f, ui.modal.contentFade + kSpeed * dt);
        if (ui.modal.contentFade >= 0.999f) {
            ui.modal.contentFade = 1.f;
            ui.modal.contentFadingIn = false;
        }
    }
}

void TickInject(UiState& ui, std::uint64_t now) {
    if (!ui.inject.active) return;
    const std::uint64_t elapsed = now - ui.inject.startTime;

    constexpr std::uint64_t kFadeOutMs = 280;
    constexpr std::uint64_t kShrinkMs = 320;
    constexpr std::uint64_t kFadeInMs = 360;
    constexpr std::uint64_t kHoldMs = 2200;
    constexpr std::uint64_t kProgressMs = 1800;

    const std::uint64_t fadeOutEnd = kFadeOutMs;
    const std::uint64_t shrinkEnd = fadeOutEnd + kShrinkMs;
    const std::uint64_t injectEnd = shrinkEnd + kFadeInMs + kHoldMs;

    if (elapsed < fadeOutEnd) {
        ui.inject.showContent = false;
        ui.inject.modalW = kModalW;
        ui.inject.modalH = kModalH;
        ui.inject.logoAlpha = 0.f;
        ui.inject.progress = 0.f;
        const float t =
            EaseInOutCubic(static_cast<float>(elapsed) / static_cast<float>(kFadeOutMs));
        ui.inject.contentAlpha = 1.f - t;
    } else if (elapsed < shrinkEnd) {
        ui.inject.showContent = false;
        ui.inject.contentAlpha = 0.f;
        ui.inject.logoAlpha = 0.f;
        const float t =
            EaseOutExpand(static_cast<float>(elapsed - fadeOutEnd) / static_cast<float>(kShrinkMs));
        ui.inject.modalW = kModalW + (kInjectW - kModalW) * t;
        ui.inject.modalH = kModalH + (kInjectH - kModalH) * t;
    } else if (elapsed < injectEnd) {
        ui.inject.showContent = true;
        ui.inject.modalW = kInjectW;
        ui.inject.modalH = kInjectH;
        const float local = static_cast<float>(elapsed - shrinkEnd);
        ui.inject.logoAlpha = EaseInOutCubic(std::min(1.f, local / static_cast<float>(kFadeInMs)));
        ui.inject.contentAlpha = 0.f;
        ui.inject.progress = EaseOutExpand(std::min(1.f, local / static_cast<float>(kProgressMs)));
    } else {
        ui.requestExit = true;
        ui.inject.active = false;
    }
}

struct BranchPopupLayout {
    bool wantDraw = false;
    ImVec2 pos{};
    float w = 0.f;
    float h = 0.f;
    float alpha = 0.f;
    float rounding = 5.f;
    ImVec2 branchBtnMin{};
    ImVec2 branchBtnMax{};
};

void DrawModalFrame(ImDrawList* drawList, Theme& theme, const ImVec2& windowPos, float modalW,
                    float modalH, float alpha) {
    ImGui::PushClipRect(ImVec2(windowPos.x - 2.f, windowPos.y - 2.f),
                        ImVec2(windowPos.x + modalW + 2.f, windowPos.y + modalH + 2.f), false);
    drawList->AddRectFilled(
        windowPos, ImVec2(windowPos.x + modalW, windowPos.y + modalH),
        ImGui::GetColorU32(ImVec4(theme.cardBg.x, theme.cardBg.y, theme.cardBg.z, alpha)),
        theme.space.modalRounding);
    drawList->AddRect(windowPos, ImVec2(windowPos.x + modalW, windowPos.y + modalH),
                      ImGui::GetColorU32(ImVec4(theme.cardOutline.x, theme.cardOutline.y,
                                                theme.cardOutline.z, alpha)),
                      theme.space.modalRounding, 0, 1.5f);
    ImGui::PopClipRect();
}

void DrawMetaColumns(Theme& theme, UiState& ui, const ImVec2& windowPos, float modalW, float modalH,
                     float contentAlpha, float fadeA, float fadeY, const FrameInput& frame) {
    const float leftPad = 20.f;
    const float labelGap = 6.f;
    float baseY = 100.f;
    float lineSp = ImGui::GetTextLineHeight() + 8.f;

    struct Line {
        const char* label;
        const char* value;
        float y;
    };
    Line left[] = {
        {"Branch:", ui.modal.displayedBranch.c_str(), baseY + fadeY},
        {"Updated:", ui.modal.displayedUpdated.c_str(), baseY + lineSp + fadeY},
        {"Valid Until:", ui.modal.displayedValid.c_str(), baseY + 2 * lineSp + fadeY},
        {"Last Launch:", ui.modal.displayedLastLaunch.c_str(), baseY + 3 * lineSp + fadeY},
    };
    float leftColW = 0.f;
    for (auto& line : left) {
        float w = ImGui::CalcTextSize(line.label).x + labelGap + ImGui::CalcTextSize(line.value).x;
        leftColW = std::max(leftColW, w);
    }
    const float colGap = 28.f;
    const float rightPad = 18.f;
    const float minChangelogW = 155.f;
    float changelogX = leftPad + leftColW + colGap;
    float textAreaW = modalW - changelogX - rightPad;
    if (textAreaW < minChangelogW) {
        textAreaW = minChangelogW;
        changelogX = modalW - rightPad - textAreaW;
    }
    float leftClipR = changelogX - 10.f;

    ImGui::PushClipRect(ImVec2(windowPos.x + leftPad - 2.f, windowPos.y + baseY - 2.f),
                        ImVec2(windowPos.x + leftClipR, windowPos.y + modalH - 70.f), true);
    for (auto& line : left) {
        ImGui::SetCursorPos(ImVec2(leftPad, line.y));
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(theme.textSoft.x, theme.textSoft.y, theme.textSoft.z, fadeA));
        ImGui::Text("%s", line.label);
        float labelW = ImGui::CalcTextSize(line.label).x;
        ImGui::PopStyleColor();
        ImGui::SetCursorPos(ImVec2(leftPad + labelW + labelGap, line.y));
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(theme.link.x, theme.link.y, theme.link.z, fadeA));
        ImGui::Text("%s", line.value);
        ImGui::PopStyleColor();
    }
    ImGui::PopClipRect();

    float textAreaH = modalH - baseY - 80.f;
    const float textScrollSpeed = 60.f;
    ImVec2 textAreaScreen(windowPos.x + changelogX, windowPos.y + baseY);

    ImGui::SetCursorPos(ImVec2(changelogX, baseY));
    ImGui::BeginChild("##changelog", ImVec2(textAreaW, textAreaH), false,
                      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    ImGui::SetScrollY(0.f);

    float contentOriginY = ui.textScroll.ContentOffset() + fadeY;
    ImGui::SetCursorPosY(contentOriginY);

    ImGui::PushTextWrapPos(textAreaW - 10.f);
    for (size_t bi = 0; bi < ui.modal.displayedChangelogBlocks.size(); ++bi) {
        const auto& block = ui.modal.displayedChangelogBlocks[bi];
        if (bi > 0) ImGui::Dummy(ImVec2(0.f, 10.f));
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(theme.text.x, theme.text.y, theme.text.z, fadeA));
        ImGui::Text("%s", block.date.c_str());
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(theme.textDim.x, theme.textDim.y, theme.textDim.z, fadeA));
        for (const auto& line : block.lines)
            ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopTextWrapPos();

    float contentH = ImGui::GetCursorPosY() - contentOriginY;
    float maxScrollText = std::max(0.f, contentH - textAreaH);

    bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    TickScroll(ui.textScroll, maxScrollText, ImGui::GetIO().MouseWheel,
               hovered && !ui.inject.active, frame.dt, frame.nowMs, textScrollSpeed);

    ImGui::EndChild();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    DrawScrollBar(drawList, ui.textScroll, maxScrollText, contentH, textAreaScreen.x,
                  textAreaScreen.y, textAreaH, textAreaW, theme.accent,
                  ui.modal.alpha * contentAlpha);
}

void DrawFooterLinks(Theme& theme, UiState& ui, const ImVec2& windowPos, ImVec2 loadPos, float btnH,
                     float contentAlpha, float dt, ModalActions& out) {
    const char* community = "Community";
    const char* api = "API Documentation";
    ImVec2 communitySz = ImGui::CalcTextSize(community);
    ImVec2 apiSz = ImGui::CalcTextSize(api);
    float textY = loadPos.y + btnH / 2.f - communitySz.y / 2.f;
    const float leftPad = 20.f;
    ImGui::SetCursorPos(ImVec2(leftPad, textY - windowPos.y));
    ImGui::InvisibleButton("##community", communitySz);
    bool communityHov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked() && !ui.inject.active) out.openUrl = L"https://forum.neverlose.cc/";
    ImGui::SetCursorPos(ImVec2(leftPad + communitySz.x + 15.f, textY - windowPos.y));
    ImGui::InvisibleButton("##api", apiSz);
    bool apiHov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked() && !ui.inject.active)
        out.openUrl = L"https://docs-csgo.neverlose.cc/";
    HoverLerp(ui.communityHover, communityHov, dt);
    HoverLerp(ui.apiHover, apiHov, dt);
    ImVec4 communityCol = Lerp4(
        ImVec4(theme.textDim.x, theme.textDim.y, theme.textDim.z, ui.modal.alpha * contentAlpha),
        ImVec4(theme.textHover.x, theme.textHover.y, theme.textHover.z,
               ui.modal.alpha * contentAlpha),
        ui.communityHover);
    ImVec4 apiCol = Lerp4(
        ImVec4(theme.textDim.x, theme.textDim.y, theme.textDim.z, ui.modal.alpha * contentAlpha),
        ImVec4(theme.textHover.x, theme.textHover.y, theme.textHover.z,
               ui.modal.alpha * contentAlpha),
        ui.apiHover);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(ImVec2(windowPos.x + leftPad, textY), ImGui::GetColorU32(communityCol),
                      community);
    drawList->AddText(ImVec2(windowPos.x + leftPad + communitySz.x + 15.f, textY),
                      ImGui::GetColorU32(apiCol), api);
}

void DrawInjectProgress(Theme& theme, ImDrawList* drawList, ID3D11ShaderResourceView* tex,
                        const ImVec2& windowPos, float modalW, float modalH, float alpha,
                        float progress) {
    const char* startText = "The game will start automatically";
    ImFont* captionFont = theme.fontSmall ? theme.fontSmall : theme.fontDefault;
    if (!tex || !captionFont) return;

    const float logoSz = 44.f;
    ImGui::PushFont(captionFont);
    ImVec2 textSz = ImGui::CalcTextSize(startText);
    ImGui::PopFont();

    const float barH = 3.f;
    const float barW = textSz.x * 0.72f;
    const float gapLogoBar = 14.f;
    const float gapBarText = 12.f;
    float totalH = logoSz + gapLogoBar + barH + gapBarText + textSz.y;
    float groupY = windowPos.y + (modalH - totalH) * 0.5f;

    ImVec2 logoPos(windowPos.x + (modalW - logoSz) * 0.5f, groupY);
    drawList->AddImageRounded(reinterpret_cast<ImTextureID>(tex), logoPos,
                              ImVec2(logoPos.x + logoSz, logoPos.y + logoSz), ImVec2(0, 0),
                              ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, alpha)),
                              theme.space.modalRounding);

    float barX = windowPos.x + (modalW - barW) * 0.5f;
    float barY = groupY + logoSz + gapLogoBar;
    drawList->AddRectFilled(
        ImVec2(barX, barY), ImVec2(barX + barW, barY + barH),
        ImGui::GetColorU32(ImVec4(theme.progressTrack.x, theme.progressTrack.y,
                                  theme.progressTrack.z, theme.progressTrack.w * alpha)),
        barH * 0.5f);

    float fillW = barW * progress;
    if (fillW > 0.5f) {
        drawList->AddRectFilled(
            ImVec2(barX, barY), ImVec2(barX + fillW, barY + barH),
            ImGui::GetColorU32(
                ImVec4(theme.progressFill.x, theme.progressFill.y, theme.progressFill.z, alpha)),
            barH * 0.5f);
    }

    float textX = windowPos.x + (modalW - textSz.x) * 0.5f;
    float textY = barY + barH + gapBarText;
    drawList->AddText(
        captionFont, captionFont->FontSize, ImVec2(textX, textY),
        ImGui::GetColorU32(ImVec4(theme.caption.x, theme.caption.y, theme.caption.z, alpha)),
        startText);
}

void DrawBranchPopup(Theme& theme, UiState& ui, data::CheatInfo& info,
                     const BranchPopupLayout& layout, const ImVec2& modalPos, float modalW,
                     float modalH, float dt, std::uint64_t now, ModalActions& out) {
    if (!layout.wantDraw) return;
    constexpr float itemH = 30.f;
    constexpr float padX = 10.f;
    constexpr float iconSize = 14.f;
    ImDrawList* fgDrawList = ImGui::GetForegroundDrawList();
    ImVec2 wp = layout.pos;
    const bool interactive = ui.modal.branchMenu && ui.modal.branchMenuT > 0.55f;

    ImVec4 popBg(theme.cardBg.x * 0.55f, theme.cardBg.y * 0.55f, theme.cardBg.z * 0.55f,
                 0.98f * layout.alpha);
    ImVec4 popBd(theme.popupBorder.x, theme.popupBorder.y, theme.popupBorder.z,
                 0.8f * layout.alpha);

    fgDrawList->AddRectFilled(wp, ImVec2(wp.x + layout.w, wp.y + layout.h),
                              ImGui::GetColorU32(popBg), layout.rounding);
    fgDrawList->AddRect(wp, ImVec2(wp.x + layout.w, wp.y + layout.h), ImGui::GetColorU32(popBd),
                        layout.rounding, 0, 1.f);

    ImVec2 mousePos = ImGui::GetIO().MousePos;
    const bool mouseInPop = mousePos.x >= wp.x && mousePos.x <= wp.x + layout.w &&
                            mousePos.y >= wp.y && mousePos.y <= wp.y + layout.h;
    const bool mouseInBtn =
        mousePos.x >= layout.branchBtnMin.x && mousePos.x <= layout.branchBtnMax.x &&
        mousePos.y >= layout.branchBtnMin.y && mousePos.y <= layout.branchBtnMax.y;
    bool clicked = interactive && ImGui::IsMouseClicked(0);
    bool picked = false;

    constexpr size_t kHoverCap = static_cast<size_t>(kMaxBranchHover);
    for (size_t bi = 0; bi < info.branches.size(); ++bi) {
        const auto& b = info.branches[bi];
        ImVec2 r0(wp.x + 3.f, wp.y + 6.f + itemH * static_cast<float>(bi));
        ImVec2 r1(wp.x + layout.w - 3.f, r0.y + itemH);
        bool hov = interactive && mousePos.x >= r0.x && mousePos.x <= r1.x && mousePos.y >= r0.y &&
                   mousePos.y <= r1.y;
        const std::string& effectiveSel =
            (out.persistSelectedBranch && !out.selectedBranchId.empty()) ? out.selectedBranchId
                                                                         : info.selectedBranch;
        bool sel = (b.id == effectiveSel);

        if (bi < kHoverCap) {
            float& hv = ui.modal.branchItemHover[bi];
            float hoverTarget = interactive && (sel || hov) ? 1.f : 0.f;
            hv += (hoverTarget - hv) * std::min(1.f, 14.f * dt);
            if (hv > 0.01f) {
                fgDrawList->AddRectFilled(
                    r0, r1, ImGui::GetColorU32(ImVec4(1, 1, 1, 0.08f * hv * layout.alpha)), 4.f);
            }
        }

        if (clicked && hov && !ui.inject.active) {
            const bool changed = (b.id != info.selectedBranch);
            out.persistSelectedBranch = true;
            out.selectedCheatId = ui.modal.gameId;
            out.selectedBranchId = b.id;
            if (changed) BeginBranchContentSwap(ui);
            ui.modal.branchMenu = false;
            ui.modal.loadBlockUntil = now + kBranchLoadBlockMs;
            picked = true;
        }

        ImU32 iconColor = ImGui::GetColorU32(
            ImVec4(theme.branchIcon.x, theme.branchIcon.y, theme.branchIcon.z, layout.alpha));
        DrawGitBranchIcon(fgDrawList,
                          ImVec2(r0.x + padX + iconSize * 0.5f - 3.f, r0.y + itemH * 0.5f),
                          iconSize, iconColor);
        ImVec2 textPos(r0.x + padX + iconSize + 2.f,
                       r0.y + (itemH - ImGui::GetTextLineHeight()) * 0.5f);
        fgDrawList->AddText(textPos,
                            ImGui::GetColorU32(ImVec4(theme.branchLabel.x, theme.branchLabel.y,
                                                      theme.branchLabel.z, layout.alpha)),
                            b.name.c_str());
    }

    if (interactive && clicked && !picked && !mouseInPop && !mouseInBtn) {
        if (mousePos.x >= modalPos.x && mousePos.x <= modalPos.x + modalW &&
            mousePos.y >= modalPos.y && mousePos.y <= modalPos.y + modalH)
            ui.modal.branchMenu = false;
    }
}

BranchPopupLayout DrawDetailBody(Theme& theme, UiState& ui, data::CheatInfo& info,
                                 const UiGpuResources& gpu, const ImVec2& windowPos, float modalW,
                                 float modalH, float contentAlpha, const FrameInput& frame,
                                 ModalActions& out) {
    const float dt = frame.dt;
    const std::uint64_t now = frame.nowMs;
    BranchPopupLayout popup{};
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ID3D11ShaderResourceView* tex = gpu.ArtFor(ArtForGameId(ui.modal.gameId));

    if (tex) {
        drawList->AddImageRounded(
            reinterpret_cast<ImTextureID>(tex), ImVec2(windowPos.x + 20.f, windowPos.y + 20.f),
            ImVec2(windowPos.x + 60.f, windowPos.y + 60.f), ImVec2(0, 0), ImVec2(1, 1),
            ImGui::GetColorU32(ImVec4(1, 1, 1, ui.modal.alpha * contentAlpha)), 10.f);
    }

    ImGui::SetCursorPos(ImVec2(70.f, 27.f));
    {
        const ScopedFont titleFont(theme.fontModalTitle);
        const ScopedTextColor titleColor(theme.text, ui.modal.alpha * contentAlpha);
        ImGui::TextUnformatted(info.name.c_str());
    }

    const ScopedFont bodyFont(theme.fontDefault);
    const float cf = ui.modal.contentFade;
    const float fadeA = ui.modal.alpha * contentAlpha * cf;
    const float fadeY = (1.f - cf) * 8.f;
    DrawMetaColumns(theme, ui, windowPos, modalW, modalH, contentAlpha, fadeA, fadeY, frame);

    const float btnH = 30.f;
    const float loadW = 100.f;
    const float branchSz = 30.f;
    const float btnGap = 8.f;
    const float btnRounding = theme.space.modalRounding * 0.5f;
    ImVec2 loadPos(windowPos.x + modalW - 20.f - loadW, windowPos.y + modalH - 20.f - btnH);
    ImVec2 branchPos(loadPos.x - btnGap - branchSz, loadPos.y);

    float btnAlpha = ui.modal.alpha * contentAlpha;
    popup.branchBtnMin = branchPos;
    popup.branchBtnMax = ImVec2(branchPos.x + branchSz, branchPos.y + branchSz);

    ImGui::SetCursorPos(ImVec2(branchPos.x - windowPos.x, branchPos.y - windowPos.y));
    ImGui::InvisibleButton("##branchBtn", ImVec2(branchSz, branchSz));
    bool branchHov = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked() && !ui.inject.active && !ui.modal.closing)
        ui.modal.branchMenu = !ui.modal.branchMenu;

    {
        constexpr float kBranchAnimSec = 0.20f;
        const float step = dt / kBranchAnimSec;
        if (ui.modal.branchMenu)
            ui.modal.branchMenuT = std::min(1.f, ui.modal.branchMenuT + step);
        else
            ui.modal.branchMenuT = std::max(0.f, ui.modal.branchMenuT - step);
    }
    float menuT = EaseInOutCubic(ui.modal.branchMenuT);
    const bool loadBlocked = menuT > 0.001f || now < ui.modal.loadBlockUntil;

    ImU32 branchBg =
        ImGui::GetColorU32(ImVec4(theme.button.x, theme.button.y, theme.button.z,
                                  (branchHov || ui.modal.branchMenu ? 1.f : 0.92f) * btnAlpha));
    drawList->AddRectFilled(branchPos, ImVec2(branchPos.x + branchSz, branchPos.y + branchSz),
                            branchBg, btnRounding);
    {
        ImVec2 center(branchPos.x + branchSz * 0.5f, branchPos.y + branchSz * 0.5f + 0.5f);
        const float aw = 4.2f, ah = 2.4f;
        ImU32 chevron =
            ImGui::GetColorU32(ImVec4(theme.text.x, theme.text.y, theme.text.z, btnAlpha));
        drawList->PathClear();
        drawList->PathLineTo(ImVec2(center.x - aw, center.y - ah * 0.35f));
        drawList->PathLineTo(ImVec2(center.x, center.y + ah));
        drawList->PathLineTo(ImVec2(center.x + aw, center.y - ah * 0.35f));
        drawList->PathStroke(chevron, ImDrawFlags_None, 1.4f);
    }

    const bool loadInteractive = !loadBlocked && !ui.inject.active && !ui.modal.closing;
    ImGui::SetCursorPos(ImVec2(loadPos.x - windowPos.x, loadPos.y - windowPos.y));
    if (loadInteractive) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, btnRounding);
        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImVec4(theme.button.x, theme.button.y, theme.button.z, btnAlpha));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(theme.button.x, theme.button.y,
                                                             theme.button.z, 0.9f * btnAlpha));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(theme.button.x, theme.button.y,
                                                            theme.button.z, 0.8f * btnAlpha));
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(theme.text.x, theme.text.y, theme.text.z, btnAlpha));
        if (ImGui::Button("Load", ImVec2(loadW, btnH))) {
            ui.modal.branchMenu = false;
            ui.modal.branchMenuT = 0.f;
            ui.inject = {};
            ui.inject.active = true;
            ui.inject.startTime = now;
            ui.inject.modalW = kModalW;
            ui.inject.modalH = kModalH;
            ui.inject.contentAlpha = 1.f;
            if (auto* br = info.ActiveBranch()) {
                out.persistLastLaunch = true;
                out.lastLaunchCheatId = ui.modal.gameId;
                out.lastLaunchBranchId = br->id;
                out.lastLaunchWhen = frame.nowUnix;
                ui.modal.displayedLastLaunch =
                    FormatRelativeTime(out.lastLaunchWhen, frame.nowUnix);
            }
        }
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar();
    } else {
        drawList->AddRectFilled(
            loadPos, ImVec2(loadPos.x + loadW, loadPos.y + btnH),
            ImGui::GetColorU32(ImVec4(theme.button.x, theme.button.y, theme.button.z, btnAlpha)),
            btnRounding);
        ImVec2 loadTextSz = ImGui::CalcTextSize("Load");
        drawList->AddText(
            ImVec2(loadPos.x + (loadW - loadTextSz.x) * 0.5f,
                   loadPos.y + (btnH - loadTextSz.y) * 0.5f),
            ImGui::GetColorU32(ImVec4(theme.text.x, theme.text.y, theme.text.z, btnAlpha)), "Load");
        ImGui::InvisibleButton("##loadDead", ImVec2(loadW, btnH));
    }

    if (menuT > 0.001f && !info.branches.empty()) {
        const float itemH = 30.f;
        const float padX = 10.f;
        const float iconSize = 14.f;
        float maxLabelW = 0.f;
        for (const auto& b : info.branches)
            maxLabelW = std::max(maxLabelW, ImGui::CalcTextSize(b.name.c_str()).x);
        float contentW = padX + iconSize + 8.f + maxLabelW + padX;
        float menuContentH = 6.f + itemH * static_cast<float>(info.branches.size()) + 6.f;

        float popW = std::max(contentW, loadW + 8.f);
        float popH = std::max(menuContentH, btnH + 8.f);
        float popBottom = loadPos.y + btnH + 2.f;
        float popRight = loadPos.x + loadW + 2.f;
        const float slide = (1.f - menuT) * 10.f;
        ImVec2 pop(popRight - popW, popBottom - popH + slide);

        popup.wantDraw = true;
        popup.pos = pop;
        popup.w = popW;
        popup.h = popH;
        popup.alpha = btnAlpha * menuT;
        popup.rounding = btnRounding;
    } else {
        for (float& hv : ui.modal.branchItemHover)
            hv = 0.f;
    }

    DrawFooterLinks(theme, ui, windowPos, loadPos, btnH, contentAlpha, dt, out);
    return popup;
}

std::string BranchLabel(const data::BranchInfo& b) {
    if (b.version.empty()) return b.name;
    return b.name + " v" + b.version;
}

} // namespace

void FillDisplayedBranch(UiState& ui, const data::CheatInfo& info, time_t now) {
    const data::BranchInfo* b = info.ActiveBranch();
    ui.modal.displayedBranch = b ? BranchLabel(*b) : "—";
    ui.modal.displayedUpdated = FormatDate(b ? b->buildDate : 0, now);
    ui.modal.displayedValid = FormatValidUntil(info.lifetime, info.license);
    ui.modal.displayedLastLaunch = FormatRelativeTime(b ? b->lastLaunch : 0, now);
    ui.modal.displayedChangelogBlocks.clear();
    if (b) {
        for (const auto& ent : b->changelogs) {
            ModalState::DisplayedChangelogBlock block;
            block.date = FormatDate(ent.date);
            std::istringstream iss(ent.text);
            std::string line;
            while (std::getline(iss, line))
                block.lines.push_back(line);
            ui.modal.displayedChangelogBlocks.push_back(std::move(block));
        }
    }
}

void DrawGameModal(Theme& theme, UiState& ui, const UiGpuResources& gpu, data::CheatStore& store,
                   const FrameInput& frame, ModalActions& out) {
    if (!ui.modal.open && !ui.modal.closing) {
        ui.disableDrag = false;
        return;
    }
    ui.disableDrag = true;

    const int winW = frame.winW;
    const int winH = frame.winH;
    const float dt = frame.dt;
    const std::uint64_t now = frame.nowMs;
    const time_t nowUnix = frame.nowUnix;
    TickInject(ui, now);

    auto* info = store.Find(ui.modal.gameId);
    if (!info) {
        ui.modal = {};
        ui.inject = {};
        ui.disableDrag = false;
        return;
    }

    const bool active = info->IsActive(nowUnix);
    if (!active && !ui.modal.closing) {
        ui.modal.closing = true;
        ui.modal.closeTime = now;
    }

    float modalT =
        ui.modal.closing
            ? 1.f - std::min(static_cast<float>(now - ui.modal.closeTime) / kModalFadeMs, 1.f)
            : std::min(static_cast<float>(now - ui.modal.openTime) / kModalFadeMs, 1.f);
    ui.modal.alpha = EaseInOutCubic(modalT);
    if (modalT <= 0.f && ui.modal.closing) {
        ui.modal.open = false;
        ui.modal.closing = false;
        ui.modal.branchMenu = false;
        ui.modal.branchMenuT = 0.f;
        ui.modal.contentFade = 1.f;
        ui.modal.contentFadingOut = false;
        ui.modal.contentFadingIn = false;
        ui.modal.contentSwapPending = false;
        ui.modal.reopenBlockUntil = now + kModalReopenBlockMs;
        ui.inject = {};
        ui.modal.gameId.clear();
        ui.disableDrag = false;
        return;
    }

    TickBranchContentFade(ui, *info, dt, nowUnix);

    float modalW = ui.inject.modalW;
    float modalH = ui.inject.modalH;
    float contentAlpha = ui.inject.contentAlpha;
    ImVec2 modalPos((static_cast<float>(winW) - modalW) / 2.f,
                    (static_cast<float>(winH) - modalH) / 2.f);

    ImGui::SetNextWindowPos(modalPos);
    ImGui::SetNextWindowSize(ImVec2(modalW, modalH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##GameModal", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoCollapse);

    ImVec2 windowPos = ImGui::GetWindowPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##modalCatch", ImVec2(modalW, modalH));

    DrawModalFrame(drawList, theme, windowPos, modalW, modalH, ui.modal.alpha);

    BranchPopupLayout popup{};
    if (!ui.inject.showContent) {
        popup = DrawDetailBody(theme, ui, *info, gpu, windowPos, modalW, modalH, contentAlpha,
                               frame, out);
    } else {
        ID3D11ShaderResourceView* tex = gpu.ArtFor(ArtForGameId(ui.modal.gameId));
        DrawInjectProgress(theme, drawList, tex, windowPos, modalW, modalH,
                           ui.modal.alpha * ui.inject.logoAlpha, ui.inject.progress);
    }

    if (!ui.inject.showContent && contentAlpha > 0.001f) {
        float titleH = 0.f;
        {
            const ScopedFont titleFont(theme.fontModalTitle);
            titleH = ImGui::GetTextLineHeight();
        }
        const float closeSz = 10.f;
        const float closeHit = 20.f;
        float closeY = 27.f + titleH / 2.f - closeHit / 2.f;
        ImVec2 closeBtn(windowPos.x + modalW - 35.f, windowPos.y + closeY);
        ImGui::SetCursorPos(ImVec2(closeBtn.x - windowPos.x, closeBtn.y - windowPos.y));
        ImGui::InvisibleButton("##modalClose", ImVec2(closeHit, closeHit));
        bool closeHov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && !ui.inject.active && !ui.modal.closing)
            RequestModalClose(ui, now);
        ImVec4 closeCol = closeHov ? ImVec4(theme.text.x, theme.text.y, theme.text.z,
                                            ui.modal.alpha * contentAlpha)
                                   : ImVec4(theme.textDim.x, theme.textDim.y, theme.textDim.z,
                                            ui.modal.alpha * contentAlpha);
        ImVec2 closePos(closeBtn.x + (closeHit - closeSz) * 0.5f,
                        closeBtn.y + (closeHit - closeSz) * 0.5f);
        drawList->AddLine(closePos, ImVec2(closePos.x + closeSz, closePos.y + closeSz),
                          ImGui::GetColorU32(closeCol), 2.f);
        drawList->AddLine(ImVec2(closePos.x, closePos.y + closeSz),
                          ImVec2(closePos.x + closeSz, closePos.y), ImGui::GetColorU32(closeCol),
                          2.f);
    }

    ImGui::End();
    ImGui::PopStyleVar();

    if (popup.wantDraw && !ui.inject.showContent && contentAlpha > 0.001f)
        DrawBranchPopup(theme, ui, *info, popup, modalPos, modalW, modalH, dt, now, out);

    if (ImGui::IsMouseClicked(0) && !ui.inject.active && !ui.modal.closing) {
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        const bool inCard = mousePos.x >= modalPos.x && mousePos.x <= modalPos.x + modalW &&
                            mousePos.y >= modalPos.y && mousePos.y <= modalPos.y + modalH;
        const bool inPop = popup.wantDraw && mousePos.x >= popup.pos.x &&
                           mousePos.x <= popup.pos.x + popup.w && mousePos.y >= popup.pos.y &&
                           mousePos.y <= popup.pos.y + popup.h;
        if (!inCard && !inPop) RequestModalClose(ui, now);
    }
}

} // namespace nl::ui
