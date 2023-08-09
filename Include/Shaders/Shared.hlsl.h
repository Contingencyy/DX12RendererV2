#pragma once

#if !defined(__HLSL_VERSION)
#define float4x4 Mat4x4
#define uint uint32_t
#define float3 Vec3
#endif

#define PBR_DIFFUSE_BRDF_LAMBERT	0
#define PBR_DIFFUSE_BRDF_BURLEY		1
#define PBR_DIFFUSE_BRDF_OREN_NAYAR 2
#define PBR_DIFFUSE_BRDF_NUM_TYPES  3

struct RenderSettings
{
	struct PBR
	{
		uint use_linear_perceptual_roughness;
		uint diffuse_brdf;
	} pbr;
};

struct SceneData
{
	float4x4 view;
	float4x4 projection;
	float4x4 view_projection;
	float3 view_pos;
};
