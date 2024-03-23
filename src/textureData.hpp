#pragma once

#include <string>
#include <stdint.h>

struct TextureData {
    unsigned char* data;
    uint32_t width, height; 
    int32_t channels;
    std::string path;
};

