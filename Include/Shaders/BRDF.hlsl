#pragma once
#include "Common.hlsl"

// Normal distribution function, determines how many normals are oriented towards our view
float D_GGX(float NoH, float a)
{
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

// Fresnel, determines reflectivity based on view/incidence angle
float3 F_Schlick(float u, float3 f0)
{
    return f0 + (1.0, 1.0, 1.0 - f0) * pow(1.0 - u, 5.0);
}

float F_Schlick90(float u, float f0, float f90)
{
    return f0 + (f90 - f0) * pow(1.0 - u, 5.0);
}

// Visibility of the light, based on the roughness of the surface
// Takes into account geometric shadowing and masking of microfacets
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

// Disney's Burley diffuse, leads to retro-reflections at grazing angles
float Fd_Burley(float NoV, float NoL, float LoH, float roughness)
{
    float f90 = 0.5 + 2.0 * roughness * LoH * LoH;
    float light_scatter = F_Schlick90(NoL, 1.0, f90);
    float view_scatter = F_Schlick90(NoV, 1.0, f90);
    return light_scatter * view_scatter * (1.0 / PI);
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
    
    // Remap roughness to a perceptual more linear roughness
    if (g_settings.pbr.use_linear_perceptual_roughness)
    {
        roughness *= roughness;
    }
    
    float D = D_GGX(NoH, roughness);
    float3 F = F_Schlick(LoH, f0);
    float V = V_SmithGGXCorrelated(NoV, NoL, roughness);
    
    // Specular BRDF (Fr)
    brdf_specular = (D * V) * F;
    
    // Diffuse color, Diffuse BRDF (Fd)
    float3 diffuse_color = (1.0 - F) * (1.0 - metallic) * base_color;
    switch (g_settings.pbr.diffuse_brdf)
    {
        case PBR_DIFFUSE_BRDF_LAMBERT:
        {
            brdf_diffuse = diffuse_color * Fd_Lambert();
        } break;
        case PBR_DIFFUSE_BRDF_BURLEY:
        {
            brdf_diffuse = diffuse_color * Fd_Burley(NoV, NoL, LoH, roughness);
        } break;
        case PBR_DIFFUSE_BRDF_OREN_NAYAR:
        {
            // TODO
        } break;
    }
}
