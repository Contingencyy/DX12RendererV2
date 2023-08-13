#pragma once

#if !defined(__HLSL_VERSION)
#define float4x4 Mat4x4
#define uint uint32_t
#define float2 Vec2
#define float3 Vec3
#define float4 Vec4
#define CPP_HLSL_STRUCT(x) struct alignas(16) x
#else
#define CPP_HLSL_STRUCT(x) struct x
#endif

#define PBR_DIFFUSE_BRDF_LAMBERT	0
#define PBR_DIFFUSE_BRDF_BURLEY		1
#define PBR_DIFFUSE_BRDF_OREN_NAYAR 2
#define PBR_DIFFUSE_BRDF_NUM_TYPES  3

#define TONEMAP_OP_REINHARD_RGB 0
#define TONEMAP_OP_REINHARD_RGB_WHITE 1
#define TONEMAP_OP_REINHARD_LUM_WHITE 2
#define TONEMAP_OP_UNCHARTED2 3
#define TONEMAP_OP_NUM_TYPES 4

struct RenderSettings
{
	CPP_HLSL_STRUCT(PBR)
	{
		uint use_linear_perceptual_roughness;
		uint diffuse_brdf;
	} pbr;

	CPP_HLSL_STRUCT(PostProcess)
	{
		uint tonemap_operator;
		float gamma;
		float exposure;
		float max_white;
	} post_process;
};

struct SceneData
{
	float4x4 view;
	float4x4 projection;
	float4x4 view_projection;
	float3 view_pos;
};
