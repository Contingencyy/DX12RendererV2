#include "Shared.hlsl.h"
#include "BRDF.hlsl"

ConstantBuffer<SceneData> g_scene_cb : register(b0);

struct VertexLayout
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float4x4 transform : TRANSFORM;
    uint base_color_texture : BASE_COLOR_TEXTURE;
    uint normal_texture : NORMAL_TEXTURE;
    uint metallic_roughness_texture : METALLIC_ROUGHNESS_TEXTURE;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 world_pos : WORLD_POSITION;
    float3 world_normal : NORMAL;
    float4 world_tangent : TANGENT;
    float3 world_bitangent : BITANGENT;
    uint base_color_texture : BASE_COLOR_TEXTURE;
    uint normal_texture : NORMAL_TEXTURE;
    uint metallic_roughness_texture : METALLIC_ROUGHNESS_TEXTURE;
    float3x3 world_transform_no_translation : WORLD_TRANSFORM_NO_TRANSLATION;
};

VSOut VSMain(VertexLayout vertex)
{
	VSOut OUT;
	
    float3x3 world_transform_no_translation = float3x3(
        vertex.transform[0].xyz, vertex.transform[1].xyz, vertex.transform[2].xyz
    );
    
    OUT.world_pos = mul(float4(vertex.pos, 1), vertex.transform);
    OUT.pos = mul(OUT.world_pos, g_scene_cb.view_projection);
	OUT.uv = vertex.uv;
    OUT.world_normal = mul(world_transform_no_translation, vertex.normal);
    OUT.world_tangent.xyz = mul(world_transform_no_translation, vertex.tangent.xyz);
    OUT.world_tangent.w = vertex.tangent.w;
    OUT.world_bitangent = cross(OUT.world_normal, OUT.world_tangent.xyz) * OUT.world_tangent.w;
    OUT.base_color_texture = vertex.base_color_texture;
    OUT.normal_texture = vertex.normal_texture;
    OUT.metallic_roughness_texture = vertex.metallic_roughness_texture;
    OUT.world_transform_no_translation = world_transform_no_translation;

	return OUT;
}

SamplerState g_samp_point_wrap : register(s0);

//[earlydepthstencil]
float4 PSMain(VSOut IN) : SV_TARGET
{
    Texture2D<float4> base_color_texture = ResourceDescriptorHeap[NonUniformResourceIndex(IN.base_color_texture)];
    Texture2D<float4> normal_texture = ResourceDescriptorHeap[NonUniformResourceIndex(IN.normal_texture)];
    Texture2D<float4> metallic_roughness_texture = ResourceDescriptorHeap[NonUniformResourceIndex(IN.metallic_roughness_texture)];
    
    float4 base_color = base_color_texture.Sample(g_samp_point_wrap, IN.uv);
    float3 normal = normal_texture.Sample(g_samp_point_wrap, IN.uv).rgb;
    float2 metallic_roughness = metallic_roughness_texture.Sample(g_samp_point_wrap, IN.uv).bg;
    
    // Calculate the bitangent, and create the rotation matrix to transform the sampled normal from tangent to world space
    float3x3 TBN = transpose(float3x3(IN.world_tangent.xyz, IN.world_bitangent, IN.world_normal));
    normal = normal * 2.0 - 1.0;
    normal = normalize(mul(TBN, normal));
    
    float3 view_pos = float3(g_scene_cb.view[0].w, g_scene_cb.view[1].w, g_scene_cb.view[2].w);
    float3 view_dir = normalize(view_pos - IN.world_pos.xyz);
    float3 light_dir = normalize(float3(0.0, 200.0, 0.0) - IN.world_pos.xyz);
    float3 dist_to_light = length(float3(0.0, 200.0, 0.0) - IN.world_pos.xyz);
    
    float4 final_color = float4(0.0, 0.0, 0.0, base_color.a);
    float3 attenuation = float3(1.0, 0.007, 0.0002);
    float3 radiance = clamp(1.0 / (attenuation.x + (attenuation.y * dist_to_light) + (attenuation.z * (dist_to_light * dist_to_light))), 0.0, 1.0);
    float NoL = clamp(dot(normal, light_dir), 0.0, 1.0);
    float3 brdf = EvaluateBRDF(view_pos, light_dir, base_color.rgb, normal, metallic_roughness.x, metallic_roughness.y);
    final_color.xyz = brdf * NoL * radiance * float3(20.0, 20.0, 20.0);
    
    return final_color;
}
