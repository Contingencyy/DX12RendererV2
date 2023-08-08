struct PostProcessSettings
{
    uint hdr_texture_index;
    uint sdr_texture_index;
};

ConstantBuffer<PostProcessSettings> g_post_process_cb : register(b0);

float3 Uncharted2Tonemap(float3 color, float exposure, float gamma)
{
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    float W = 11.2f;

    color *= exposure;
    color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
    float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
    color /= white;
    color = pow(abs(color), (1.0f / gamma));

    return color;
}

float3 ReinhardRGB(float3 color)
{
    return color / (1.0 + color);
}

float3 ReinhardRGBWhite(float3 color, float max_white)
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

float3 ReinhardLuminanceWhite(float3 color, float max_white)
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
    float3 tonemapped_color = Uncharted2Tonemap(hdr_color.xyz, 1.5, 2.2);
    
    sdr_texture[dispatch_idx.xy].xyz = tonemapped_color;
    sdr_texture[dispatch_idx.xy].a = hdr_color.a;
}