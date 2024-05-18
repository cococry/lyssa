#pragma once

extern "C" {
    #include <leif/leif.h>
}
#include <string>

#include "textureData.hpp"

struct SoundMetadata {
    std::string artist, title;
    std::string comment;
    TextureData thumbnailData;
    uint32_t releaseYear;
    double duration;
};

namespace SoundTagParser {
    LfTexture getSoundThubmnail(const std::string& soundPath, vec2s size_factor = (vec2s){-1, -1});
    TextureData getSoundThubmnailData(const std::string& soundPath, vec2s size_factor = (vec2s){-1, -1});
    std::string getSoundArtist(const std::string& soundPath);
    std::string getSoundAlbum(const std::string& soundPath);
    std::string getSoundTitle(const std::string& soundPath);
    int32_t getSoundDuration(const std::string& soundPath);
    uint32_t getSoundReleaseYear(const std::string& soundPath);
    std::string getSoundComment(const std::string& soundPath);
    SoundMetadata getSoundMetadata(const std::string& soundPath);
    SoundMetadata getSoundMetadataNoThumbnail(const std::string& soundPath);
    bool isValidSoundFile(const std::string& path);
}
