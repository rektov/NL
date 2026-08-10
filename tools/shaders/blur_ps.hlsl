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
