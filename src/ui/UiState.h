#pragma once
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>
#include "Theme.h"
#include "Widgets.h"
#include "../Cheats.h"

namespace nl::ui {

struct UiGpuResources;

constexpr float kModalFadeMs = 300.f;
constexpr float kModalW = 450.f;
constexpr float kModalH = 350.f;
constexpr float kInjectW = 350.f;
constexpr float kInjectH = 200.f;
constexpr float kContainerAnimSec = 0.5f;
constexpr int kMaxBranchHover = 8;
constexpr int kMaxGameSlots = 8;
constexpr std::uint64_t kModalReopenBlockMs = 280;
constexpr std::uint64_t kBranchLoadBlockMs = 250;

struct FrameInput {
    std::uint64_t nowMs = 0;
    time_t nowUnix = 0;
    float dt = 0.f;
    int winW = 0;
    int winH = 0;
};

struct ContainerInfo {
    int gameIdx = 0;
    bool isActive = false;
    float currentY = 0.f;
    float targetY = 0.f;
    float startY = 0.f;
    float animationProgress = 1.f;
};

struct ModalState {
    bool open = false;
    bool closing = false;
    std::uint64_t openTime = 0;
    std::uint64_t closeTime = 0;
    std::uint64_t reopenBlockUntil = 0;
    float alpha = 0.f;
    std::string gameId;

    bool branchMenu = false;
    float branchMenuT = 0.f;
    std::uint64_t loadBlockUntil = 0;
    float branchItemHover[kMaxBranchHover] = {};

    float contentFade = 1.f;
    bool contentFadingOut = false;
    bool contentFadingIn = false;
    bool contentSwapPending = false;
    std::string displayedBranch;
    std::string displayedUpdated;
    std::string displayedValid;
    std::string displayedLastLaunch;
    struct DisplayedChangelogBlock {
        std::string date;
        std::vector<std::string> lines;
    };
    std::vector<DisplayedChangelogBlock> displayedChangelogBlocks;
};

struct InjectAnim {
    bool active = false;
    bool showContent = false;
    std::uint64_t startTime = 0;
    float contentAlpha = 1.f;
    float modalW = kModalW;
    float modalH = kModalH;
    float logoAlpha = 0.f;
    float progress = 0.f;
};

struct UiState {
    float closeHover = 0.f;
    float minimizeHover = 0.f;
    float siteHover = 0.f;
    float discordHover = 0.f;
    float telegramHover = 0.f;
    float communityHover = 0.f;
    float apiHover = 0.f;

    ScrollState listScroll;
    ScrollState textScroll;

    float gameDim[kMaxGameSlots] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
    bool dimReady = false;
    unsigned prevActiveMask = 0;
    bool prevActiveValid = false;

    std::vector<ContainerInfo> containers;
    ModalState modal;
    InjectAnim inject;
    bool disableDrag = false;
    bool requestExit = false;
};

struct DashActions {
    bool close = false;
    bool minimize = false;
    const wchar_t* openUrl = nullptr;
};

struct ModalActions {
    const wchar_t* openUrl = nullptr;
    bool persistLastLaunch = false;
    std::string lastLaunchCheatId;
    std::string lastLaunchBranchId;
    time_t lastLaunchWhen = 0;
    bool persistSelectedBranch = false;
    std::string selectedCheatId;
    std::string selectedBranchId;
};

void FillDisplayedBranch(UiState& ui, const data::CheatInfo& info, time_t now);

void DrawSplash(Theme& theme, float timeSec, float loaderAlpha);
void DrawDashboard(Theme& theme, UiState& ui, const UiGpuResources& gpu, data::CheatStore& store,
                   float contentAlpha, const FrameInput& frame, DashActions& out);
void DrawGameModal(Theme& theme, UiState& ui, const UiGpuResources& gpu, data::CheatStore& store,
                   const FrameInput& frame, ModalActions& out);

} // namespace nl::ui
