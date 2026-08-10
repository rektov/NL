#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstddef>

namespace nl::gfx {

struct TextureDims {
    int width = 0;
    int height = 0;
};

[[nodiscard]] bool LoadTextureFromMemory(const void* data, std::size_t dataSize,
                                         ID3D11Device* device,
                                         Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSrv,
                                         TextureDims* outDims = nullptr);

[[nodiscard]] bool LoadCircularTextureFromMemory(
    const void* data, std::size_t dataSize, ID3D11Device* device, ID3D11DeviceContext* context,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSrv, TextureDims* outDims = nullptr);

}
