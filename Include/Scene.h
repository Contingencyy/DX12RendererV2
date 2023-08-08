#pragma once

namespace Scene
{

	void Update(float dt);
	void Render();

	Vec3 GetCameraPosition();
	Mat4x4 GetCameraView();
	Mat4x4 GetCameraProjection();

}
