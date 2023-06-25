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
		data.camera_translation = Vec3Add(Vec3MulScalar(Vec3(1.0, 0.0, 0.0), dt * Input::GetAxis1D(Input::KeyCode_D, Input::KeyCode_A)), data.camera_translation);
		data.camera_translation = Vec3Add(Vec3MulScalar(Vec3(0.0, 1.0, 0.0), dt * Input::GetAxis1D(Input::KeyCode_Space, Input::KeyCode_LCTRL)), data.camera_translation);
		data.camera_translation = Vec3Add(Vec3MulScalar(Vec3(0.0, 0.0, -1.0), dt * Input::GetAxis1D(Input::KeyCode_W, Input::KeyCode_S)), data.camera_translation);
		data.camera_view = Mat4x4Inverse(MakeMat4x4FromTranslation(data.camera_translation));
		data.camera_projection = MakeMat4x4Perspective(Deg2Rad(60.0), 16.0 / 9.0, 0.1, 1000.0);
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
