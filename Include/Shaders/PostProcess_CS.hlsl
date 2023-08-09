#include "Common.hlsl"

struct PostProcessSettings
{
    uint hdr_texture_index;
    uint sdr_texture_index;
};

ConstantBuffer<PostProcessSettings> g_post_process_cb : register(b0, space1);

float3 ApplyExposure(float3 color, float exposure)
{
    return color * exposure;
}

float3 ApplyGammaCorrection(float3 color, float gamma)
{
    return pow(abs(color), (1.0 / gamma));
}

float3 TonemapUncharted2(float3 color)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    
    return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

float3 TonemapReinhardRGB(float3 color)
{
    return color / (1.0 + color);
}

float3 TonemapReinhardRGBWhite(float3 color, float max_white)
{
    float max_white_sq = max_white * max_white;
    float3 numerator = color * (1.0 + (color / float3(max_white_sq, max_white_sq, max_white_sq)));
    return numerator / (1.0 + color);
}

float Luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float3 ChangeLuminance(float3 color, float3 luma_out)
{
    float luma_in = Luminance(color);
    return color * (luma_out / luma_in);
}

float3 TonemapReinhardLuminanceWhite(float3 color, float max_white)
{
    float luma_old = Luminance(color);
    float numerator = luma_old * (1.0 + (luma_old / (max_white * max_white)));
    float luma_new = numerator / (1.0 + luma_old);
    
    return ChangeLuminance(color, luma_new);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_idx : SV_DispatchThreadID)
{
    Texture2D<float4> hdr_texture = ResourceDescriptorHeap[NonUniformResourceIndex(g_post_process_cb.hdr_texture_index)];
    RWTexture2D<float4> sdr_texture = ResourceDescriptorHeap[NonUniformResourceIndex(g_post_process_cb.sdr_texture_index)];
    
    float4 hdr_color = hdr_texture[dispatch_idx.xy];
    float3 final_color = hdr_color.xyz;
    final_color = ApplyExposure(final_color, g_settings.post_process.exposure);
    
    switch (g_settings.post_process.tonemap_operator)
    {
        case TONEMAP_OP_REINHARD_RGB:
        {
            final_color = TonemapReinhardRGB(final_color);
        } break;
        case TONEMAP_OP_REINHARD_RGB_WHITE:
        {
            final_color = TonemapReinhardRGBWhite(final_color, g_settings.post_process.max_white);
        } break;
        case TONEMAP_OP_REINHARD_LUM_WHITE:
        {
            final_color = TonemapReinhardLuminanceWhite(final_color, g_settings.post_process.max_white);
        } break;
        case TONEMAP_OP_UNCHARTED2:
        {
            final_color = TonemapUncharted2(final_color);
        } break;
    }
    
    final_color = ApplyGammaCorrection(final_color, g_settings.post_process.gamma);
    
    sdr_texture[dispatch_idx.xy].xyz = final_color;
    sdr_texture[dispatch_idx.xy].a = hdr_color.a;
}