#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct ImageData {
    std::vector<uint8_t> pixels;  // RGBA, 4 bytes per pixel
    uint32_t width = 0;
    uint32_t height = 0;
};

class ImageLoader {
public:
    static ImageData load(const std::string& filepath);
};
