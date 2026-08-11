#include "Blur.h"
#include "BlurShaders.h"
#include "../Log.h"
#include <algorithm>
#include <cmath>

namespace nl::gfx {
namespace {

using Microsoft::WRL::ComPtr;

struct alignas(16) BlurInputBuffer {
    float textureSize[2];
    float blurDirections;
    float blurQuality;
    float blurRadius;
    float pad[3]{};
};

} // namespace

bool Blur::CreateDeviceObjects(IDXGISwapChain* swap) {
    if (!device_ || !swap) {
        Log("Blur::CreateDeviceObjects: missing device/swap");
        return false;
    }
    DestroyDeviceObjects();
    swap_ = swap;

    ComPtr<ID3D11Texture2D> bb;
    HRESULT hr = swap_->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (FAILED(hr) || !bb) {
        LogHr("Blur GetBuffer", hr);
        return false;
    }

    D3D11_TEXTURE2D_DESC bbDesc{};
    bb->GetDesc(&bbDesc);

    texW_ = bbDesc.Width;
    texH_ = bbDesc.Height;
    halfW_ = (std::max)(1u, texW_ / 2);
    halfH_ = (std::max)(1u, texH_ / 2);

    if (!vs_) {
        hr = device_->CreateVertexShader(shaders::kVsBytecode, shaders::kVsBytecodeSize, nullptr,
                                         vs_.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            LogHr("Blur CreateVertexShader", hr);
            return false;
        }
    }
    if (!downsamplePs_) {
        hr = device_->CreatePixelShader(shaders::kDownsampleBytecode,
                                        shaders::kDownsampleBytecodeSize, nullptr,
                                        downsamplePs_.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            LogHr("Blur CreatePixelShader(downsample)", hr);
            return false;
        }
    }
    if (!blurPs_) {
        hr = device_->CreatePixelShader(shaders::kBlurBytecode, shaders::kBlurBytecodeSize, nullptr,
                                        blurPs_.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            LogHr("Blur CreatePixelShader(blur)", hr);
            return false;
        }
    }

    if (!blurCb_) {
        D3D11_BUFFER_DESC cbd{};
        cbd.ByteWidth = sizeof(BlurInputBuffer);
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device_->CreateBuffer(&cbd, nullptr, blurCb_.ReleaseAndGetAddressOf())))
            return false;
    }
    if (!sampler_) {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device_->CreateSamplerState(&sd, sampler_.ReleaseAndGetAddressOf())))
            return false;
    }
    if (!rast_) {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        if (FAILED(device_->CreateRasterizerState(&rd, rast_.ReleaseAndGetAddressOf())))
            return false;
    }
    if (!depth_) {
        D3D11_DEPTH_STENCIL_DESC dd{};
        if (FAILED(device_->CreateDepthStencilState(&dd, depth_.ReleaseAndGetAddressOf())))
            return false;
    }
    if (!blend_) {
        D3D11_BLEND_DESC bd{};
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if (FAILED(device_->CreateBlendState(&bd, blend_.ReleaseAndGetAddressOf()))) return false;
    }

    D3D11_TEXTURE2D_DESC td{};
    td.Width = texW_;
    td.Height = texH_;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device_->CreateTexture2D(&td, nullptr, captureTex_.ReleaseAndGetAddressOf())) ||
        FAILED(device_->CreateShaderResourceView(captureTex_.Get(), nullptr,
                                                 captureSrv_.ReleaseAndGetAddressOf()))) {
        DestroyDeviceObjects();
        return false;
    }

    auto makeHalf = [&](ComPtr<ID3D11Texture2D>& tex, ComPtr<ID3D11ShaderResourceView>& srv,
                        ComPtr<ID3D11RenderTargetView>& rtv) -> bool {
        D3D11_TEXTURE2D_DESC d = td;
        d.Width = halfW_;
        d.Height = halfH_;
        d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device_->CreateTexture2D(&d, nullptr, tex.ReleaseAndGetAddressOf())))
            return false;
        if (FAILED(device_->CreateShaderResourceView(tex.Get(), nullptr,
                                                     srv.ReleaseAndGetAddressOf())))
            return false;
        if (FAILED(
                device_->CreateRenderTargetView(tex.Get(), nullptr, rtv.ReleaseAndGetAddressOf())))
            return false;
        return true;
    };

    if (!makeHalf(halfTex_, halfSrv_, halfRtv_) || !makeHalf(effectTex_, effectSrv_, effectRtv_)) {
        DestroyDeviceObjects();
        return false;
    }

    if (ctx_ && effectRtv_) {
        const float clear[4] = {0.f, 0.01f, 0.01f, 1.f};
        ctx_->ClearRenderTargetView(effectRtv_.Get(), clear);
    }

    ready_ = true;
    return true;
}

void Blur::DestroyDeviceObjects() {
    effectRtv_.Reset();
    effectSrv_.Reset();
    effectTex_.Reset();
    halfRtv_.Reset();
    halfSrv_.Reset();
    halfTex_.Reset();
    captureSrv_.Reset();
    captureTex_.Reset();
    texW_ = texH_ = halfW_ = halfH_ = 0;
    ready_ = false;
    pending_ = false;
}

bool Blur::Init(ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swap) {
    device_ = device;
    ctx_ = ctx;
    return CreateDeviceObjects(swap);
}

void Blur::OnResize(IDXGISwapChain* swap) {
    if (!CreateDeviceObjects(swap)) {
        ready_ = false;
        texW_ = texH_ = halfW_ = halfH_ = 0;
        pending_ = false;
        cacheValid_ = false;
    } else {
        cacheValid_ = false;
    }
}

void Blur::Shutdown() {
    DestroyDeviceObjects();
    blend_.Reset();
    depth_.Reset();
    rast_.Reset();
    sampler_.Reset();
    blurCb_.Reset();
    blurPs_.Reset();
    downsamplePs_.Reset();
    vs_.Reset();
    device_ = nullptr;
    ctx_ = nullptr;
    swap_ = nullptr;
    pending_ = false;
    cacheValid_ = false;
    ready_ = false;
}

bool Blur::RunBlur() {
    if (!ctx_ || !swap_ || !captureTex_ || !effectRtv_ || !halfRtv_) return false;

    ComPtr<ID3D11RenderTargetView> oldRtv;
    ComPtr<ID3D11DepthStencilView> oldDsv;
    ctx_->OMGetRenderTargets(1, &oldRtv, &oldDsv);
    if (!oldRtv) return false;

    ID3D11ShaderResourceView* nullSrvs[4] = {};
    ctx_->PSSetShaderResources(0, 4, nullSrvs);

    ComPtr<ID3D11Resource> bbRes;
    oldRtv->GetResource(&bbRes);
    if (!bbRes) return false;

    D3D11_VIEWPORT oldVp{};
    UINT nvp = 1;
    ctx_->RSGetViewports(&nvp, &oldVp);

    {
        ComPtr<ID3D11Texture2D> bbTex;
        if (SUCCEEDED(bbRes.As(&bbTex)) && bbTex) {
            D3D11_TEXTURE2D_DESC bd{};
            bbTex->GetDesc(&bd);
            if (bd.Width != texW_ || bd.Height != texH_ ||
                bd.Format != DXGI_FORMAT_R8G8B8A8_UNORM) {
                ID3D11RenderTargetView* restoreRtv = oldRtv.Get();
                ctx_->OMSetRenderTargets(1, &restoreRtv, oldDsv.Get());
                if (nvp) ctx_->RSSetViewports(1, &oldVp);
                return false;
            }
        }
    }

    ctx_->OMSetRenderTargets(0, nullptr, nullptr);
    ctx_->CopyResource(captureTex_.Get(), bbRes.Get());

    const float clear[4] = {0.f, 0.01f, 0.01f, 1.f};
    ctx_->ClearRenderTargetView(halfRtv_.Get(), clear);
    ctx_->ClearRenderTargetView(effectRtv_.Get(), clear);

    auto setCb = [&](float tw, float th, float radius, float quality, float dirs) -> bool {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(ctx_->Map(blurCb_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return false;
        auto* p = reinterpret_cast<BlurInputBuffer*>(mapped.pData);
        p->textureSize[0] = tw;
        p->textureSize[1] = th;
        p->blurRadius = radius;
        p->blurQuality = quality;
        p->blurDirections = dirs;
        p->pad[0] = p->pad[1] = p->pad[2] = 0.f;
        ctx_->Unmap(blurCb_.Get(), 0);
        return true;
    };

    auto draw = [&](ID3D11RenderTargetView* rtv, ID3D11PixelShader* ps,
                    ID3D11ShaderResourceView* srv) {
        ID3D11ShaderResourceView* nulls[1] = {nullptr};
        ctx_->PSSetShaderResources(0, 1, nulls);
        ctx_->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT vp{};
        vp.Width = static_cast<float>(halfW_);
        vp.Height = static_cast<float>(halfH_);
        vp.MaxDepth = 1.f;
        ctx_->RSSetViewports(1, &vp);
        ctx_->PSSetShader(ps, nullptr, 0);
        ctx_->PSSetShaderResources(0, 1, &srv);
        ctx_->Draw(3, 0);
        ctx_->PSSetShaderResources(0, 1, nulls);
    };

    ID3D11SamplerState* sampler = sampler_.Get();
    ID3D11Buffer* cb = blurCb_.Get();
    ctx_->VSSetShader(vs_.Get(), nullptr, 0);
    ctx_->PSSetSamplers(0, 1, &sampler);
    ctx_->PSSetConstantBuffers(0, 1, &cb);
    ctx_->IASetInputLayout(nullptr);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->RSSetState(rast_.Get());
    ctx_->OMSetBlendState(blend_.Get(), nullptr, 0xffffffff);
    ctx_->OMSetDepthStencilState(depth_.Get(), 0);

    const bool ok = [&] {
        if (!setCb(static_cast<float>(texW_), static_cast<float>(texH_), 0.f, 1.f, 1.f))
            return false;
        draw(halfRtv_.Get(), downsamplePs_.Get(), captureSrv_.Get());

        if (!setCb(static_cast<float>(halfW_), static_cast<float>(halfH_), 12.f, 4.f, 16.f))
            return false;
        draw(effectRtv_.Get(), blurPs_.Get(), halfSrv_.Get());
        return true;
    }();

    ID3D11ShaderResourceView* nulls[1] = {nullptr};
    ID3D11RenderTargetView* restoreRtv = oldRtv.Get();
    ctx_->PSSetShaderResources(0, 1, nulls);
    ctx_->OMSetRenderTargets(1, &restoreRtv, oldDsv.Get());
    if (nvp) ctx_->RSSetViewports(1, &oldVp);
    return ok;
}

bool Blur::EnsureSize() {
    if (!swap_) return effectSrv_ != nullptr;
    ComPtr<ID3D11Texture2D> cur;
    if (SUCCEEDED(swap_->GetBuffer(0, IID_PPV_ARGS(&cur))) && cur) {
        D3D11_TEXTURE2D_DESC d{};
        cur->GetDesc(&d);
        if (d.Width != texW_ || d.Height != texH_) return CreateDeviceObjects(swap_);
    }
    return effectSrv_ != nullptr;
}

bool Blur::QueueCapture(bool dirty) {
    if (!ready_ || !EnsureSize() || !effectSrv_) return false;
    if (!dirty && cacheValid_) return true;
    pending_ = true;
    return true;
}

void Blur::ExecutePending() {
    if (!ready_ || !pending_) return;
    const bool ok = RunBlur();
    pending_ = false;
    cacheValid_ = ok && effectSrv_ != nullptr;
}

} // namespace nl::gfx
