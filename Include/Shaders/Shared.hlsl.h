#pragma once

#if !defined(__HLSL_VERSION)
#define float4x4 Mat4x4
#define uint uint32_t
#endif

struct SceneData
{
	float4x4 view;
	float4x4 projection;
	float4x4 view_projection;
};
