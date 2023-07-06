#include "Shared.hlsl.h"

ConstantBuffer<SceneData> g_scene_cb : register(b0);

struct VertexLayout
{
	float3 pos : POSITION;
	float2 uv : TEXCOORD;
    float4x4 transform : TRANSFORM;
    uint base_color_texture : BASE_COLOR_TEXTURE;
};

struct VSOut
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
    uint base_color_texture : BASE_COLOR_TEXTURE;
};

VSOut VSMain(VertexLayout vertex)
{
	VSOut OUT;
	
    OUT.pos = mul(float4(vertex.pos, 1), vertex.transform);
    OUT.pos = mul(OUT.pos, g_scene_cb.view_projection);
	OUT.uv = vertex.uv;
    OUT.base_color_texture = vertex.base_color_texture;

	return OUT;
}

SamplerState g_samp_point_wrap : register(s0);

float4 PSMain(VSOut IN) : SV_TARGET
{
    Texture2D<float4> base_color_texture = ResourceDescriptorHeap[IN.base_color_texture];
    return base_color_texture.Sample(g_samp_point_wrap, IN.uv);
}
