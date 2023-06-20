struct SceneData
{
    float4x4 view_projection;
};

ConstantBuffer<SceneData> g_scene_cb : register(b0);

struct VertexLayout
{
	float3 pos : POSITION;
	float2 uv : TEXCOORD;
};

struct VSOut
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

VSOut VSMain(VertexLayout vertex)
{
	VSOut OUT;
	
    OUT.pos = mul(g_scene_cb.view_projection, float4(vertex.pos, 1));
	OUT.uv = vertex.uv;

	return OUT;
}

Texture2D<float4> g_tex : register(s0);
SamplerState g_samp_point_wrap : register(s0);

float4 PSMain(VSOut IN) : SV_TARGET
{
	return g_tex.Sample(g_samp_point_wrap, IN.uv);
}
