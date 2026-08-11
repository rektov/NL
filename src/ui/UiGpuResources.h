#pragma once
#include "GameCatalog.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace nl::gfx {
class Blur;
}

namespace nl::ui {

struct UiGpuResources {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> avatar;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texCs2;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texCsgo;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texValorant;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texApex;

    gfx::Blur* blur = nullptr;

    [[nodiscard]] ID3D11ShaderResourceView* ArtFor(GameArt art) const {
        switch (art) {
        case GameArt::Cs2:
            return texCs2.Get();
        case GameArt::Csgo:
            return texCsgo.Get();
        case GameArt::Valorant:
            return texValorant.Get();
        case GameArt::Apex:
            return texApex.Get();
        }
        return nullptr;
    }

    void Release() {
        avatar.Reset();
        texCs2.Reset();
        texCsgo.Reset();
        texValorant.Reset();
        texApex.Reset();
        blur = nullptr;
    }
};

void ReleaseTextures(UiGpuResources& gpu);
void LoadTextures(UiGpuResources& gpu, ID3D11Device* device, ID3D11DeviceContext* ctx);

} // namespace nl::ui
