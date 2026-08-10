#include "App.h"
#include "AppWindow.h"
#include "AppDevice.h"
#include "AppFrame.h"
#include "Log.h"
#include "ui/Anim.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

namespace nl {

struct App::Impl : AppWindowHost {
    AppWindow window;
    AppDevice device;
    AppFrame frame;
    bool running = true;
    bool shutdownDone = false;

    bool ForwardRawMessage(HWND h, UINT m, WPARAM w, LPARAM l) override {
        if (!frame.IsImGuiReady()) return false;
        return ImGui_ImplWin32_WndProcHandler(h, m, w, l) != 0;
    }
    bool WantsMouseCapture() const override { return frame.WantCaptureMouse(); }
    bool IsDragDisabled() const override { return frame.IsDragDisabled(); }
    void OnBeginDrag() override { window.BeginDrag(); }
    void OnDragMove() override {
        if (window.IsDragging()) window.DragTo();
    }
    void OnEndDrag() override { window.EndDrag(); }
    void OnRequestClose() override { frame.RequestUserClose(window); }
    void OnDpiChanged(UINT dpi, int width, int height) override {
        frame.OnDpiChanged(device, dpi, width, height);
    }
};

App::App() : impl_(std::make_unique<Impl>()) {}
App::~App() {
    Shutdown();
}

bool App::Init(HINSTANCE inst) {
    Impl& a = *impl_;
    a.frame.ConfigureBootAnim(ui::kSplashSize, 500, 400);

    if (!a.window.Create(inst, a.frame.Anim().targetW, a.frame.Anim().targetH, &a)) return false;

    a.window.ApplyRoundRegion(a.frame.Anim().targetW, a.frame.Anim().targetH, a.frame.Anim().winW,
                              a.frame.Anim().winH, ui::kCornerRadius);
    a.frame.Anim().regionDirty = false;

    if (!a.device.Init(a.window.Hwnd(), a.frame.Anim().targetW, a.frame.Anim().targetH)) {
        Shutdown();
        return false;
    }

    if (!a.frame.InitImGui(a.window.Hwnd(), a.device)) {
        Shutdown();
        return false;
    }

    if (!a.frame.InitBlurAndTextures(a.device)) {
        Shutdown();
        return false;
    }

    if (!a.frame.Store().Load()) {
        Log("CheatStore::Load failed");
        MessageBoxW(a.window.Hwnd(), L"Failed to load cheats catalog", L"NL Loader",
                    MB_ICONWARNING);
    }

    a.frame.InitClock();
    return true;
}

void App::Run() {
    Impl& a = *impl_;
    MSG msg{};
    while (a.running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                a.running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!a.running) break;
        if (!a.frame.TickFrame(a.window, a.device, a.running)) break;
    }
}

void App::Shutdown() {
    if (!impl_) return;
    Impl& a = *impl_;
    if (a.shutdownDone) return;
    a.shutdownDone = true;

    a.frame.ShutdownGraphics();
    a.device.Cleanup();
    a.window.Destroy(false);
}

}
