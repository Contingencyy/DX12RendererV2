#include "Pch.h"
#include "Scene.h"

#include "Input.h"

namespace Scene
{

	struct InternalData
	{
		Vec3 camera_translation;
		Vec3 camera_rotation;
		float camera_yaw;
		float camera_pitch;
		Mat4x4 camera_transform;
		Mat4x4 camera_view;
		Mat4x4 camera_projection;
		float camera_speed = 250.0;
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
			data.camera_translation = Vec3Add(Vec3MulScalar(data.camera_transform.r0.xyz, dt * data.camera_speed * Input::GetAxis1D(Input::KeyCode_D, Input::KeyCode_A)), data.camera_translation);
			data.camera_translation = Vec3Add(Vec3MulScalar(data.camera_transform.r1.xyz, dt * data.camera_speed * Input::GetAxis1D(Input::KeyCode_Space, Input::KeyCode_LCTRL)), data.camera_translation);
			data.camera_translation = Vec3Add(Vec3MulScalar(data.camera_transform.r2.xyz, dt * data.camera_speed * Input::GetAxis1D(Input::KeyCode_W, Input::KeyCode_S)), data.camera_translation);
		}

		// Make camera transform
		data.camera_transform = Mat4x4FromTRS(data.camera_translation, EulerToQuat(data.camera_rotation), Vec3(1.0));
		
		// Make the camera view and projection matrices
		data.camera_view = Mat4x4Inverse(data.camera_transform);
		data.camera_projection = Mat4x4Perspective(Deg2Rad(60.0), 16.0 / 9.0, 0.1, 10000.0);
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
