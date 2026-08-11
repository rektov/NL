#pragma once

#include <d3d11.h>
#include <wrl/client.h>

namespace nl::gfx {

class Blur {
  public:
    Blur() = default;
    ~Blur() { Shutdown(); }
    Blur(const Blur&) = delete;
    Blur& operator=(const Blur&) = delete;
    Blur(Blur&&) = delete;
    Blur& operator=(Blur&&) = delete;

    [[nodiscard]] bool Init(ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swap);
    void Shutdown();
    void OnResize(IDXGISwapChain* swap);

    [[nodiscard]] bool QueueCapture(bool dirty = true);
    void ExecutePending();

    [[nodiscard]] bool Ready() const { return ready_ && effectSrv_; }
    [[nodiscard]] bool HasEffect() const { return effectSrv_ != nullptr; }
    [[nodiscard]] bool IsCapturePending() const { return pending_; }
    [[nodiscard]] ID3D11ShaderResourceView* EffectSrv() const { return effectSrv_.Get(); }
    void InvalidateCache() { cacheValid_ = false; }

  private:
    bool EnsureSize();
    bool CreateDeviceObjects(IDXGISwapChain* swap);
    void DestroyDeviceObjects();
    [[nodiscard]] bool RunBlur();

    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* ctx_ = nullptr;
    IDXGISwapChain* swap_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> downsamplePs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> blurPs_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> blurCb_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rast_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depth_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> captureTex_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> captureSrv_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> halfTex_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> halfSrv_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> halfRtv_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> effectTex_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> effectSrv_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> effectRtv_;

    UINT texW_ = 0, texH_ = 0;
    UINT halfW_ = 0, halfH_ = 0;

    bool ready_ = false;
    bool pending_ = false;
    bool cacheValid_ = false;
};

} // namespace nl::gfx
