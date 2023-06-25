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

	// ----------------------------------------------------------------------------
	// Vec2

	struct Vec2
	{
		Vec2() = default;
		Vec2(float x, float y)
			: x(x), y(y) {}
		Vec2(float scalar)
			: x(scalar), y(scalar) {}

		union
		{
			float xy[2] = { 0 };
			struct
			{
				float x, y;
			};
		};
	};

	static inline Vec2 Vec2Add(const Vec2& v1, const Vec2& v2)
	{
		return Vec2(v1.x + v2.x, v1.y + v2.y);
	}

	static inline Vec2 Vec2Sub(const Vec2& v1, const Vec2& v2)
	{
		return Vec2(v1.x - v2.x, v1.y - v2.y);
	}

	static inline Vec2 Vec2MulScalar(const Vec2& v, float s)
	{
		return Vec2(v.x * s, v.y * s);
	}

	// ----------------------------------------------------------------------------
	// Vec3

	struct Vec3
	{
		Vec3() = default;
		Vec3(float x, float y, float z)
			: x(x), y(y), z(z) {}
		Vec3(float scalar)
			: x(scalar), y(scalar), z(scalar) {}

		union
		{
			float xyz[3] = { 0 };
			Vec2 xy;
			struct
			{
				float x, y, z;
			};
		};
	};

	static inline Vec3 Vec3Add(const Vec3& v1, const Vec3& v2)
	{
		return Vec3(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z);
	}

	static inline Vec3 Vec3Sub(const Vec3& v1, const Vec3& v2)
	{
		return Vec3(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z);
	}

	static inline Vec3 Vec3MulScalar(const Vec3& v, float s)
	{
		return Vec3(v.x * s, v.y * s, v.z * s);
	}

	static inline float Vec3Dot(const Vec3& v1, const Vec3& v2)
	{
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	static inline Vec3 Vec3Cross(const Vec3& v1, const Vec3& v2)
	{
		Vec3 result;
		result.x = v1.y * v2.z - v1.z * v2.y;
		result.y = v1.z * v2.x - v1.x * v2.z;
		result.z = v1.x * v2.y - v1.y * v2.x;
		return result;
	}

	// ----------------------------------------------------------------------------
	// Vec4

	struct Vec4
	{
		Vec4() = default;
		Vec4(float x, float y, float z, float w)
			: x(x), y(y), z(z), w(w) {}
		Vec4(float scalar)
			: x(scalar), y(scalar), z(scalar), w(scalar) {}

		union
		{
			float xyzw[4] = { 0 };
			Vec2 xy;
			Vec3 xyz;
			struct
			{
				float x, y, z, w;
			};
		};
	};

	static inline Vec4 Vec4Add(const Vec4& v1, const Vec4& v2)
	{
		return Vec4(v1.x + v2.x, v1.y + v2.y, v1.z + v2.z, v1.w + v2.w);
	}

	static inline Vec4 Vec4Sub(const Vec4& v1, const Vec4& v2)
	{
		return Vec4(v1.x - v2.x, v1.y - v2.y, v1.z - v2.z, v1.w - v2.w);
	}

	static inline Vec4 Vec4MulScalar(const Vec4& v, float s)
	{
		return Vec4(v.x * s, v.y * s, v.z * s, v.w * s);
	}

	// ----------------------------------------------------------------------------
	// Mat4x4

	struct Mat4x4
	{
		Mat4x4() = default;
		Mat4x4(const Vec4& r0, const Vec4& r1, const Vec4& r2, const Vec4& r3)
			: r0(r0), r1(r1), r2(r2), r3(r3) {}
		Mat4x4(float r00, float r01, float r02, float r03,
			   float r10, float r11, float r12, float r13,
			   float r20, float r21, float r22, float r23,
			   float r30, float r31, float r32, float r33)
			: r0(Vec4(r00, r01, r02, r03)), r1(Vec4(r10, r11, r12, r13)),
			  r2(r20, r21, r22, r23), r3(Vec4(r30, r31, r32, r33)) {}

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

	static Mat4x4 Mat4x4Inverse(const Mat4x4& m)
	{
		Vec3 a = Vec3(m.v[0][0], m.v[1][0], m.v[2][0]);
		Vec3 b = Vec3(m.v[0][1], m.v[1][1], m.v[2][1]);
		Vec3 c = Vec3(m.v[0][2], m.v[1][2], m.v[2][2]);
		Vec3 d = Vec3(m.v[0][3], m.v[1][3], m.v[2][3]);

		float x = m.v[3][0];
		float y = m.v[3][1];
		float z = m.v[3][2];
		float w = m.v[3][3];

		Vec3 s = Vec3Cross(a, b);
		Vec3 t = Vec3Cross(c, d);

		Vec3 u = Vec3Sub(Vec3MulScalar(a, y), Vec3MulScalar(b, x));
		Vec3 v = Vec3Sub(Vec3MulScalar(c, w), Vec3MulScalar(d, z));

		float inv_det = 1.0f / (Vec3Dot(s, v) + Vec3Dot(t, u));
		s = Vec3MulScalar(s, inv_det);
		t = Vec3MulScalar(t, inv_det);
		u = Vec3MulScalar(u, inv_det);
		v = Vec3MulScalar(v, inv_det);

		Vec3 r0 = Vec3Add(Vec3Cross(b, v), Vec3MulScalar(t, y));
		Vec3 r1 = Vec3Sub(Vec3Cross(v, a), Vec3MulScalar(t, x));
		Vec3 r2 = Vec3Add(Vec3Cross(d, u), Vec3MulScalar(s, w));
		Vec3 r3 = Vec3Sub(Vec3Cross(u, c), Vec3MulScalar(s, z));

		return Mat4x4(
			r0.x, r0.y, r0.z, -Vec3Dot(b, t),
			r1.x, r1.y, r1.z, Vec3Dot(a, t),
			r2.x, r2.y, r2.z, -Vec3Dot(d, s),
			r3.x, r3.y, r3.z, Vec3Dot(c, s)
		);
	}

}
