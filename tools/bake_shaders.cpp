#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

static bool Bake(FILE* out, const char* name, const char* src, const char* target) {
    ID3DBlob* blob = nullptr;
    ID3DBlob* err = nullptr;
    HRESULT hr = D3DCompile(src, strlen(src), name, nullptr, nullptr, "main", target,
                            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &blob, &err);
    if (FAILED(hr) || !blob) {
        if (err) {
            fwrite(err->GetBufferPointer(), 1, err->GetBufferSize(), stderr);
            err->Release();
        }
        fprintf(stderr, "compile failed: %s\n", name);
        return false;
    }

    const auto* bytes = static_cast<const unsigned char*>(blob->GetBufferPointer());
    const size_t n = blob->GetBufferSize();
    fprintf(out, "static const unsigned char %s[] = {\n", name);
    for (size_t i = 0; i < n; ++i) {
        if ((i % 12) == 0) fputs("    ", out);
        fprintf(out, "0x%02X,", bytes[i]);
        if ((i % 12) == 11 || i + 1 == n) fputc('\n', out);
        else fputc(' ', out);
    }
    fprintf(out, "};\nstatic const size_t %sSize = %zu;\n\n", name, n);
    blob->Release();
    return true;
}

static const char kBlurVs[] = R"(
struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VS_OUT main(uint id : SV_VertexID) {
    float2 uv = float2((id << 1) & 2, id & 2);
    VS_OUT o;
    o.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0, 1);
    o.uv = uv;
    return o;
}
)";

static const char kBlurDownsample[] = R"(
Texture2D backTexture : register(t0);
SamplerState textureSampler : register(s0);
cbuffer BlurInputBuffer : register(b0) {
    float2 textureSize;
    float blurDirections;
    float blurQuality;
    float blurRadius;
};
float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_Target {
    float2 t = 1.0 / textureSize;
    float3 c = backTexture.Sample(textureSampler, uv + float2(-0.5, -0.5) * t).rgb;
    c += backTexture.Sample(textureSampler, uv + float2( 0.5, -0.5) * t).rgb;
    c += backTexture.Sample(textureSampler, uv + float2(-0.5,  0.5) * t).rgb;
    c += backTexture.Sample(textureSampler, uv + float2( 0.5,  0.5) * t).rgb;
    return float4(c * 0.25, 1.0);
}
)";

static const char kBlurPs[] = R"(
Texture2D backTexture : register(t0);
SamplerState textureSampler : register(s0);
cbuffer BlurInputBuffer : register(b0) {
    float2 textureSize;
    float blurDirections;
    float blurQuality;
    float blurRadius;
};
float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_Target {
    const float pi2 = 6.28318530718f;
    float2 radius = blurRadius / textureSize;
    float3 color = backTexture.Sample(textureSampler, uv).rgb;
    float increment = 1.0f / blurQuality;
    for (float d = 0.0f; d < pi2; d += pi2 / blurDirections)
    {
        for (float i = increment; i < 1.00001f; i += increment)
        {
            float2 blurUv = uv + float2(cos(d), sin(d)) * radius * i;
            color += backTexture.Sample(textureSampler, blurUv).rgb;
        }
    }
    color /= (blurQuality * blurDirections + 1.0f);
    return float4(color, 1.0);
}
)";

static const char kImguiVs[] =
    "cbuffer vertexBuffer : register(b0)"
    "{"
    "  float4x4 ProjectionMatrix;"
    "};"
    "struct VS_INPUT"
    "{"
    "  float2 pos : POSITION;"
    "  float4 col : COLOR0;"
    "  float2 uv  : TEXCOORD0;"
    "};"
    "struct PS_INPUT"
    "{"
    "  float4 pos : SV_POSITION;"
    "  float4 col : COLOR0;"
    "  float2 uv  : TEXCOORD0;"
    "};"
    "PS_INPUT main(VS_INPUT input)"
    "{"
    "  PS_INPUT output;"
    "  output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));"
    "  output.col = input.col;"
    "  output.uv  = input.uv;"
    "  return output;"
    "}";

static const char kImguiPs[] =
    "struct PS_INPUT"
    "{"
    "float4 pos : SV_POSITION;"
    "float4 col : COLOR0;"
    "float2 uv  : TEXCOORD0;"
    "};"
    "sampler sampler0;"
    "Texture2D texture0;"
    "float4 main(PS_INPUT input) : SV_Target"
    "{"
    "float4 out_col = input.col * texture0.Sample(sampler0, input.uv);"
    "return out_col;"
    "}";

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: bake_shaders <BlurShaders.h> <ImGuiShaders.h>\n");
        return 1;
    }

    FILE* blur = nullptr;
    if (fopen_s(&blur, argv[1], "wb") != 0 || !blur) return 1;
    fputs("#pragma once\n#include <cstddef>\n\nnamespace nl::gfx::shaders {\n\n", blur);
    bool ok = Bake(blur, "kVsBytecode", kBlurVs, "vs_5_0")
           && Bake(blur, "kDownsampleBytecode", kBlurDownsample, "ps_4_0")
           && Bake(blur, "kBlurBytecode", kBlurPs, "ps_4_0");
    fputs("}\n", blur);
    fclose(blur);
    if (!ok) return 1;

    FILE* imgui = nullptr;
    if (fopen_s(&imgui, argv[2], "wb") != 0 || !imgui) return 1;
    fputs("#pragma once\n#include <cstddef>\n\nnamespace nl::imgui_shaders {\n\n", imgui);
    ok = Bake(imgui, "kVsBytecode", kImguiVs, "vs_4_0")
      && Bake(imgui, "kPsBytecode", kImguiPs, "ps_4_0");
    fputs("}\n", imgui);
    fclose(imgui);
    return ok ? 0 : 1;
}
