#pragma once

namespace FileIO
{

	struct LoadImageResult
	{
		uint32_t width;
		uint32_t height;
		uint8_t* bytes;
	};

	LoadImageResult LoadImage(const char* filepath);

}
