#include "Pch.h"
#include "AssetManager.h"
#include "FileIO.h"
#include "Renderer/Renderer.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
    
template<typename T>
static T* CGLTFGetDataPointer(const cgltf_accessor* accessor)
{
    cgltf_buffer_view* buffer_view = accessor->buffer_view;
    uint8_t* base_ptr = (uint8_t*)(buffer_view->buffer->data);
    base_ptr += buffer_view->offset;
    base_ptr += accessor->offset;

    return (T*)base_ptr;
}

static size_t CGLTFImageIndex(const cgltf_data* data, const cgltf_image* texture)
{
    return (size_t)(texture - data->images);
}

static size_t CGLTFMeshIndex(const cgltf_data* data, const cgltf_mesh* mesh)
{
    return (size_t)(mesh - data->meshes);
}

static size_t CGLTFPrimitiveIndex(const cgltf_mesh* mesh, const cgltf_primitive* primitive)
{
    return (size_t)(primitive - mesh->primitives);
}

static size_t CGLTFGetNodeIndex(const cgltf_data* data, const cgltf_node* node)
{
    return (size_t)(node - data->nodes);
}

static Mat4x4 CGLTFNodeGetTransform(const cgltf_node* node)
{
    if (node->has_matrix)
    {
        Mat4x4 transform;
        memcpy(&transform.v, &node->matrix[0], sizeof(Mat4x4));
        return transform;
    }

    Vec3 translation;
    Quat rotation;
    Vec3 scale(1.0);

    if (node->has_translation)
    {
        translation.x = node->translation[0];
        translation.y = node->translation[1];
        translation.z = node->translation[2];
    }
    if (node->has_rotation)
    {
        rotation.x = node->rotation[0];
        rotation.y = node->rotation[1];
        rotation.z = node->rotation[2];
        rotation.w = node->rotation[3];
    }
    if (node->has_scale)
    {
        scale.x = node->scale[0];
        scale.y = node->scale[1];
        scale.z = node->scale[2];
    }
    
    return Mat4x4FromTRS(translation, rotation, scale);
}

static char* CreatePathFromUri(const char* filepath, const char* uri)
{
    char* result = g_thread_alloc.Allocate<char>(strlen(filepath) + strlen(uri));

    cgltf_combine_paths(result, filepath, uri);
    cgltf_decode_uri(result + strlen(result) - strlen(uri));

    return result;
}

namespace AssetManager
{

    template<typename TAsset>
    class AssetHashmap
    {
    public:
        static constexpr const char* SLOT_UNUSED = "";

    public:
        AssetHashmap(Allocator* alloc, size_t capacity = 1024)
            : m_allocator(alloc), m_capacity(capacity), m_size(0)
        {
            m_slots = m_allocator->Allocate<Slot>(m_capacity);

            for (uint32_t i = 0; i < m_capacity; ++i)
            {
                m_slots[i].key = SLOT_UNUSED;
            }
        }

        ~AssetHashmap()
        {
            if constexpr (!std::is_trivially_destructible_v<TAsset>)
            {
                for (size_t i = 0; i < m_capacity; ++i)
                {
                    Slot* slot = &m_slots[i];

                    if (slot->key != SLOT_UNUSED)
                    {
                        slot->value.~TAsset();
                    }
                }
            }
        }

        AssetHashmap(const AssetHashmap& other) = delete;
        AssetHashmap(AssetHashmap&& other) = delete;
        const AssetHashmap& operator=(const AssetHashmap& other) = delete;
        AssetHashmap&& operator=(AssetHashmap&& other) = delete;

        void Insert(const char* key, TAsset asset)
        {
            Slot temp = {
                .key = key,
                .value = asset
            };
            uint32_t slot_index = HashSlotIndex(key);

            while (m_slots[slot_index].key != key &&
                m_slots[slot_index].key != SLOT_UNUSED)
            {
                slot_index++;
                slot_index %= m_capacity;
            }

            DX_ASSERT(m_slots[slot_index].key == SLOT_UNUSED && "Collision found in hash map while trying to insert");
            m_slots[slot_index] = temp;
            m_size++;
        }

        void Remove(const char* key)
        {
            uint32_t slot_index = HashSlotIndex(key);

            while (m_slots[slot_index].key != SLOT_UNUSED)
            {
                if (m_slots[slot_index].key == key)
                {
                    Slot* slot = &m_slots[slot_index];

                    if constexpr (!std::is_trivially_destructible_v<TAsset>)
                    {
                        slot->value.~TAsset();
                    }

                    slot->key = SLOT_UNUSED;
                    slot->value = {};

                    m_size--;
                    break;
                }

                slot_index++;
                slot_index %= m_capacity;
            }
        }

        TAsset* Find(const char* key)
        {
            TAsset* value = nullptr;
            uint32_t slot_index = HashSlotIndex(key);
            uint32_t counter = 0;

            while (m_slots[slot_index].key != SLOT_UNUSED)
            {
                if (counter++ > m_capacity)
                {
                    break;
                }

                if (m_slots[slot_index].key == key)
                {
                    value = &m_slots[slot_index].value;
                    break;
                }

                slot_index++;
                slot_index %= m_capacity;
            }

            return value;
        }

    private:
        uint32_t HashSlotIndex(const char* key)
        {
            return Hash::DJB2(key, strlen(key)) % m_capacity;
        }

    private:
        struct Slot
        {
            const char* key;
            TAsset value;
        };

        Allocator* m_allocator;
        size_t m_capacity;
        size_t m_size;

        Slot* m_slots;

    };

    struct InternalData
    {
        Allocator allocator;

        AssetHashmap<ResourceHandle>* texture_assets_map;
        AssetHashmap<Model>* model_assets_map;
    } static data;

    void Init()
    {
        data.texture_assets_map = data.allocator.AllocateConstruct<AssetHashmap<ResourceHandle>>(&data.allocator, 1024);
        data.model_assets_map = data.allocator.AllocateConstruct<AssetHashmap<Model>>(&data.allocator, 1024);
    }

    void Exit()
    {
    }

	void LoadTexture(const char* filepath)
	{
		FileIO::LoadImageResult image = FileIO::LoadImage(filepath);

		Renderer::UploadTextureParams texture_params = {};
		texture_params.format = Renderer::TextureFormat_RGBA8;
		texture_params.width = image.width;
		texture_params.height = image.height;
		texture_params.bytes = image.bytes;
		texture_params.name = filepath;
		
		ResourceHandle texture_handle = Renderer::UploadTexture(texture_params);
        FileIO::FreeImage(image);

        data.texture_assets_map->Insert(filepath, texture_handle);
	}

    ResourceHandle GetTexture(const char* filepath)
    {
        return *data.texture_assets_map->Find(filepath);
    }

	void LoadModel(const char* filepath)
	{
        cgltf_data* cgltf_data = FileIO::LoadGLTF(filepath);

        Model model = {};
        model.name = filepath;
        model.nodes = data.allocator.Allocate<Model::Node>(cgltf_data->nodes_count);
        model.num_nodes = cgltf_data->nodes_count;

        // -------------------------------------------------------------------------------
        // Parse the CGLTF data - Materials and textures
        
        ResourceHandle* texture_handles = g_thread_alloc.Allocate<ResourceHandle>(cgltf_data->materials_count);

        for (uint32_t img_idx = 0; img_idx < cgltf_data->images_count; ++img_idx)
        {
            const char* texture_filepath = CreatePathFromUri(filepath, cgltf_data->images[img_idx].uri);
            LoadTexture(texture_filepath);
            texture_handles[img_idx] = GetTexture(texture_filepath);
        }

        // -------------------------------------------------------------------------------
        // Parse the CGLTF data

        // Check how many individual meshes we have
        size_t num_meshes = 0;

        for (uint32_t mesh_idx = 0; mesh_idx < cgltf_data->meshes_count; ++mesh_idx)
        {
            cgltf_mesh* mesh = &cgltf_data->meshes[mesh_idx];
            num_meshes += mesh->primitives_count;
        }

        // Allocate all the mesh resource handles we need
        ResourceHandle* mesh_handles = g_thread_alloc.Allocate<ResourceHandle>(num_meshes);
        size_t mesh_handle_cur = 0;

        for (uint32_t mesh_idx = 0; mesh_idx < cgltf_data->meshes_count; ++mesh_idx)
        {
            cgltf_mesh* mesh = &cgltf_data->meshes[mesh_idx];

            for (uint32_t prim_idx = 0; prim_idx < mesh->primitives_count; ++prim_idx)
            {
                Renderer::UploadMeshParams upload_mesh_params = {};
                cgltf_primitive* primitive = &mesh->primitives[prim_idx];
                DX_ASSERT(primitive->indices->count % 3 == 0);
                
                // -------------------------------------------------------------------------------
                // Load all of the index data for the current primitive

                upload_mesh_params.num_indices = primitive->indices->count;
                upload_mesh_params.indices = g_thread_alloc.Allocate<uint32_t>(primitive->indices->count);

                if (primitive->indices->component_type == cgltf_component_type_r_32u)
                {
                    upload_mesh_params.indices = CGLTFGetDataPointer<uint32_t>(primitive->indices);
                }
                else
                {
                    DX_ASSERT(primitive->indices->component_type == cgltf_component_type_r_16u);
                    uint16_t* indices_16 = CGLTFGetDataPointer<uint16_t>(primitive->indices);

                    for (uint32_t i = 0; i < primitive->indices->count; ++i)
                    {
                        upload_mesh_params.indices[i] = indices_16[i];
                    }
                }

                // -------------------------------------------------------------------------------
                // Load all of the vertex data for the current primitive

                upload_mesh_params.num_vertices = primitive->attributes[0].data->count;
                upload_mesh_params.vertices = g_thread_alloc.Allocate<Renderer::Vertex>(primitive->attributes[0].data->count);

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
                            upload_mesh_params.vertices[vert_idx].pos = data_pos[vert_idx];
                        }
                    } break;
                    case cgltf_attribute_type_texcoord:
                    {
                        DX_ASSERT(attribute->data->type == cgltf_type_vec2);
                        DXMath::Vec2* data_uv = CGLTFGetDataPointer<DXMath::Vec2>(attribute->data);

                        for (uint32_t vert_idx = 0; vert_idx < attribute->data->count; ++vert_idx)
                        {
                            upload_mesh_params.vertices[vert_idx].uv = data_uv[vert_idx];
                        }
                    } break;
                    }
                }

                mesh_handles[mesh_handle_cur++] = Renderer::UploadMesh(upload_mesh_params);
            }
        }

        for (uint32_t node_idx = 0; node_idx < cgltf_data->nodes_count; ++node_idx)
        {
            cgltf_node* cgltf_node = &cgltf_data->nodes[node_idx];
            Model::Node* node = &model.nodes[node_idx];
            node->transform = cgltf_node->mesh ? CGLTFNodeGetTransform(cgltf_node) : Mat4x4Identity();
            node->num_children = cgltf_node->children_count;
            node->children = data.allocator.Allocate<size_t>(cgltf_node->children_count);

            for (uint32_t child_idx = 0; child_idx < cgltf_node->children_count; ++child_idx)
            {
                node->children[child_idx] = CGLTFGetNodeIndex(cgltf_data, cgltf_node->children[child_idx]);
            }

            if (cgltf_node->mesh)
            {
                node->num_meshes = cgltf_node->mesh->primitives_count;
                node->mesh_handles = data.allocator.Allocate<ResourceHandle>(cgltf_node->mesh->primitives_count);
                node->texture_handles = data.allocator.Allocate<ResourceHandle>(cgltf_node->mesh->primitives_count);

                for (uint32_t prim_idx = 0; prim_idx < cgltf_node->mesh->primitives_count; ++prim_idx)
                {
                    cgltf_primitive* primitive = &cgltf_node->mesh->primitives[prim_idx];

                    node->name = cgltf_node->name;

                    size_t mesh_index = CGLTFMeshIndex(cgltf_data, cgltf_node->mesh) + CGLTFPrimitiveIndex(cgltf_node->mesh, primitive);
                    node->mesh_handles[prim_idx] = mesh_handles[mesh_index];

                    // Note: Only set the texture handle if the base color texture is actually valid
                    // Note: The renderer will fall back to default textures if texture handles are invalid
                    if (primitive->material->pbr_metallic_roughness.base_color_texture.texture)
                    {
                        node->texture_handles[prim_idx] = texture_handles[CGLTFImageIndex(cgltf_data, primitive->material->pbr_metallic_roughness.base_color_texture.texture->image)];
                    }
                }
            }

            if (!cgltf_node->parent)
            {
                model.num_root_nodes++;
            }
        }

        size_t root_node_cur = 0;
        model.root_nodes = data.allocator.Allocate<size_t>(model.num_root_nodes);

        for (uint32_t node_idx = 0; node_idx < cgltf_data->nodes_count; ++node_idx)
        {
            cgltf_node* cgltf_node = &cgltf_data->nodes[node_idx];

            if (!cgltf_node->parent)
            {
                model.root_nodes[root_node_cur++] = node_idx;
            }
        }
        
        // Free the clgtf data
        cgltf_free(cgltf_data);
        data.model_assets_map->Insert(filepath, model);
	}

    Model* GetModel(const char* filepath)
    {
        return data.model_assets_map->Find(filepath);
    }

}
