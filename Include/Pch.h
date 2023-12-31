#pragma once

/*

	PRECOMPILED HEADER

*/

#include "LinearAllocator.h"
#include "DXMath.h"
#include "Hash.h"
#include "CPUProfiler.h"

using namespace DXMath;

#include <stdio.h>
#include <cstdint>
#include <assert.h>

// Useful defines
#define DX_ASSERT(x) assert(x)
#define DX_MIN(x, y) ((x) < (y) ? (x) : (y))
#define DX_MAX(x, y) ((x) > (y) ? (x) : (y))

#define DX_KB(x) ((x) << 10)
#define DX_MB(x) ((x) << 20)
#define DX_GB(x) ((x) << 30)

#define DX_TO_KB(x) ((x) >> 10)
#define DX_TO_MB(x) ((x) >> 20)
#define DX_TO_GB(x) ((x) >> 30)

#define DX_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define DX_ALIGN_POW2(x, align) ((intptr_t)(x) + ((align) - 1) & (-(intptr_t)(align)))
#define DX_ALIGN_DOWN_POW2(x, align) ((intptr_t)(x) & (-(intptr_t)(align)))

#define DX_GPU_VALIDATION 0

// DirectX, DXC
#include "D3D12Agility/build/native/include/d3d12.h"
//#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3d12shader.h>
#include "DXC/inc/dxcapi.h"

// Windows
#define NOMINMAX
#define WINDOWS_LEAN_AND_MEAN
#include <Windows.h>

// Undef all these windows defines..
#ifdef CreateWindow
#undef CreateWindow
#endif

#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif

#ifdef LoadImage
#undef LoadImage
#endif

#ifdef OPAQUE
#undef OPAQUE
#endif

#ifdef TRANSPARENT
#undef TRANSPARENT
#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif
