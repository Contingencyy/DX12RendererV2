#pragma once

namespace Scene
{

	void Update(float dt);
	void Render();

	Mat4x4 GetCameraView();
	Mat4x4 GetCameraProjection();

}
