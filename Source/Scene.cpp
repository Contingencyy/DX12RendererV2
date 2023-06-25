#include "Pch.h"
#include "Scene.h"

#include "Input.h"

namespace Scene
{

	struct InternalData
	{
		Vec3 camera_translation;
		Mat4x4 camera_view;
		Mat4x4 camera_projection;
	} static data;

	void Update(float dt)
	{
		// TODO: We want to be left-handed, but currently positive X is A (left), and positive Y is Space (up)
		data.camera_translation.x += dt * Input::GetAxis2D(Input::KeyCode_A, Input::KeyCode_D);
		data.camera_translation.y += dt * Input::GetAxis2D(Input::KeyCode_LCTRL, Input::KeyCode_Space);
		data.camera_translation.z += dt * Input::GetAxis2D(Input::KeyCode_W, Input::KeyCode_S);
		data.camera_view = MakeMat4x4FromTranslation(data.camera_translation);
		data.camera_projection = MakeMat4x4Perspective(Deg2Rad(60.0), 1280 / 720, 0.1, 1000.0);
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
