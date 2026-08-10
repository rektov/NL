#include "App.h"
#include "Win32Util.h"
#include <Windows.h>

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE) - 4)
#endif
#ifndef PROCESS_PER_MONITOR_DPI_AWARE
#define PROCESS_PER_MONITOR_DPI_AWARE 2
#endif

namespace {

void EnableDpiAwareness() {
    if (const HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        using SetCtxFn = BOOL(WINAPI*)(HANDLE);
        const auto setCtx =
            nl::GetProcAddressAs<SetCtxFn>(user32, "SetProcessDpiAwarenessContext");
        if (setCtx && setCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) return;
    }

    if (const HMODULE shcore = LoadLibraryW(L"Shcore.dll")) {
        using SetAwareFn = HRESULT(WINAPI*)(int);
        const auto setAware = nl::GetProcAddressAs<SetAwareFn>(shcore, "SetProcessDpiAwareness");
        const bool ok = setAware && SUCCEEDED(setAware(PROCESS_PER_MONITOR_DPI_AWARE));
        FreeLibrary(shcore);
        if (ok) return;
    }

    SetProcessDPIAware();
}

}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    EnableDpiAwareness();

    nl::App app;
    if (!app.Init(hInstance)) {
        MessageBoxW(nullptr, L"Failed to initialize NL Loader", L"Error", MB_ICONERROR);
        app.Shutdown();
        return 1;
    }
    app.Run();
    app.Shutdown();
    return 0;
}
