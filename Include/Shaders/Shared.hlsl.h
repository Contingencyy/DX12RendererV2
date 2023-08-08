#pragma once

#if !defined(__HLSL_VERSION)
#define float4x4 Mat4x4
#define uint uint32_t
#define float3 Vec3
#endif

struct SceneData
{
	float4x4 view;
	float4x4 projection;
	float4x4 view_projection;
	float3 view_pos;
};
