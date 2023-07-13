#include "Pch.h"
#include "Scene.h"
#include "Renderer/Renderer.h"
#include "AssetManager.h"
#include "Input.h"

namespace Scene
{

	static void RenderModelNode(const Model& model, const Model::Node& node, Mat4x4& current_transform)
	{
		for (uint32_t mesh_idx = 0; mesh_idx < node.num_meshes; ++mesh_idx)
		{
			Renderer::RenderMesh(node.mesh_handles[mesh_idx], node.texture_handles[mesh_idx], current_transform);
		}

		for (uint32_t child_idx = 0; child_idx < node.num_children; ++child_idx)
		{
			const Model::Node& child_node = model.nodes[node.children[child_idx]];
			Mat4x4 node_transform = Mat4x4Mul(child_node.transform, current_transform);
			RenderModelNode(model, child_node, node_transform);
		}
	}

	static void RenderModel(const Model& model, Mat4x4& current_transform)
	{
		for (uint32_t root_node_idx = 0; root_node_idx < model.num_root_nodes; ++root_node_idx)
		{
			const Model::Node& root_node = model.nodes[model.root_nodes[root_node_idx]];
			Mat4x4 root_transform = Mat4x4Mul(root_node.transform, current_transform);
			RenderModelNode(model, root_node, root_transform);
		}
	}

	struct InternalData
	{
		Vec3 camera_translation;
		Vec3 camera_rotation;
		float camera_yaw;
		float camera_pitch;
		Mat4x4 camera_transform;
		Mat4x4 camera_view;
		Mat4x4 camera_projection;
		float camera_speed = 25.0;
	} static data;

	void Update(float dt)
	{
		if (Input::IsMouseCaptured())
		{
			// Camera rotation
			int mouse_x, mouse_y;
			Input::GetMouseMoveRel(&mouse_x, &mouse_y);

			float yaw_sign = data.camera_transform.r1.y < 0.0 ? -1.0 : 1.0;
			data.camera_yaw += dt * yaw_sign * mouse_x;
			data.camera_pitch += dt * mouse_y;
			data.camera_pitch = DX_MIN(data.camera_pitch, Deg2Rad(90.0));
			data.camera_pitch = DX_MAX(data.camera_pitch, Deg2Rad(-90.0));

			data.camera_rotation = Vec3(data.camera_pitch, data.camera_yaw, 0.0);

			// Camera translation
			data.camera_translation = Vec3Add(Vec3MulScalar(RightVectorFromTransform(data.camera_transform), dt * data.camera_speed * Input::GetAxis1D(Input::KeyCode_D, Input::KeyCode_A)), data.camera_translation);
			data.camera_translation = Vec3Add(Vec3MulScalar(UpVectorFromTransform(data.camera_transform), dt * data.camera_speed * Input::GetAxis1D(Input::KeyCode_Space, Input::KeyCode_LCTRL)), data.camera_translation);
			data.camera_translation = Vec3Add(Vec3MulScalar(ForwardVectorFromTransform(data.camera_transform), dt * data.camera_speed * Input::GetAxis1D(Input::KeyCode_W, Input::KeyCode_S)), data.camera_translation);
		}

		// Make camera transform
		data.camera_transform = Mat4x4FromTRS(data.camera_translation, EulerToQuat(data.camera_rotation), Vec3(1.0));
		
		// Make the camera view and projection matrices
		data.camera_view = Mat4x4Inverse(data.camera_transform);
		data.camera_projection = Mat4x4Perspective(Deg2Rad(60.0), 16.0 / 9.0, 0.1, 10000.0);
	}

	void Render()
	{
		Mat4x4 model_transform = Mat4x4FromTRS(Vec3(0.0), EulerToQuat(Vec3(0.0)), Vec3(10.0));

		Model* chess_model = AssetManager::GetModel("Assets/Models/ABeautifulGame/ABeautifulGame.gltf");
		Model* sponza_model = AssetManager::GetModel("Assets/Models/Sponza/Sponza.gltf");

		RenderModel(*chess_model, model_transform);
		RenderModel(*sponza_model, model_transform);
	}

	Mat4x4 GetCameraView()
	{
		return data.camera_view;
	}

	Mat4x4 GetCameraProjection()
	{
		return data.camera_projection;
	}

}
