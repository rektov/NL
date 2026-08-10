#pragma once
#include <Windows.h>

namespace nl {

struct AppWindowHost {
    virtual ~AppWindowHost() = default;
    virtual bool ForwardRawMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;
    virtual bool WantsMouseCapture() const = 0;
    virtual bool IsDragDisabled() const = 0;
    virtual void OnBeginDrag() = 0;
    virtual void OnDragMove() = 0;
    virtual void OnEndDrag() = 0;
    virtual void OnRequestClose() = 0;
    virtual void OnDpiChanged(UINT dpi, int width, int height) = 0;
};

class AppWindow {
  public:
    AppWindow() = default;
    ~AppWindow();

    AppWindow(const AppWindow&) = delete;
    AppWindow& operator=(const AppWindow&) = delete;

    [[nodiscard]] bool Create(HINSTANCE inst, int targetW, int targetH, AppWindowHost* host);
    void Destroy(bool postQuit = true);

    HWND Hwnd() const { return hwnd_; }
    HINSTANCE Instance() const { return instance_; }
    bool IsMinimized() const { return hwnd_ && IsIconic(hwnd_); }
    [[nodiscard]] UINT Dpi() const { return dpi_; }

    void ApplyRoundRegion(int fullW, int fullH, int winW, int winH, int cornerRadius);
    void SetLayeredAlpha(BYTE alpha);

    void BeginDrag();
    void DragTo();
    void EndDrag();
    bool IsDragging() const { return dragging_; }

  private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    AppWindowHost* host_ = nullptr;
    bool classRegistered_ = false;
    bool dragging_ = false;
    bool postQuitOnDestroy_ = true;
    BYTE lastLayeredAlpha_ = 255;
    UINT dpi_ = 96;
    POINT dragStart_{};
    POINT dragOrigin_{};
};

}
