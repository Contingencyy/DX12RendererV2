#include "Pch.h"
#include "Scene.h"

#include "Input.h"

#include <DirectXMath.h>
using namespace DirectX;

namespace Scene
{

	struct InternalData
	{
		Vec3 camera_translation;
		Vec3 camera_rotation;
		Mat4x4 camera_transform;
		Mat4x4 camera_view;
		Mat4x4 camera_projection;
	} static data;

	void Update(float dt)
	{
		// Camera translation
		data.camera_translation = Vec3Add(Vec3MulScalar(data.camera_transform.r0.xyz, dt * Input::GetAxis1D(Input::KeyCode_D, Input::KeyCode_A)), data.camera_translation);
		data.camera_translation = Vec3Add(Vec3MulScalar(data.camera_transform.r1.xyz, dt * Input::GetAxis1D(Input::KeyCode_Space, Input::KeyCode_LCTRL)), data.camera_translation);
		data.camera_translation = Vec3Add(Vec3MulScalar(data.camera_transform.r2.xyz, dt * Input::GetAxis1D(Input::KeyCode_W, Input::KeyCode_S)), data.camera_translation);

		// Camera rotation
		if (Input::IsKeyPressed(Input::KeyCode_RightMouse))
		{
			int mouse_x, mouse_y;
			Input::GetMouseMoveRel(&mouse_x, &mouse_y);
			float yaw_sign = data.camera_transform.r2.y < 0.0 ? -1.0 : 1.0;
			float yaw = dt * yaw_sign * mouse_x;
			float pitch = dt * mouse_y;
			data.camera_rotation = Vec3Add(data.camera_rotation, Vec3(pitch, yaw, 0.0));
		}

		// Make camera transform
		data.camera_transform = Mat4x4FromTRS(data.camera_translation, EulerToQuat(data.camera_rotation), Vec3(1.0));
		
		// Make the camera view and projection matrices
		data.camera_view = Mat4x4Inverse(data.camera_transform);
		data.camera_projection = Mat4x4Perspective(Deg2Rad(60.0), 16.0 / 9.0, 0.1, 1000.0);
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
