#pragma once

extern "C" {
    #include <leif.h>
}
#include <string>

#include "textureData.hpp"

namespace SoundTagParser {
    LfTexture getSoundThubmnail(const std::string& soundPath, vec2s size_factor = (vec2s){-1, -1});
    TextureData getSoundThubmnailData(const std::string& soundPath, vec2s size_factor = (vec2s){-1, -1});
    std::wstring getSoundArtist(const std::string& soundPath);
    std::wstring getSoundAlbum(const std::string& soundPath);
    uint32_t getSoundReleaseYear(const std::string& soundPath);
    std::string getSoundComment(const std::string& soundPath);
    bool isValidSoundFile(const std::string& path); 
}
