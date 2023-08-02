#include "Pch.h"
#include "FileIO.h"
#include "Renderer/Renderer.h"

void* Realloc(void* ptr, size_t old_size, size_t new_size)
{
    void* new_ptr = g_thread_alloc.Allocate(new_size, 1);
    memcpy(new_ptr, ptr, old_size);
    return new_ptr;
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(size) g_thread_alloc.Allocate(size, 1)
#define STBI_REALLOC_SIZED(ptr, old_size, new_size) Realloc(ptr, old_size, new_size)
// TODO: This should be a noop, it would be nicer if each asset upload had its own memory scope
#define STBI_FREE(ptr) g_thread_alloc.Reset(ptr)
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

    void FreeImage(LoadImageResult* result)
    {
        stbi_image_free(result->bytes);
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
