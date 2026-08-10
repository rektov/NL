#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <span>

namespace nl {

enum class PresentResult {
    Ok,
    DeviceLost,
    Failed,
};

class AppDevice {
  public:
    AppDevice() = default;
    ~AppDevice() { Cleanup(); }
    AppDevice(const AppDevice&) = delete;
    AppDevice& operator=(const AppDevice&) = delete;

    [[nodiscard]] bool Init(HWND hwnd, int width, int height);
    void Cleanup();

    void CreateRenderTarget();
    void CleanupRenderTarget();
    [[nodiscard]] bool Resize(int width, int height);

    [[nodiscard]] PresentResult Present(UINT syncInterval = 1);
    void ClearAndBind(std::span<const float, 4> clearRgba);

    ID3D11Device* Device() const { return device_.Get(); }
    ID3D11DeviceContext* Context() const { return ctx_.Get(); }
    IDXGISwapChain* SwapChain() const { return swap_.Get(); }
    ID3D11RenderTargetView* Rtv() const { return rtv_.Get(); }
    int Width() const { return width_; }
    int Height() const { return height_; }
    bool Ready() const { return device_ && ctx_ && swap_ && rtv_; }

  private:
    void UnbindRenderTargets();

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
    int width_ = 0;
    int height_ = 0;
};

}
