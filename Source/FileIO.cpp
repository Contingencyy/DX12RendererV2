#include "Pch.h"
#include "FileIO.h"
#include "Renderer/Renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"

#define CGLTF_IMPLEMENTATION
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

        return result;
    }

    template<typename T>
    T* CGLTFGetDataPointer(const cgltf_accessor* accessor)
    {
        cgltf_buffer_view* buffer_view = accessor->buffer_view;
        uint8_t* base_ptr = (uint8_t*)(buffer_view->buffer->data);
        base_ptr += buffer_view->offset;
        base_ptr += accessor->offset;

        return (T*)base_ptr;
    }

    LoadGLTFResult LoadGLTF(const char* filepath)
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

        // -------------------------------------------------------------------------------
        // Parse the CGLTF data

        LoadGLTFResult result = {};
        result.num_meshes = data->meshes_count;
        result.mesh_params = g_thread_alloc.Allocate<Renderer::UploadMeshParams>(data->meshes_count);

        for (uint32_t mesh_idx = 0; mesh_idx < data->meshes_count; ++mesh_idx)
        {
            Renderer::UploadMeshParams* upload_mesh_params = &result.mesh_params[mesh_idx];
            cgltf_mesh* mesh = &data->meshes[mesh_idx];
            
            for (uint32_t prim_idx = 0; prim_idx < mesh->primitives_count; ++prim_idx)
            {
                cgltf_primitive* primitive = &mesh->primitives[prim_idx];
                DX_ASSERT(primitive->indices->count % 3 == 0);

                // -------------------------------------------------------------------------------
                // Load all of the index data for the current primitive

                upload_mesh_params->num_indices = primitive->indices->count;
                upload_mesh_params->indices = g_thread_alloc.Allocate<uint32_t>(primitive->indices->count);
                if (primitive->indices->component_type == cgltf_component_type_r_32u)
                {
                    upload_mesh_params->indices = CGLTFGetDataPointer<uint32_t>(primitive->indices);
                }
                else
                {
                    DX_ASSERT(primitive->indices->component_type == cgltf_component_type_r_16u);
                    uint16_t* indices_16 = CGLTFGetDataPointer<uint16_t>(primitive->indices);

                    for (uint32_t i = 0; i < primitive->indices->count; ++i)
                    {
                        upload_mesh_params->indices[i] = indices_16[i];
                    }
                }

                // -------------------------------------------------------------------------------
                // Load all of the vertex data for the current primitive

                upload_mesh_params->num_vertices = primitive->attributes[0].data->count;
                upload_mesh_params->vertices = g_thread_alloc.Allocate<Renderer::Vertex>(primitive->attributes[0].data->count);

                for (uint32_t attrib_idx = 0; attrib_idx < primitive->attributes_count; ++attrib_idx)
                {
                    cgltf_attribute* attribute = &primitive->attributes[attrib_idx];

                    switch (attribute->type)
                    {
                    case cgltf_attribute_type_position:
                    {
                        DX_ASSERT(attribute->data->type == cgltf_type_vec3);
                        DXMath::Vec3* data_pos = CGLTFGetDataPointer<DXMath::Vec3>(attribute->data);
                        
                        for (uint32_t vert_idx = 0; vert_idx < attribute->data->count; ++vert_idx)
                        {
                            upload_mesh_params->vertices[vert_idx].pos = data_pos[vert_idx];
                        }
                    } break;
                    case cgltf_attribute_type_texcoord:
                    {
                        DX_ASSERT(attribute->data->type == cgltf_type_vec2);
                        DXMath::Vec2* data_uv = CGLTFGetDataPointer<DXMath::Vec2>(attribute->data);

                        for (uint32_t vert_idx = 0; vert_idx < attribute->data->count; ++vert_idx)
                        {
                            upload_mesh_params->vertices[vert_idx].uv = *data_uv;
                        }
                    } break;
                    }
                }
            }

        }

        // Free the clgtf data
        cgltf_free(data);
        return result;
    }

}
