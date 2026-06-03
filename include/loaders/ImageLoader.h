#pragma once                                     // Include this header only once per translation unit

#include <cstdint>                               // Fixed-width integer types (uint8_t, uint32_t)
#include <string>                                // std::string for the file path
#include <vector>                                // std::vector for the pixel buffer

// =============================================================================
// ImageLoader.h — Simple RGBA image loading
//
// Declares a minimal image loader that reads an image file from disk into a
// CPU-side RGBA8 pixel buffer (via stb_image). ImageData holds the decoded
// pixels plus dimensions; the renderer uploads these into Vulkan textures.
// =============================================================================

// *** Michael Salton ***

struct ImageData {                               // Decoded image held in CPU memory
    std::vector<uint8_t> pixels;  // RGBA, 4 bytes per pixel // Tightly packed RGBA8 pixel data
    uint32_t width = 0;                          // Image width in pixels
    uint32_t height = 0;                         // Image height in pixels
};

class ImageLoader {                              // Stateless image-loading utility
public:
    static ImageData load(const std::string& filepath); // Load an image file into an RGBA8 ImageData
};

// *** ************ ***
