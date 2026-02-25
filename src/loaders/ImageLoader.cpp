#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "loaders/ImageLoader.h"
#include <stdexcept>
#include <iostream>

ImageData ImageLoader::load(const std::string& filepath) {
    int width, height, channels;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels) {
        throw std::runtime_error("Failed to load image: " + filepath);
    }

    ImageData data;
    data.width = static_cast<uint32_t>(width);
    data.height = static_cast<uint32_t>(height);
    size_t imageSize = static_cast<size_t>(width) * height * 4;
    data.pixels.assign(pixels, pixels + imageSize);

    stbi_image_free(pixels);

    std::cout << "Loaded image: " << filepath
              << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;

    return data;
}
