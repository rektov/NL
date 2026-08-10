#pragma once
#include "AppDevice.h"
#include "AppWindow.h"
#include "Cheats.h"
#include "gfx/Blur.h"
#include "ui/Anim.h"
#include "ui/Theme.h"
#include "ui/UiGpuResources.h"
#include "ui/UiState.h"
#include <ctime>

namespace nl {

class AppFrame {
  public:
    AppFrame() = default;
    AppFrame(const AppFrame&) = delete;
    AppFrame& operator=(const AppFrame&) = delete;

    [[nodiscard]] bool IsImGuiReady() const { return imguiReady_; }
    [[nodiscard]] bool IsDragDisabled() const { return ui_.disableDrag; }
    [[nodiscard]] bool WantCaptureMouse() const;

    ui::AnimState& Anim() { return anim_; }
    const ui::AnimState& Anim() const { return anim_; }
    data::CheatStore& Store() { return store_; }

    void InitClock();
    void ConfigureBootAnim(int splashSize, int targetW, int targetH);

    [[nodiscard]] bool InitImGui(HWND hwnd, AppDevice& device);
    [[nodiscard]] bool InitBlurAndTextures(AppDevice& device);
    [[nodiscard]] bool RecreateGraphics(AppDevice& device, AppWindow& window);

    void RequestUserClose(AppWindow& window);
    void OnDpiChanged(AppDevice& device, UINT dpi, int width, int height);
    [[nodiscard]] bool TickFrame(AppWindow& window, AppDevice& device, bool& running);

    void ShutdownGraphics();

  private:
    void InitFonts();
    void TeardownImGui();
    void OpenUrl(const wchar_t* url);
    void ProcessDashActions(const ui::DashActions& act, AppWindow& window, bool& requestExitAnim);
    void ProcessModalActions(ui::ModalActions& act, time_t now);
    void RenderUi(float dt, AppWindow& window);

    ui::Theme theme_;
    ui::AnimState anim_;
    ui::UiState ui_;
    ui::UiGpuResources gpu_;
    data::CheatStore store_;
    gfx::Blur blur_;

    bool contextCreated_ = false;
    bool win32Backend_ = false;
    bool dx11Backend_ = false;
    bool imguiReady_ = false;

    LARGE_INTEGER qpcFreq_{};
    LARGE_INTEGER qpcLast_{};
    float timeSec_ = 0.f;
};

}
