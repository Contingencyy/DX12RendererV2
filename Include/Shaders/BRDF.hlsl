#pragma once
#include "Common.hlsl"

// Normal distribution function
float D_GGX(float NoH, float a)
{
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

// Fresnel, determines reflectivity based on view angle
float3 F_Schlick(float u, float3 f0)
{
    return f0 + (1.0, 1.0, 1.0 - f0) * pow(1.0 - u, 5.0);
}

// Visibility of the light, based on the roughness of the surface
// Takes into account geometric self-shadowing of the surface
float V_SmithGGXCorrelated(float NoV, float NoL, float a)
{
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL);
}

// Basic lambert diffuse
float Fd_Lambert()
{
    return 1.0 / PI;
}

void EvaluateBRDF(float3 view_dir, float3 light_dir, float3 base_color, float3 normal,
    float metallic, float roughness, out float3 brdf_specular, out float3 brdf_diffuse)
{
    float3 f0 = float3(0.04, 0.04, 0.04);
    f0 = lerp(f0, base_color, metallic);
    
    float3 half_vec = normalize(view_dir + light_dir);
    
    float NoV = max(0, dot(normal, view_dir));
    float NoL = max(0, dot(normal, light_dir));
    float NoH = max(0, dot(normal, half_vec));
    float LoH = max(0, dot(light_dir, half_vec));
    
    // We remap the roughness to a more linear perceptual roughness
    float roughness_sq = roughness * roughness;
    
    float D = D_GGX(NoH, roughness_sq);
    float3 F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, roughness_sq);
    
    // Specular BRDF (Fr)
    brdf_specular = (D * V) * F;
    
    // Diffuse color, Diffuse BRDF (Fd)
    float3 diffuse_color = (1.0 - F) * (1.0 - metallic) * base_color;
    brdf_diffuse = diffuse_color * Fd_Lambert();
}
