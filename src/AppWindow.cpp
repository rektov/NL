#include "AppWindow.h"
#include "Win32Util.h"
#include "Log.h"

namespace nl {
namespace {

constexpr wchar_t kWndClassName[] = L"NlLoaderWnd";

}

AppWindow::~AppWindow() {
    Destroy(false);
}

bool AppWindow::Create(HINSTANCE inst, int targetW, int targetH, AppWindowHost* host) {
    instance_ = inst;
    host_ = host;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = AppWindow::WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWndClassName;
    if (!RegisterClassExW(&wc)) {
        const DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            Log("RegisterClassExW failed: {}", err);
            return false;
        }
    } else {
        classRegistered_ = true;
    }

    const MonitorCenter c = CenterOnNearestMonitor(targetW, targetH);
    hwnd_ = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_LAYERED | WS_EX_TOPMOST, kWndClassName,
                            L"NL Loader", WS_POPUP | WS_VISIBLE, c.x, c.y, targetW, targetH,
                            nullptr, nullptr, inst, this);
    if (!hwnd_) {
        Log("CreateWindowExW failed: {}", GetLastError());
        return false;
    }
    dpi_ = DpiForWindowOrDefault(hwnd_);
    lastLayeredAlpha_ = 0;
    SetLayeredWindowAttributes(hwnd_, 0, 0, LWA_ALPHA);
    return true;
}

void AppWindow::Destroy(bool postQuit) {
    postQuitOnDestroy_ = postQuit;
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (classRegistered_ && instance_) {
        UnregisterClassW(kWndClassName, instance_);
        classRegistered_ = false;
    }
    host_ = nullptr;
    dragging_ = false;
}

void AppWindow::ApplyRoundRegion(int fullW, int fullH, int winW, int winH, int cornerRadius) {
    if (!hwnd_) return;
    int w = winW;
    int h = winH;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (w > fullW) w = fullW;
    if (h > fullH) h = fullH;
    const int x = (fullW - w) / 2;
    const int y = (fullH - h) / 2;
    HRGN rgn = CreateRoundRectRgn(x, y, x + w + 1, y + h + 1, cornerRadius, cornerRadius);
    if (!rgn) {
        Log("CreateRoundRectRgn failed");
        return;
    }
    if (SetWindowRgn(hwnd_, rgn, TRUE) == 0) {
        DeleteObject(rgn);
        Log("SetWindowRgn failed: {}", GetLastError());
    }
}

void AppWindow::SetLayeredAlpha(BYTE alpha) {
    if (!hwnd_ || alpha == lastLayeredAlpha_) return;
    if (SetLayeredWindowAttributes(hwnd_, 0, alpha, LWA_ALPHA))
        lastLayeredAlpha_ = alpha;
    else
        Log("SetLayeredWindowAttributes failed: {}", GetLastError());
}

void AppWindow::BeginDrag() {
    if (!hwnd_ || (host_ && host_->IsDragDisabled())) return;
    dragging_ = true;
    SetCapture(hwnd_);
    GetCursorPos(&dragStart_);
    RECT r{};
    GetWindowRect(hwnd_, &r);
    dragOrigin_.x = r.left;
    dragOrigin_.y = r.top;
}

void AppWindow::DragTo() {
    if (!dragging_ || !hwnd_) return;
    POINT cur{};
    GetCursorPos(&cur);
    SetWindowPos(hwnd_, nullptr, dragOrigin_.x + (cur.x - dragStart_.x),
                 dragOrigin_.y + (cur.y - dragStart_.y), 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void AppWindow::EndDrag() {
    if (!dragging_) return;
    dragging_ = false;
    if (GetCapture() == hwnd_) ReleaseCapture();
}

LRESULT CALLBACK AppWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        auto* self = static_cast<AppWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    auto* self = reinterpret_cast<AppWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    AppWindowHost* host = self ? self->host_ : nullptr;

    if (host && host->ForwardRawMessage(hwnd, msg, wp, lp)) return 1;

    switch (msg) {
    case WM_LBUTTONDOWN:
        if (host && !host->IsDragDisabled() && !host->WantsMouseCapture()) host->OnBeginDrag();
        break;
    case WM_MOUSEMOVE:
        if (host) host->OnDragMove();
        break;
    case WM_LBUTTONUP:
        if (host) host->OnEndDrag();
        break;
    case WM_CAPTURECHANGED:
        if (self && reinterpret_cast<HWND>(lp) != hwnd) self->dragging_ = false;
        break;
    case WM_DPICHANGED: {
        const auto* suggested = reinterpret_cast<const RECT*>(lp);
        if (suggested && self) {
            self->dpi_ = HIWORD(wp);
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            if (host) {
                host->OnDpiChanged(self->dpi_, suggested->right - suggested->left,
                                   suggested->bottom - suggested->top);
            }
        }
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == SC_CLOSE) {
            if (host) host->OnRequestClose();
            return 0;
        }
        break;
    case WM_CLOSE:
        if (host) {
            host->OnRequestClose();
            return 0;
        }
        break;
    case WM_DESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        if (self && self->postQuitOnDestroy_) PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}
