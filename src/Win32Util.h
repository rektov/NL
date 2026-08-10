#pragma once
#include <Windows.h>
#include <shellapi.h>

namespace nl {

struct MonitorCenter {
    int x = 0;
    int y = 0;
};

[[nodiscard]] inline MonitorCenter CenterOnNearestMonitor(int w, int h, HWND anchor = nullptr) {
    HMONITOR mon = anchor ? MonitorFromWindow(anchor, MONITOR_DEFAULTTONEAREST)
                          : MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) {
        return {(GetSystemMetrics(SM_CXSCREEN) - w) / 2, (GetSystemMetrics(SM_CYSCREEN) - h) / 2};
    }
    const int mw = mi.rcWork.right - mi.rcWork.left;
    const int mh = mi.rcWork.bottom - mi.rcWork.top;
    return {mi.rcWork.left + (mw - w) / 2, mi.rcWork.top + (mh - h) / 2};
}

template <class Fn> [[nodiscard]] Fn GetProcAddressAs(HMODULE mod, const char* name) {
    return reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(mod, name)));
}

[[nodiscard]] inline UINT DpiForWindowOrDefault(HWND hwnd) {
    using GetDpiFn = UINT(WINAPI*)(HWND);
    static GetDpiFn fn = []() -> GetDpiFn {
        if (HMODULE user32 = GetModuleHandleW(L"user32.dll"))
            return GetProcAddressAs<GetDpiFn>(user32, "GetDpiForWindow");
        return nullptr;
    }();
    if (fn && hwnd) {
        const UINT dpi = fn(hwnd);
        if (dpi) return dpi;
    }
    return 96;
}

[[nodiscard]] inline bool OpenHttpsUrl(const wchar_t* url) {
    if (!url || !url[0]) return false;
    if (wcsncmp(url, L"https://", 8) != 0 && wcsncmp(url, L"http://", 7) != 0) return false;
    const HINSTANCE hi = ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(hi) > 32;
}

}
