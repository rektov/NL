#include "Texture.h"
#include "Log.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <memory>

namespace nl::gfx {
namespace {

using Microsoft::WRL::ComPtr;

struct StbiFree {
    void operator()(unsigned char* p) const {
        if (p) stbi_image_free(p);
    }
};
using StbiImage = std::unique_ptr<unsigned char, StbiFree>;

StbiImage DecodeRgba(const void* data, std::size_t dataSize, int& w, int& h, const char* who) {
    if (!data || dataSize == 0 || dataSize > static_cast<std::size_t>(INT_MAX)) {
        Log("{}: invalid image buffer", who);
        return nullptr;
    }
    int channels = 0;
    StbiImage img(stbi_load_from_memory(static_cast<const stbi_uc*>(data),
                                        static_cast<int>(dataSize), &w, &h, &channels,
                                        STBI_rgb_alpha));
    if (!img || w <= 0 || h <= 0) Log("{}: stbi_load_from_memory failed ({} bytes)", who, dataSize);
    return img;
}

}

bool LoadTextureFromMemory(const void* data, std::size_t dataSize, ID3D11Device* device,
                           ComPtr<ID3D11ShaderResourceView>& outSrv, TextureDims* outDims) {
    outSrv.Reset();
    if (!device) {
        Log("LoadTextureFromMemory: null device");
        return false;
    }

    int width = 0, height = 0;
    StbiImage imageData = DecodeRgba(data, dataSize, width, height, "LoadTextureFromMemory");
    if (!imageData) return false;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subResource{};
    subResource.pSysMem = imageData.get();
    subResource.SysMemPitch = static_cast<UINT>(width * 4);

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&desc, &subResource, texture.GetAddressOf());
    if (FAILED(hr)) {
        LogHr("LoadTextureFromMemory CreateTexture2D", hr);
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(texture.Get(), &srvDesc, outSrv.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        LogHr("LoadTextureFromMemory CreateShaderResourceView", hr);
        outSrv.Reset();
        return false;
    }

    if (outDims) *outDims = {width, height};
    return true;
}

bool LoadCircularTextureFromMemory(const void* data, std::size_t dataSize, ID3D11Device* device,
                                   ID3D11DeviceContext* context,
                                   ComPtr<ID3D11ShaderResourceView>& outSrv, TextureDims* outDims) {
    outSrv.Reset();
    if (!device || !context) {
        Log("LoadCircularTextureFromMemory: null device/context");
        return false;
    }

    int width = 0, height = 0;
    StbiImage imageData =
        DecodeRgba(data, dataSize, width, height, "LoadCircularTextureFromMemory");
    if (!imageData) return false;

    const int radius = std::min(width, height) / 2;
    const int centerX = width / 2;
    const int centerY = height / 2;

    const int radiusSq = radius * radius;
    const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    auto circularData = std::make_unique_for_overwrite<unsigned char[]>(byteCount);
    const unsigned char* src = imageData.get();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int idx = (y * width + x) * 4;
            const int dx = x - centerX;
            const int dy = y - centerY;
            const bool inside = dx * dx + dy * dy <= radiusSq;
            circularData[idx + 0] = inside ? src[idx + 0] : 0;
            circularData[idx + 1] = inside ? src[idx + 1] : 0;
            circularData[idx + 2] = inside ? src[idx + 2] : 0;
            circularData[idx + 3] = inside ? 255 : 0;
        }
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 0;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
    if (FAILED(hr)) {
        LogHr("LoadCircularTextureFromMemory CreateTexture2D", hr);
        return false;
    }

    context->UpdateSubresource(texture.Get(), 0, nullptr, circularData.get(),
                               static_cast<UINT>(width * 4), 0);
    hr = device->CreateShaderResourceView(texture.Get(), nullptr, outSrv.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        LogHr("LoadCircularTextureFromMemory CreateShaderResourceView", hr);
        outSrv.Reset();
        return false;
    }

    context->GenerateMips(outSrv.Get());

    if (outDims) *outDims = {width, height};
    return true;
}

}
