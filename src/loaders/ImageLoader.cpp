#define STB_IMAGE_IMPLEMENTATION                 // Emit stb_image's function bodies in this translation unit (single-header lib)
#include <stb_image.h>                           // stb_image: decodes PNG/JPG/etc. into raw pixels

#include "loaders/ImageLoader.h"                 // ImageLoader / ImageData declarations
#include <stdexcept>                             // std::runtime_error on load failure
#include <iostream>                              // Console logging

// =============================================================================
// ImageLoader.cpp — RGBA image loading via stb_image
//
// Implements ImageLoader::load: decodes any stb_image-supported file to RGBA8,
// copies it into an ImageData buffer, frees the stb allocation, and returns it.
// Defining STB_IMAGE_IMPLEMENTATION here compiles the stb_image library into
// this one object file.
// =============================================================================

// *** Michael Salton ***

ImageData ImageLoader::load(const std::string& filepath) { // Load and decode an image to RGBA8
    int width, height, channels;                 // Out-params filled by stb_image
    stbi_uc* pixels = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha); // Decode, forcing 4 channels (RGBA)

    if (!pixels) {                               // Null result = decode/open failure
        throw std::runtime_error("Failed to load image: " + filepath);
    }

    ImageData data;                              // Output structure
    data.width = static_cast<uint32_t>(width);   // Record dimensions
    data.height = static_cast<uint32_t>(height);
    size_t imageSize = static_cast<size_t>(width) * height * 4; // Total bytes (4 per pixel, forced RGBA)
    data.pixels.assign(pixels, pixels + imageSize); // Copy stb's buffer into the owned vector

    stbi_image_free(pixels);                     // Free stb's allocation (data now owns its own copy)

    std::cout << "Loaded image: " << filepath    // Log the load result
              << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;

    return data;                                 // Return the decoded image
}

// *** ************ ***
