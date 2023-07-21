#pragma once

#include <cstdint>

namespace Hash
{

	static inline uint32_t RT_Rotl32(uint32_t x, int8_t r)
	{
		return (x << r) | (x >> (32 - r));
	}

	static inline uint32_t RT_FMix(uint32_t h)
	{
		h ^= h >> 16; h *= 0x85ebca6b;
		h ^= h >> 13; h *= 0xc2b2ae35;
		return h ^= h >> 16;
	}

	static uint32_t DJB2(const void* in, uint32_t len)
	{
		uint8_t* data = (uint8_t*)in;
		uint32_t hash = 5381;
		int c;

		while (c = *data++)
		{
			hash = ((hash << 5) + hash) + c;
		}

		return hash;
	}

	static uint32_t Murmur2_32(const void* in, uint32_t len, uint32_t seed)
	{
		const uint32_t m = 0x5bd1e995;
		const int r = 24;

		uint8_t* data = (uint8_t*)in;
		uint32_t hash = seed ^ len;

		while (len >= 4)
		{
			uint32_t k = *(uint32_t*)data;

			k *= m;
			k ^= k >> r;
			k *= m;

			hash *= m;
			hash ^= k;

			data += 4;
			len -= 4;
		}

		switch (len)
		{
		case 3: hash ^= data[2] << 16;
		case 2: hash ^= data[1] << 8;
		case 1: hash ^= data[0];
			hash *= m;
		}

		hash ^= hash >> 13;
		hash *= m;
		hash ^= hash >> 15;

		return hash;
	}

	static uint32_t Murmur3_32(const void* in, uint32_t len, uint32_t seed)
	{
		const uint8_t* tail = (const uint8_t*)in + (len / 4) * 4;
		uint32_t c1 = 0xcc9e2d51, c2 = 0x1b873593;

		for (uint32_t* p = (uint32_t*)in; p < (const uint32_t*)tail; p++)
		{
			uint32_t k1 = *p; k1 *= c1; k1 = RT_Rotl32(k1, 15); k1 *= c2; // MUR1
			seed ^= k1; seed = RT_Rotl32(seed, 13); seed = seed * 5 + 0xe6546b64; // MUR2
		}

		uint32_t t = 0; // handle up to 3 tail bytes
		switch (len & 3)
		{
			case 3: t ^= tail[2] << 16;
			case 2: t ^= tail[1] << 8;
			case 1: {t ^= tail[0]; t *= c1; t = RT_Rotl32(t, 15); t *= c2; seed ^= t; };
		}
		return RT_FMix(seed ^ len);
	}

}
