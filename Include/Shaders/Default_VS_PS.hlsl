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
    float metallic_factor : METALLIC_FACTOR;
    float roughness_factor : ROUGHNESS_FACTOR;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 world_pos : WORLD_POSITION;
    float3 world_normal : NORMAL;
    float3 world_tangent : TANGENT;
    float3 world_bitangent : BITANGENT;
    uint base_color_texture : BASE_COLOR_TEXTURE;
    uint normal_texture : NORMAL_TEXTURE;
    uint metallic_roughness_texture : METALLIC_ROUGHNESS_TEXTURE;
    float metallic_factor : METALLIC_FACTOR;
    float roughness_factor : ROUGHNESS_FACTOR;
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
    OUT.world_normal = normalize(mul(vertex.normal, world_transform_no_translation));
    OUT.world_tangent = normalize(mul(vertex.tangent.xyz, world_transform_no_translation));
    OUT.world_bitangent = normalize(cross(OUT.world_normal, OUT.world_tangent.xyz)) * (-vertex.tangent.w);
    OUT.base_color_texture = vertex.base_color_texture;
    OUT.normal_texture = vertex.normal_texture;
    OUT.metallic_roughness_texture = vertex.metallic_roughness_texture;
    OUT.metallic_factor = vertex.metallic_factor;
    OUT.roughness_factor = vertex.roughness_factor;

	return OUT;
}

SamplerState g_samp_linear_wrap : register(s0);

//[earlydepthstencil]
float4 PSMain(VSOut IN) : SV_TARGET
{
    Texture2D<float4> base_color_texture = ResourceDescriptorHeap[NonUniformResourceIndex(IN.base_color_texture)];
    Texture2D<float4> normal_texture = ResourceDescriptorHeap[NonUniformResourceIndex(IN.normal_texture)];
    Texture2D<float4> metallic_roughness_texture = ResourceDescriptorHeap[NonUniformResourceIndex(IN.metallic_roughness_texture)];
    
    float4 base_color = base_color_texture.Sample(g_samp_linear_wrap, IN.uv);
    float3 normal = normal_texture.Sample(g_samp_linear_wrap, IN.uv).rgb;
    float2 metallic_roughness = metallic_roughness_texture.Sample(g_samp_linear_wrap, IN.uv).bg;
    
    // Calculate the bitangent, and create the rotation matrix to transform the sampled normal from tangent to world space
    float3x3 TBN = float3x3(IN.world_tangent, IN.world_bitangent, IN.world_normal);
    normal = normal * 2.0 - 1.0;
    normal = normalize(mul(normal, TBN));
    
    metallic_roughness.x *= IN.metallic_factor;
    metallic_roughness.y *= IN.roughness_factor;
    
    float3 view_pos = g_scene_cb.view_pos;
    float3 view_dir = normalize(view_pos - IN.world_pos.xyz);
    float3 frag_to_light = normalize(float3(0.0, 1.0, 0.0) - IN.world_pos.xyz);
    float3 dist_to_light = length(float3(0.0, 1.0, 0.0) - IN.world_pos.xyz);
    
    float4 final_color = float4(0.0, 0.0, 0.0, base_color.a);
    
    float3 light_color = float3(2.0, 2.0, 2.0);
    float3 falloff = float3(1.0, 0.007, 0.0002);
    float3 attenuation = clamp(1.0 / (falloff.x + (falloff.y * dist_to_light) + (falloff.z * (dist_to_light * dist_to_light))), 0.0, 1.0);
    float3 radiance = attenuation * light_color;
    float NoL = clamp(dot(normal, frag_to_light), 0.0, 1.0);
    
    float3 brdf_specular, brdf_diffuse;
    EvaluateBRDF(view_dir, frag_to_light, base_color.rgb, normal, metallic_roughness.x, metallic_roughness.y, brdf_specular, brdf_diffuse);
    
    // Incident light is determined by the light color, distance attenuation and the angle of incidence (NoL)
    float3 incident_light = radiance * NoL;
    
    final_color.rgb = brdf_specular * incident_light + brdf_diffuse * incident_light;
    //final_color.rgb = normal;
    
    return final_color;
}
