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
