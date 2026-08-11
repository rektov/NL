#include "UiState.h"
#include "UiGpuResources.h"
#include "GameCatalog.h"
#include "Widgets.h"
#include "Anim.h"
#include "../Format.h"
#include "BlurImGui.h"
#include "../gfx/Blur.h"
#include "imgui.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <span>
#include <vector>

namespace nl::ui {

static_assert(kGameCatalogCount <= kMaxGameSlots,
              "gameDim[] would silently stop animating slots past kMaxGameSlots");

namespace {

constexpr float kCardRounding = 10.f;
constexpr float kCardBorder = 1.5f;
constexpr float kCardPadX = 15.f;
constexpr float kCardTitleY = 10.f;
constexpr float kCardStatusY = 35.f;
constexpr float kCardIconSize = 20.f;
constexpr float kCardIconPad = 10.f;
constexpr float kCardIconRound = 5.f;
constexpr float kInactiveDim = 0.5f;
constexpr ImVec4 kOpaqueWhite(1.f, 1.f, 1.f, 1.f);

bool ActiveFirstThenIndex(const ContainerInfo& a, const ContainerInfo& b) {
    if (a.isActive != b.isActive) return a.isActive > b.isActive;
    return a.gameIdx < b.gameIdx;
}

void OpenGameModal(UiState& ui, data::CheatStore& store, const char* openId, std::uint64_t nowMs,
                   time_t nowUnix) {
    ui.modal.open = true;
    ui.modal.closing = false;
    ui.modal.openTime = nowMs;
    ui.modal.alpha = 0.f;
    ui.modal.gameId = openId;
    ui.inject = {};
    ui.inject.modalW = kModalW;
    ui.inject.modalH = kModalH;
    ui.inject.contentAlpha = 1.f;
    ui.modal.branchMenu = false;
    ui.modal.branchMenuT = 0.f;
    ui.modal.contentFade = 1.f;
    ui.modal.contentFadingOut = false;
    ui.modal.contentFadingIn = false;
    ui.modal.contentSwapPending = false;
    if (auto* info = store.Find(openId)) FillDisplayedBranch(ui, *info, nowUnix);
    ui.textScroll.Reset();
}

float DrawLogoAndHeader(Theme& theme, float contentAlpha, int winW) {
    {
        const ScopedFont logoFont(theme.fontLogo);
        ImGui::SetCursorPos(ImVec2(18.f, 15.f));
        {
            const ScopedTextColor accent(theme.accent, contentAlpha);
            ImGui::TextUnformatted("NL");
        }
        ImGui::SetCursorPos(ImVec2(20.f, 15.f));
        {
            const ScopedTextColor face(theme.text, contentAlpha);
            ImGui::TextUnformatted("NL");
        }
    }

    ImVec2 subPos;
    {
        const ScopedFont titleFont(theme.fontTitle);
        const ScopedTextColor color(theme.text, 0.9f * contentAlpha);
        const float subW = ImGui::CalcTextSize("Subscription").x;
        subPos = ImVec2((static_cast<float>(winW) - subW - 50.f) / 2.f, 20.f);
        ImGui::SetCursorPos(subPos);
        ImGui::TextUnformatted("Subscription");
    }

    {
        const ScopedFont bodyFont(theme.fontDefault);
        const ScopedTextColor color(theme.textMuted, contentAlpha);
        ImGui::SetCursorPos(ImVec2(subPos.x, 45.f));
        ImGui::TextUnformatted("Available subscriptions");
    }
    return subPos.x;
}

void DrawSideLinks(Theme& theme, UiState& ui, float contentAlpha, float dt, bool modalOpen,
                   DashActions& out) {
    struct LinkBtn {
        const char* label;
        float w;
        float* hover;
        const wchar_t* url;
    };
    LinkBtn links[3] = {
        {"Site", 50.f, &ui.siteHover, L"https://vk.com/yowls"},
        {"Discord", 70.f, &ui.discordHover, L"https://discord.com/"},
        {"Telegram", 80.f, &ui.telegramHover, L"https://t.me/"},
    };
    float buttonY = 100.f;
    for (auto& btn : links) {
        ImGui::SetCursorPos(ImVec2(20.f, buttonY));
        ImGui::InvisibleButton(btn.label, ImVec2(btn.w, 20.f));
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked() && !modalOpen) out.openUrl = btn.url;
        HoverLerp(*btn.hover, hovered, dt);
        const ImVec4 col =
            Lerp4(ImVec4(theme.textDim.x, theme.textDim.y, theme.textDim.z, contentAlpha),
                  ImVec4(theme.text.x, theme.text.y, theme.text.z, contentAlpha), *btn.hover);
        ImGui::SetCursorPos(ImVec2(20.f, buttonY));
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(btn.label);
        ImGui::PopStyleColor();
        buttonY += 35.f;
    }
}

void TickContainerAnim(UiState& ui, float dt) {
    for (auto& c : ui.containers) {
        if (c.animationProgress < 1.f) {
            c.animationProgress = std::min(1.f, c.animationProgress + dt / kContainerAnimSec);
            const float t = EaseInOutCubic(c.animationProgress);
            c.currentY = c.startY + (c.targetY - c.startY) * t;
        } else {
            c.currentY = c.targetY;
        }
    }
}

struct ComingSoonCard {
    float y = 0.f;
    const char* name = "";
    ID3D11ShaderResourceView* tex = nullptr;
};

void DrawGameCard(ImDrawList* dl, Theme& theme, const ImVec2& pos, float containerWidth,
                  float containerHeight, float vis, const char* gameName, bool isActive,
                  const char* status, ID3D11ShaderResourceView* texture) {
    const ImVec2 end(pos.x + containerWidth, pos.y + containerHeight);
    dl->AddRectFilled(pos, end, ColorA(theme.cardBg, vis), kCardRounding);
    dl->AddRect(pos, end, ColorA(theme.cardOutline, vis), kCardRounding, 0, kCardBorder);

    const float titleTop = pos.y + kCardTitleY;
    float textX = pos.x + kCardPadX;
    if (!isActive) {
        const ImVec2 circle(pos.x + kCardPadX, titleTop + ImGui::GetFontSize() * 0.5f);
        dl->AddCircleFilled(circle, 3.f, ColorA(theme.accent, vis), 32);
        dl->AddCircleFilled(circle, 5.f, ColorA(theme.accent, 0.35f * vis), 32);
        dl->AddCircleFilled(circle, 7.5f, ColorA(theme.accent, 0.15f * vis), 32);
        textX += 12.f;
    }

    ImGui::SetCursorPos(ImVec2(textX, titleTop));
    {
        const ScopedTextColor color(theme.text, vis);
        ImGui::TextUnformatted(gameName);
    }

    ImGui::SetCursorPos(ImVec2(pos.x + kCardPadX, pos.y + kCardStatusY));
    {
        const ScopedTextColor color(theme.textMuted, vis);
        ImGui::TextUnformatted(status);
    }

    if (texture) {
        const ImVec2 ip(pos.x + containerWidth - kCardIconSize - kCardIconPad,
                        pos.y + kCardIconPad);
        dl->AddImageRounded(reinterpret_cast<ImTextureID>(texture), ip,
                            ImVec2(ip.x + kCardIconSize, ip.y + kCardIconSize), ImVec2(0, 0),
                            ImVec2(1, 1), ColorA(kOpaqueWhite, vis), kCardIconRound);
    }
}

void DrawComingSoonOverlay(ImDrawList* dl, Theme& theme, const UiGpuResources& gpu,
                           std::span<const ComingSoonCard> cards, float startX,
                           float containerWidth, float containerHeight, float containersStartY,
                           float contentAlpha, int winW, int winH, bool containersAnimating,
                           ScrollState& listScroll) {
    const float displayW = static_cast<float>(winW);
    const float displayH = static_cast<float>(winH);
    const float x1 = startX + containerWidth;
    const float contentA = std::clamp(contentAlpha, 0.f, 1.f);
    ImFont* font = ImGui::GetFont();

    dl->PushClipRect(ImVec2(startX, containersStartY),
                     ImVec2(static_cast<float>(winW), static_cast<float>(winH)), true);

    constexpr float kSoonOpacity = 180.f / 255.f;
    const float soonA = contentA * kSoonOpacity;
    const ImVec4 soonText(theme.text.x * 0.86f, theme.text.y * 0.86f, theme.text.z * 0.86f, 1.f);

    for (const ComingSoonCard& card : cards) {
        const ImVec2 pos(startX, card.y);
        const ImVec2 end(x1, card.y + containerHeight);
        dl->AddRectFilled(pos, end, ColorA(theme.cardBg, soonA), kCardRounding);
        dl->AddRect(pos, end, ColorA(theme.cardOutline, soonA * 0.7f), kCardRounding, 0,
                    kCardBorder);
        dl->AddText(font, font->FontSize, ImVec2(startX + kCardPadX, card.y + kCardTitleY),
                    ColorA(soonText, soonA), card.name);
        dl->AddText(font, font->FontSize, ImVec2(startX + kCardPadX, card.y + kCardStatusY),
                    ColorA(theme.textMuted, soonA), "Coming soon");
        if (card.tex) {
            const ImVec2 ip(x1 - kCardIconSize - kCardIconPad, pos.y + kCardIconPad);
            dl->AddImageRounded(reinterpret_cast<ImTextureID>(card.tex), ip,
                                ImVec2(ip.x + kCardIconSize, ip.y + kCardIconSize), ImVec2(0, 0),
                                ImVec2(1, 1), ColorA(kOpaqueWhite, soonA * 0.28f), kCardIconRound);
        }
    }

    bool blurred = false;
    if (gpu.blur && gpu.blur->Ready()) {
        const bool blurDirty = listScroll.IsMoving() || containersAnimating ||
                               contentAlpha < 0.999f || !gpu.blur->HasEffect();
        blurred = QueueBlurCapture(*gpu.blur, dl, blurDirty);
        if (blurred) {
            for (const ComingSoonCard& card : cards) {
                BlitBlurSolid(*gpu.blur, dl, startX, card.y, x1, card.y + containerHeight, 1.f,
                              displayW, displayH, theme.space.cardRounding);
            }
        }
    }
    if (!blurred) {
        for (const ComingSoonCard& card : cards) {
            dl->AddRectFilled(ImVec2(startX, card.y), ImVec2(x1, card.y + containerHeight),
                              ColorA(theme.cardBg, 1.f), theme.space.cardRounding);
        }
    }
    dl->PopClipRect();
}

void DrawAvatarFooter(ImDrawList* dl, Theme& theme, const UiGpuResources& gpu, float contentAlpha,
                      int winH) {
    if (!gpu.avatar) return;
    constexpr float kImageSize = 40.f;
    const ImVec2 ip(20.f, static_cast<float>(winH) - kImageSize - 25.f);
    dl->AddImage(reinterpret_cast<ImTextureID>(gpu.avatar.Get()), ip,
                 ImVec2(ip.x + kImageSize, ip.y + kImageSize), ImVec2(0, 0), ImVec2(1, 1),
                 ColorA(ImVec4(1, 1, 1, 1), contentAlpha));
    ImGui::SetCursorPos(
        ImVec2(ip.x + kImageSize + 10.f, ip.y + (kImageSize - ImGui::GetTextLineHeight()) / 2.f));
    const ScopedTextColor color(theme.text, 0.9f * contentAlpha);
    ImGui::TextUnformatted("yowls");
}

void DrawWindowChrome(ImDrawList* dl, Theme& theme, UiState& ui, float contentAlpha, float dt,
                      int winW, bool modalOpen, DashActions& out) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::SetCursorPos(ImVec2(static_cast<float>(winW) - 35.f, 20.f));
    ImGui::InvisibleButton("CloseButton", ImVec2(20.f, 20.f));
    HoverLerp(ui.closeHover, ImGui::IsItemHovered(), dt);
    if (ImGui::IsItemClicked() && !modalOpen) out.close = true;
    const ImVec4 closeCol =
        Lerp4(ImVec4(theme.textDim.x, theme.textDim.y, theme.textDim.z, contentAlpha),
              ImVec4(theme.text.x, theme.text.y, theme.text.z, contentAlpha), ui.closeHover);
    dl->AddLine(ImVec2(static_cast<float>(winW) - 30.f, 25.f),
                ImVec2(static_cast<float>(winW) - 20.f, 35.f), ImGui::GetColorU32(closeCol), 2.f);
    dl->AddLine(ImVec2(static_cast<float>(winW) - 30.f, 35.f),
                ImVec2(static_cast<float>(winW) - 20.f, 25.f), ImGui::GetColorU32(closeCol), 2.f);

    ImGui::SetCursorPos(ImVec2(static_cast<float>(winW) - 60.f, 25.f));
    ImGui::InvisibleButton("MinimizeButton", ImVec2(20.f, 20.f));
    HoverLerp(ui.minimizeHover, ImGui::IsItemHovered(), dt);
    if (ImGui::IsItemClicked() && !modalOpen) out.minimize = true;
    const ImVec4 minCol =
        Lerp4(ImVec4(theme.textDim.x, theme.textDim.y, theme.textDim.z, contentAlpha),
              ImVec4(theme.text.x, theme.text.y, theme.text.z, contentAlpha), ui.minimizeHover);
    dl->AddLine(ImVec2(static_cast<float>(winW) - 55.f, 30.f),
                ImVec2(static_cast<float>(winW) - 43.f, 30.f), ImGui::GetColorU32(minCol), 2.f);
    ImGui::PopStyleVar();
}

void DrawModalDim(ImDrawList* dl, const ModalState& modal, int winW, int winH, std::uint64_t now) {
    if (!modal.open && !modal.closing) return;
    const float modalT =
        modal.closing
            ? 1.f - std::min(static_cast<float>(now - modal.closeTime) / kModalFadeMs, 1.f)
            : std::min(static_cast<float>(now - modal.openTime) / kModalFadeMs, 1.f);
    const float dimA = EaseInOutCubic(modalT) * 0.55f;
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(static_cast<float>(winW), static_cast<float>(winH)),
                      ImGui::GetColorU32(ImVec4(0, 0, 0, dimA)));
}

} // namespace

void DrawDashboard(Theme& theme, UiState& ui, const UiGpuResources& gpu, data::CheatStore& store,
                   float contentAlpha, const FrameInput& frame, DashActions& out) {
    const int winW = frame.winW;
    const int winH = frame.winH;
    const float dt = frame.dt;
    const std::uint64_t now = frame.nowMs;
    const time_t nowUnix = frame.nowUnix;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(winW), static_cast<float>(winH)));
    ImGui::Begin("MainWindow", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus);
    ImGui::SetScrollY(0);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    bool slotActive[kGameCatalogCount]{};
    unsigned activeMask = 0;
    for (int i = 0; i < kGameCatalogCount; ++i) {
        if (!kGameCatalog[i].id) continue;
        if (const auto* info = store.Find(kGameCatalog[i].id)) {
            slotActive[i] = info->IsActive(nowUnix);
            if (slotActive[i]) activeMask |= (1u << i);
        }
    }
    if (!ui.prevActiveValid) {
        ui.prevActiveMask = activeMask;
        ui.prevActiveValid = true;
    }

    constexpr float kDimSpeed = 2.f;
    for (int i = 0; i < kGameCatalogCount && i < kMaxGameSlots; ++i) {
        const float target = (!kGameCatalog[i].id || slotActive[i]) ? 1.f : kInactiveDim;
        if (!ui.dimReady || contentAlpha < 0.01f)
            ui.gameDim[i] = target;
        else {
            ui.gameDim[i] += (target - ui.gameDim[i]) * kDimSpeed * dt;
            ui.gameDim[i] = std::clamp(ui.gameDim[i], kInactiveDim, 1.f);
        }
    }
    ui.dimReady = true;

    const float startX = DrawLogoAndHeader(theme, contentAlpha, winW);
    ImGui::PushFont(theme.fontDefault);
    DrawSideLinks(theme, ui, contentAlpha, dt, ui.modal.open, out);

    constexpr float kContainersStartY = 100.f;
    constexpr float kScrollSpeed = 60.f;
    constexpr float kContainerHeight = 80.f;
    constexpr float kContainerGap = 10.f;
    const float containerWidth = static_cast<float>(winW) - startX - 20.f;
    const float containersAreaHeight = static_cast<float>(winH) - kContainersStartY;

    if (ui.containers.empty()) {
        ui.containers.reserve(static_cast<size_t>(kGameCatalogCount));
        for (int i = 0; i < kGameCatalogCount; ++i)
            ui.containers.push_back({i, slotActive[i], 0, 0, 0, 1.f});
        std::stable_sort(ui.containers.begin(), ui.containers.end(), ActiveFirstThenIndex);
        for (size_t i = 0; i < ui.containers.size(); ++i) {
            const float y =
                kContainersStartY + static_cast<float>(i) * (kContainerHeight + kContainerGap);
            ui.containers[i].targetY = ui.containers[i].currentY = ui.containers[i].startY = y;
        }
    } else {
        for (auto& c : ui.containers) {
            if (c.gameIdx >= 0 && c.gameIdx < kGameCatalogCount) c.isActive = slotActive[c.gameIdx];
        }
    }

    if (ui.prevActiveMask != activeMask) {
        std::stable_sort(ui.containers.begin(), ui.containers.end(), ActiveFirstThenIndex);
        ui.prevActiveMask = activeMask;
        for (auto& c : ui.containers) {
            c.startY = c.currentY;
            c.animationProgress = 0.f;
        }
        if (gpu.blur) gpu.blur->InvalidateCache();
    }

    for (size_t i = 0; i < ui.containers.size(); ++i)
        ui.containers[i].targetY =
            kContainersStartY + static_cast<float>(i) * (kContainerHeight + kContainerGap);
    TickContainerAnim(ui, dt);

    const float cardCount = static_cast<float>(ui.containers.size());
    const float contentH = cardCount * (kContainerHeight + kContainerGap);
    const float maxScroll = std::max(0.f, contentH - containersAreaHeight);

    {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const bool over = !ui.modal.open && mouse.x >= startX &&
                          mouse.x <= startX + containerWidth && mouse.y >= kContainersStartY &&
                          mouse.y <= kContainersStartY + containersAreaHeight;
        TickScroll(ui.listScroll, maxScroll, ImGui::GetIO().MouseWheel, over, dt, now,
                   kScrollSpeed);
    }

    ImGui::PushClipRect(ImVec2(startX, kContainersStartY),
                        ImVec2(static_cast<float>(winW), static_cast<float>(winH)), true);
    const float overscrollT = ui.listScroll.overscroll * 0.5f;
    bool containersAnimating = false;

    std::vector<ComingSoonCard> comingSoon;
    comingSoon.reserve(static_cast<size_t>(kGameCatalogCount));

    for (auto& container : ui.containers) {
        if (container.animationProgress < 1.f) containersAnimating = true;
        const float baseY = container.currentY - ui.listScroll.current + overscrollT;
        const ImVec2 pos(startX, baseY);
        const int idx = container.gameIdx;
        if (idx < 0 || idx >= kGameCatalogCount) continue;

        const GameCatalogEntry& entry = kGameCatalog[idx];
        const bool isComingSoon = (entry.id == nullptr);
        const data::CheatInfo* info = isComingSoon ? nullptr : store.Find(entry.id);
        const char* gameName = entry.fallbackName;
        if (info && !info->name.empty()) gameName = info->name.c_str();
        ID3D11ShaderResourceView* texture = gpu.ArtFor(entry.art);
        const bool isActive = slotActive[idx];
        const float dim = (idx < kMaxGameSlots) ? ui.gameDim[idx] : 1.f;
        const char* openId = entry.id;

        if (isComingSoon) comingSoon.push_back({baseY, gameName, texture});

        ImGui::SetCursorPos(pos);
        char gameBtnId[32];
        std::snprintf(gameBtnId, sizeof(gameBtnId), "##game%d", idx);
        ImGui::InvisibleButton(gameBtnId, ImVec2(containerWidth, kContainerHeight));
        if (ImGui::IsItemClicked() && !ui.modal.open && !ui.modal.closing &&
            now >= ui.modal.reopenBlockUntil && isActive && openId) {
            OpenGameModal(ui, store, openId, now, nowUnix);
        }

        if (isComingSoon) continue;

        const std::string status = nl::FormatSubscriptionStatus(
            isActive, info ? info->lifetime : false, info ? info->license : 0, nowUnix);
        DrawGameCard(dl, theme, pos, containerWidth, kContainerHeight, contentAlpha * dim, gameName,
                     isActive, status.c_str(), texture);
    }
    ImGui::PopClipRect();

    if (!comingSoon.empty() && contentAlpha > 0.01f) {
        DrawComingSoonOverlay(dl, theme, gpu, comingSoon, startX, containerWidth, kContainerHeight,
                              kContainersStartY, contentAlpha, winW, winH, containersAnimating,
                              ui.listScroll);
    }

    DrawScrollBar(dl, ui.listScroll, maxScroll, contentH, 0.f, kContainersStartY,
                  containersAreaHeight, static_cast<float>(winW) - 3.f, theme.accent);

    DrawAvatarFooter(dl, theme, gpu, contentAlpha, winH);
    DrawWindowChrome(dl, theme, ui, contentAlpha, dt, winW, ui.modal.open, out);
    DrawModalDim(dl, ui.modal, winW, winH, now);

    ImGui::PopFont();
    ImGui::End();
}

} // namespace nl::ui
