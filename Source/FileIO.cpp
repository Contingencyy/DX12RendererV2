#include "Pch.h"
#include "FileIO.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"

namespace FileIO
{

    LoadImageResult LoadImage(const char* filepath)
    {
        LoadImageResult result = {};

        int width, height, bpp;
        result.bytes = stbi_load(filepath, &width, &height, &bpp, STBI_rgb_alpha);
        result.width = (uint32_t)width;
        result.height = (uint32_t)height;
        result.bpp = (uint32_t)bpp;

        return result;
    }

}
