#pragma once

namespace DXMath
{

	struct Vec2
	{
		Vec2() = default;
		Vec2(float x, float y)
			: x(x), y(y) {}
		Vec2(float scalar)
			: x(scalar), y(scalar) {}

		float x = 0.0, y = 0.0;
	};

	struct Vec3
	{
		Vec3() = default;
		Vec3(float x, float y, float z)
			: x(x), y(y), z(z) {}
		Vec3(float scalar)
			: x(scalar), y(scalar), z(scalar) {}

		float x = 0.0, y = 0.0, z = 0.0;
	};

	struct Vec4
	{
		Vec4() = default;
		Vec4(float x, float y, float z, float w)
			: x(x), y(y), z(z), w(w) {}
		Vec4(float scalar)
			: x(scalar), y(scalar), z(scalar), w(scalar) {}

		float x = 0.0, y = 0.0, z = 0.0, w = 0.0;
	};

	struct Mat4x4
	{
		Mat4x4()
		{
			r0.x = r1.y = r2.z = r3.w = 1.0;
		}
		Mat4x4(const Vec4& r0, const Vec4& r1, const Vec4& r2, const Vec4& r3)
			: r0(r0), r1(r1), r2(r2), r3(r3) {}

		Vec4 r0, r1, r2, r3;
	};

}
