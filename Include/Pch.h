#pragma once

/*

	PRECOMPILED HEADER

*/

#include <stdio.h>
#include <cstdint>
#include <assert.h>

// Useful defines
#define DXV2_ASSERT(x) assert(x)
#define DXV2_MIN(x, y) ((x) < (y) ? (x) : (y))
#define DXV2_MAX(x, y) ((x) > (y) ? (x) : (y))

// Windows headers
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
