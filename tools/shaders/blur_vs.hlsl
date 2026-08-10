struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VS_OUT main(uint id : SV_VertexID) {
    float2 uv = float2((id << 1) & 2, id & 2);
    VS_OUT o;
    o.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0, 1);
    o.uv = uv;
    return o;
}
