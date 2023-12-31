#include "Pch.h"
#include "AssetManager.h"
#include "FileIO.h"
#include "Containers/Hashmap.h"
#include "Renderer/Renderer.h"

#include "mikkt/mikktspace.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

class TangentCalculator
{
public:
    TangentCalculator()
    {
        m_mikkt_interface.m_getNumFaces = GetNumFaces;
        m_mikkt_interface.m_getNumVerticesOfFace = GetNumVerticesOfFace;

        m_mikkt_interface.m_getNormal = GetNormal;
        m_mikkt_interface.m_getPosition = GetPosition;
        m_mikkt_interface.m_getTexCoord = GetTexCoord;
        m_mikkt_interface.m_setTSpaceBasic = SetTSpaceBasic;

        m_mikkt_context.m_pInterface = &m_mikkt_interface;
    }

    void Calculate(Renderer::UploadMeshParams* loadedMesh)
    {
        m_mikkt_context.m_pUserData = loadedMesh;
        genTangSpaceDefault(&m_mikkt_context);
    }

private:
    static int GetNumFaces(const SMikkTSpaceContext* context)
    {
        Renderer::UploadMeshParams* mesh = static_cast<Renderer::UploadMeshParams*>(context->m_pUserData);
        return mesh->num_indices / 3;
    }

    static int GetVertexIndex(const SMikkTSpaceContext* context, int iFace, int iVert)
    {
        Renderer::UploadMeshParams* mesh = static_cast<Renderer::UploadMeshParams*>(context->m_pUserData);

        uint32_t face_size = GetNumVerticesOfFace(context, iFace);
        uint32_t indices_index = (iFace * face_size) + iVert;

        return mesh->indices[indices_index];
    }

    static int GetNumVerticesOfFace(const SMikkTSpaceContext* context, int iFace)
    {
        // We only expect triangles (for now), so always return 3
        return 3;
    }

    static void GetPosition(const SMikkTSpaceContext* context, float outpos[], int iFace, int iVert)
    {
        Renderer::UploadMeshParams* mesh = static_cast<Renderer::UploadMeshParams*>(context->m_pUserData);

        uint32_t index = GetVertexIndex(context, iFace, iVert);
        const Renderer::Vertex& vertex = mesh->vertices[index];

        outpos[0] = vertex.pos.x;
        outpos[1] = vertex.pos.y;
        outpos[2] = vertex.pos.z;
    }

    static void GetNormal(const SMikkTSpaceContext* context, float outnormal[], int iFace, int iVert)
    {
        Renderer::UploadMeshParams* mesh = static_cast<Renderer::UploadMeshParams*>(context->m_pUserData);

        uint32_t index = GetVertexIndex(context, iFace, iVert);
        const Renderer::Vertex& vertex = mesh->vertices[index];

        outnormal[0] = vertex.normal.x;
        outnormal[1] = vertex.normal.y;
        outnormal[2] = vertex.normal.z;
    }

    static void GetTexCoord(const SMikkTSpaceContext* context, float outuv[], int iFace, int iVert)
    {
        Renderer::UploadMeshParams* mesh = static_cast<Renderer::UploadMeshParams*>(context->m_pUserData);

        uint32_t index = GetVertexIndex(context, iFace, iVert);
        const Renderer::Vertex& vertex = mesh->vertices[index];

        outuv[0] = vertex.uv.x;
        outuv[1] = vertex.uv.y;
    }

    static void SetTSpaceBasic(const SMikkTSpaceContext* context, const float tangentu[], float fSign, int iFace, int iVert)
    {
        Renderer::UploadMeshParams* mesh = static_cast<Renderer::UploadMeshParams*>(context->m_pUserData);

        uint32_t index = GetVertexIndex(context, iFace, iVert);
        Renderer::Vertex& vertex = mesh->vertices[index];

        vertex.tangent.x = tangentu[0];
        vertex.tangent.y = tangentu[1];
        vertex.tangent.z = tangentu[2];
        vertex.tangent.w = fSign;
    }

private:
    SMikkTSpaceInterface m_mikkt_interface = {};
    SMikkTSpaceContext m_mikkt_context = {};

};
    
template<typename T>
static T* CGLTFGetDataPointer(const cgltf_accessor* accessor)
{
    cgltf_buffer_view* buffer_view = accessor->buffer_view;
    uint8_t* base_ptr = (uint8_t*)(buffer_view->buffer->data);
    base_ptr += buffer_view->offset;
    base_ptr += accessor->offset;

    return (T*)base_ptr;
}

static size_t CGLTFImageIndex(const cgltf_data* data, const cgltf_image* image)
{
    return (size_t)(image - data->images);
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
    char* result = (char*)g_thread_alloc.Allocate(strlen(filepath) + strlen(uri), alignof(char));

    cgltf_combine_paths(result, filepath, uri);
    cgltf_decode_uri(result + strlen(result) - strlen(uri));

    return result;
}

namespace AssetManager
{

    struct InternalData
    {
        LinearAllocator alloc;
        MemoryScope memory_scope;

        Hashmap<const char*, ResourceHandle>* texture_assets_map;
        Hashmap<const char*, Model>* model_assets_map;

        TangentCalculator tangent_calc;
    } static data;

    void Init()
    {
        data.memory_scope = MemoryScope(&data.alloc, data.alloc.at_ptr);

        data.texture_assets_map = data.memory_scope.New<Hashmap<const char*, ResourceHandle>>(&data.memory_scope, 1024);
        data.model_assets_map = data.memory_scope.New<Hashmap<const char*, Model>>(&data.memory_scope, 64);
    }

    void Exit()
    {
        // NOTE: I am not very fond of this.. Memory scopes are great for RAII, but my systems do not have a constructor/destructor
		// which means we have to call the memory scope destructor manually here. I will figure this out later, I need to make up my mind about
		// RAII first.
        data.memory_scope.~MemoryScope();
    }

	void LoadTexture(const char* filepath)
	{
        FileIO::LoadImageResult image = FileIO::LoadImage(filepath);

		Renderer::UploadTextureParams texture_params = {};
		texture_params.format = Renderer::TextureFormat_RGBA8_Unorm;
		texture_params.width = image.width;
		texture_params.height = image.height;
		texture_params.bytes = image.bytes;
		texture_params.name = filepath;
		
		ResourceHandle texture_handle = Renderer::UploadTexture(texture_params);
        FileIO::FreeImage(&image);

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
        model.nodes = data.memory_scope.Allocate<Model::Node>(cgltf_data->nodes_count);
        model.num_nodes = cgltf_data->nodes_count;

        // -------------------------------------------------------------------------------
        // Parse the CGLTF data - Materials and textures
        
        MemoryScope alloc_scope(&g_thread_alloc, g_thread_alloc.at_ptr);
        ResourceHandle* texture_handles = alloc_scope.Allocate<ResourceHandle>(cgltf_data->images_count);

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
        ResourceHandle* mesh_handles = alloc_scope.Allocate<ResourceHandle>(num_meshes);
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
                upload_mesh_params.indices = alloc_scope.Allocate<uint32_t>(primitive->indices->count);

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
                upload_mesh_params.vertices = alloc_scope.Allocate<Renderer::Vertex>(primitive->attributes[0].data->count);
                bool calculate_tangents = true;

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
                    case cgltf_attribute_type_normal:
                    {
                        DX_ASSERT(attribute->data->type == cgltf_type_vec3);
                        DXMath::Vec3* data_normal = CGLTFGetDataPointer<DXMath::Vec3>(attribute->data);

                        for (uint32_t vert_idx = 0; vert_idx < attribute->data->count; ++vert_idx)
                        {
                            upload_mesh_params.vertices[vert_idx].normal = data_normal[vert_idx];
                        }
                    } break;
                    case cgltf_attribute_type_tangent:
                    {
                        DX_ASSERT(attribute->data->type == cgltf_type_vec4);
                        DXMath::Vec4* data_tangent = CGLTFGetDataPointer<DXMath::Vec4>(attribute->data);

                        for (uint32_t vert_idx = 0; vert_idx < attribute->data->count; ++vert_idx)
                        {
                            upload_mesh_params.vertices[vert_idx].tangent = data_tangent[vert_idx];
                        }

                        calculate_tangents = false;
                    } break;
                    }
                }

                if (calculate_tangents)
                {
                    data.tangent_calc.Calculate(&upload_mesh_params);
                }

                mesh_handles[mesh_handle_cur++] = Renderer::UploadMesh(upload_mesh_params);
            }
        }

        // TODO: GLTF Scenes
        for (uint32_t node_idx = 0; node_idx < cgltf_data->nodes_count; ++node_idx)
        {
            cgltf_node* cgltf_node = &cgltf_data->nodes[node_idx];
            Model::Node* node = &model.nodes[node_idx];
            node->transform = cgltf_node->mesh ? CGLTFNodeGetTransform(cgltf_node) : Mat4x4Identity();
            node->num_children = cgltf_node->children_count;
            node->children = data.memory_scope.Allocate<size_t>(cgltf_node->children_count);

            for (uint32_t child_idx = 0; child_idx < cgltf_node->children_count; ++child_idx)
            {
                node->children[child_idx] = CGLTFGetNodeIndex(cgltf_data, cgltf_node->children[child_idx]);
            }

            if (cgltf_node->mesh)
            {
                node->num_meshes = cgltf_node->mesh->primitives_count;
                node->mesh_handles = data.memory_scope.Allocate<ResourceHandle>(cgltf_node->mesh->primitives_count);
                node->materials = data.memory_scope.Allocate<Renderer::Material>(cgltf_node->mesh->primitives_count);

                for (uint32_t prim_idx = 0; prim_idx < cgltf_node->mesh->primitives_count; ++prim_idx)
                {
                    cgltf_primitive* primitive = &cgltf_node->mesh->primitives[prim_idx];

                    node->name = cgltf_node->name;

                    size_t mesh_index = CGLTFMeshIndex(cgltf_data, cgltf_node->mesh) + CGLTFPrimitiveIndex(cgltf_node->mesh, primitive);
                    node->mesh_handles[prim_idx] = mesh_handles[mesh_index];

                    // Note: The renderer will fall back to default textures if texture handles are invalid
                    node->materials[prim_idx].metallic_factor = 0.0;
                    node->materials[prim_idx].roughness_factor = 0.3;

                    if (primitive->material->pbr_metallic_roughness.base_color_texture.texture)
                    {
                        node->materials[prim_idx].base_color_texture_handle = texture_handles[CGLTFImageIndex(
                            cgltf_data, primitive->material->pbr_metallic_roughness.base_color_texture.texture->image
                        )];
                    }
                    if (primitive->material->normal_texture.texture)
                    {
                        node->materials[prim_idx].normal_texture_handle = texture_handles[CGLTFImageIndex(
                            cgltf_data, primitive->material->normal_texture.texture->image
                        )];
                    }
                    if (primitive->material->pbr_metallic_roughness.metallic_roughness_texture.texture)
                    {
                        node->materials[prim_idx].metallic_roughness_texture_handle = texture_handles[CGLTFImageIndex(
                            cgltf_data, primitive->material->pbr_metallic_roughness.metallic_roughness_texture.texture->image
                        )];
                        node->materials[prim_idx].metallic_factor = 1.0;
                        node->materials[prim_idx].roughness_factor = 1.0;
                    }
                }
            }

            if (!cgltf_node->parent)
            {
                model.num_root_nodes++;
            }
        }

        size_t root_node_cur = 0;
        model.root_nodes = data.memory_scope.Allocate<size_t>(model.num_root_nodes);

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
