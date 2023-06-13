#pragma once

/*

	PRECOMPILED HEADER

*/

#include <stdio.h>
#include <cstdint>
#include <assert.h>

// Useful defines
#define DX_ASSERT(x) assert(x)
#define DX_MIN(x, y) ((x) < (y) ? (x) : (y))
#define DX_MAX(x, y) ((x) > (y) ? (x) : (y))

// Windows headers
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

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
