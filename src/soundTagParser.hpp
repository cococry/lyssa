#pragma once

extern "C" {
    #include <leif.h>
}
#include <string>

struct TextureData {
    unsigned char* data;
    uint32_t width, height; 
    int32_t channels;
    std::string path;
};

LfTexture getSoundThubmnail(const std::string& soundPath, vec2s size_factor = (vec2s){-1, -1});
TextureData getSoundThubmnailData(const std::string& soundPath, vec2s size_factor = (vec2s){-1, -1});
std::wstring getSoundArtist(const std::string& soundPath);
std::string getSoundComment(const std::string& soundPath);
