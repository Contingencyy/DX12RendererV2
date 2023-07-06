#include "Pch.h"
#include "FileIO.h"
#include "Renderer/Renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#include "cgltf/cgltf.h"

namespace FileIO
{

    LoadImageResult LoadImage(const char* filepath)
    {
        LoadImageResult result = {};
        
        int width, height, bpp;
        result.bytes = stbi_load(filepath, &width, &height, &bpp, STBI_rgb_alpha);
        result.width = (uint32_t)width;
        result.height = (uint32_t)height;
        DX_ASSERT(result.bytes && "Failed to load image");

        return result;
    }

    void FreeImage(const LoadImageResult& result)
    {
        stbi_image_free(result.bytes);
    }

    cgltf_data* LoadGLTF(const char* filepath)
    {
        // -------------------------------------------------------------------------------
        // Load the GLTF data

        cgltf_options cgltf_options = {};
        // We want cgltf to use our allocator
        cgltf_options.memory.alloc_func = [](void* user, cgltf_size size)
        {
            (void)user;
            return g_thread_alloc.Allocate(size, 16);
        };
        cgltf_options.memory.free_func = [](void* user, void* ptr)
        {
            (void)user; (void)ptr;
        };
        cgltf_data* data = nullptr;
        cgltf_result cgltf_result = cgltf_parse_file(&cgltf_options, filepath, &data);
        DX_ASSERT(cgltf_result == cgltf_result_success && "Failed to load GLTF file");

        cgltf_load_buffers(&cgltf_options, data, filepath);
        return data;
    }

}
