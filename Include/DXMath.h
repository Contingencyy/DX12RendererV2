#pragma once
#include <cmath>

namespace DXMath
{

	static constexpr float PI = 3.14159265;
	static constexpr float INV_PI = 1.0 / PI;
	static constexpr float DEG2RAD_PI = PI / 180.0;
	static constexpr float RAD2DEG_PI = 180.0 / PI;

	static inline float Deg2Rad(float deg)
	{
		return deg * DEG2RAD_PI;
	}
	
	static inline float Rad2Deg(float rad)
	{
		return rad * RAD2DEG_PI;
	}

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
		Mat4x4() = default;
		Mat4x4(const Vec4& r0, const Vec4& r1, const Vec4& r2, const Vec4& r3)
			: r0(r0), r1(r1), r2(r2), r3(r3) {}

		union
		{
			float v[4][4] = {0};
			struct
			{
				Vec4 r0, r1, r2, r3;
			};
		};
	};

	static Mat4x4 MakeMat4x4Identity()
	{
		Mat4x4 result;
		result.r0.x = result.r1.y = result.r2.z = result.r3.w = 1.0;
		return result;
	}

	static Mat4x4 MakeMat4x4FromTranslation(const Vec3& translation)
	{
		Mat4x4 result = MakeMat4x4Identity();
		result.r3.x = translation.x;
		result.r3.y = translation.y;
		result.r3.z = translation.z;
		return result;
	}
	
	static Mat4x4 MakeMat4x4Perspective(float fov, float aspect, float near, float far)
	{
		float g = tanf(fov / 2);
		float k = far / (far - near);
		Mat4x4 result(
			Vec4(g / aspect, 0, 0, 0),
			Vec4(0,			 g, 0, 0),
			Vec4(0,			 0, k, -near * k),
			Vec4(0,			 0, -1, 0)
		);

		return result;
	}

	static Mat4x4 Mat4x4Mul(const Mat4x4& m1, const Mat4x4& m2)
	{
		Mat4x4 result;
		for (int col = 0; col < 4; ++col)
		{
			for (int row = 0; row < 4; ++row)
			{
				for (int v = 0; v < 4; ++v)
				{
					result.v[col][row] += m1.v[col][v] * m2.v[v][row];
				}
			}
		}

		return result;
	}

}
