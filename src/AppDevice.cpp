#include "AppDevice.h"
#include "Log.h"
#include <dxgi.h>
#include <iterator>

namespace nl {

void AppDevice::UnbindRenderTargets() {
    if (!ctx_) return;
    ID3D11RenderTargetView* nullRtv = nullptr;
    ctx_->OMSetRenderTargets(0, &nullRtv, nullptr);
}

void AppDevice::CreateRenderTarget() {
    if (!swap_ || !device_) return;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> bb;
    HRESULT hr = swap_->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (FAILED(hr) || !bb) {
        LogHr("SwapChain::GetBuffer", hr);
        return;
    }
    rtv_.Reset();
    hr = device_->CreateRenderTargetView(bb.Get(), nullptr, &rtv_);
    if (FAILED(hr)) {
        LogHr("CreateRenderTargetView", hr);
        rtv_.Reset();
    }
}

void AppDevice::CleanupRenderTarget() {
    UnbindRenderTargets();
    rtv_.Reset();
}

bool AppDevice::Init(HWND hwnd, int width, int height) {
    width_ = width;
    height_ = height;

    auto tryCreate = [&](DXGI_SWAP_EFFECT effect) -> HRESULT {
        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = static_cast<UINT>(width);
        sd.BufferDesc.Height = static_cast<UINT>(height);
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = effect;

        D3D_FEATURE_LEVEL flOut{};
        const D3D_FEATURE_LEVEL fl[] = {
            D3D_FEATURE_LEVEL_11_0,
        };

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, fl,
            static_cast<UINT>(std::size(fl)), D3D11_SDK_VERSION, &sd,
            swap_.ReleaseAndGetAddressOf(), device_.ReleaseAndGetAddressOf(), &flOut,
            ctx_.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            Log("Hardware D3D11 init failed (effect={}), trying WARP", static_cast<int>(effect));
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, fl,
                static_cast<UINT>(std::size(fl)), D3D11_SDK_VERSION, &sd,
                swap_.ReleaseAndGetAddressOf(), device_.ReleaseAndGetAddressOf(), &flOut,
                ctx_.ReleaseAndGetAddressOf());
        }
        return hr;
    };

    HRESULT hr = tryCreate(DXGI_SWAP_EFFECT_FLIP_DISCARD);
    if (FAILED(hr)) {
        Log("FLIP_DISCARD unavailable, falling back to DISCARD");
        hr = tryCreate(DXGI_SWAP_EFFECT_DISCARD);
    }
    if (FAILED(hr)) {
        LogHr("D3D11CreateDeviceAndSwapChain", hr);
        return false;
    }
    CreateRenderTarget();
    return rtv_ != nullptr;
}

bool AppDevice::Resize(int width, int height) {
    if (!swap_ || width < 1 || height < 1) return false;
    if (width == width_ && height == height_ && rtv_) return true;

    CleanupRenderTarget();
    const HRESULT hr = swap_->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height),
                                            DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        LogHr("SwapChain::ResizeBuffers", hr);
        CreateRenderTarget();
        return false;
    }
    width_ = width;
    height_ = height;
    CreateRenderTarget();
    return rtv_ != nullptr;
}

void AppDevice::Cleanup() {
    CleanupRenderTarget();
    swap_.Reset();
    ctx_.Reset();
    device_.Reset();
    width_ = height_ = 0;
}

void AppDevice::ClearAndBind(std::span<const float, 4> clearRgba) {
    if (!ctx_ || !rtv_) return;
    ID3D11RenderTargetView* rtvPtr = rtv_.Get();
    ctx_->OMSetRenderTargets(1, &rtvPtr, nullptr);
    ctx_->ClearRenderTargetView(rtvPtr, clearRgba.data());
}

PresentResult AppDevice::Present(UINT syncInterval) {
    if (!swap_ || !device_) return PresentResult::Failed;
    const HRESULT hr = swap_->Present(syncInterval, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        LogHr("Present device lost",
              hr == DXGI_ERROR_DEVICE_REMOVED ? device_->GetDeviceRemovedReason() : hr);
        return PresentResult::DeviceLost;
    }
    if (FAILED(hr)) {
        LogHr("Present", hr);
        return PresentResult::Failed;
    }
    return PresentResult::Ok;
}

} // namespace nl
