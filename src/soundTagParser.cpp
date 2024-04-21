#include "soundTagParser.hpp"
#include "log.hpp"
#include "soundHandler.hpp"
#include "utils.hpp"

#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/mpegheader.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/tfile.h>

#include <iostream>

using namespace TagLib;

namespace SoundTagParser {
    LfTexture getSoundThubmnail(const std::string& soundPath, vec2s size_factor) {
        MPEG::File file(soundPath.c_str());

        LfTexture tex = {0};
        // Get the ID3v2 tag
        ID3v2::Tag *tag = file.ID3v2Tag();
        if (!tag) {
            LOG_ERROR("No ID3v2 tag found for file '%s'.\n", soundPath.c_str());
            return tex;
        }

        // Get the first APIC (Attached Picture) frame
        ID3v2::FrameList apicFrames = tag->frameListMap()["APIC"];
        if (apicFrames.isEmpty()) {
            LOG_ERROR("No APIC frame found for file '%s'.\n", soundPath.c_str());
            return tex;
        }

        // Extract the image data
        ID3v2::AttachedPictureFrame *apicFrame = dynamic_cast<ID3v2::AttachedPictureFrame *>(apicFrames.front());
        if (!apicFrame) {
            LOG_ERROR("Failed to cast APIC frame for file '%s'.\n", soundPath.c_str());
            return tex;
        }

        ByteVector imageData = apicFrame->picture();

        if(size_factor.x == -1 || size_factor.y == -1)
            tex = lf_load_texture_from_memory(imageData.data(), (int)imageData.size(), true, LF_TEX_FILTER_LINEAR);
        else 
            tex = lf_load_texture_from_memory_resized(imageData.data(), (int)imageData.size(), true, LF_TEX_FILTER_LINEAR, (uint32_t)size_factor.x, (uint32_t)size_factor.y);

        return tex;
    }

    TextureData getSoundThubmnailData(const std::string& soundPath, vec2s size_factor) {
        MPEG::File file(soundPath.c_str());
        TextureData retData{};

        // Get the ID3v2 tag
        ID3v2::Tag *tag = file.ID3v2Tag();
        if (!tag) {
            LOG_ERROR("No ID3v2 tag found for file '%s'.\n", soundPath.c_str());
            return retData;
        }

        // Get the first APIC (Attached Picture) frame
        ID3v2::FrameList apicFrames = tag->frameListMap()["APIC"];
        if (apicFrames.isEmpty()) {
            LOG_ERROR("No APIC frame found for file '%s'.\n", soundPath.c_str());
            return retData;
        }

        // Extract the image data
        ID3v2::AttachedPictureFrame *apicFrame = dynamic_cast<ID3v2::AttachedPictureFrame *>(apicFrames.front());
        if (!apicFrame) {
            LOG_ERROR("Failed to cast APIC frame for file '%s'.\n", soundPath.c_str());
            return retData;
        }

        ByteVector imageData = apicFrame->picture();

        if(size_factor.x == -1 || size_factor.y == -1) {
            retData.data = lf_load_texture_data_from_memory(imageData.data(), (size_t)imageData.size(), (int32_t*)&retData.width, (int32_t*)&retData.height, &retData.channels, true); 
        } else  {
            retData.data = lf_load_texture_data_from_memory_resized_to_fit(imageData.data(), (size_t)imageData.size(), 
                (int32_t*)&retData.width, (int32_t*)&retData.height, &retData.channels, true, size_factor.x, size_factor.y); 
        }
        retData.path = soundPath;

        return retData;
    }

    std::wstring getSoundArtist(const std::string& soundPath) {
        FileRef file(soundPath.c_str());

        if (!file.isNull() && file.tag()) {
            Tag *tag = file.tag();

            std::wstring artist = tag->artist().toWString();
            return artist;
        } else {
            return L"-";
        }
        return L"-";
    }
    std::wstring getSoundAlbum(const std::string& soundPath) {
        FileRef file(soundPath.c_str());

        if (!file.isNull() && file.tag()) {
            Tag *tag = file.tag();

            return (tag->album().toWString() != L"") ? tag->album().toWString() : L"-";
        } else {
            return L"None";
        }
        return L"None";
    }
    std::wstring getSoundTitle(const std::string& soundPath) {
      FileRef file(soundPath.c_str());

      if (!file.isNull() && file.tag()) {
        Tag *tag = file.tag();

        return tag->title().toWString();
      } 
      return L"";
    }
    int32_t getSoundDuration(const std::string& soundPath) {
      TagLib::FileRef fileRef(soundPath.c_str());

      if (!fileRef.isNull() && fileRef.audioProperties()) {
        TagLib::AudioProperties *properties = fileRef.audioProperties();
        int durationInSeconds = properties->length();
        return durationInSeconds;
      } else {
        return 0;
      }
    }
    uint32_t getSoundReleaseYear(const std::string& soundPath) {
        FileRef file(soundPath.c_str());

        if (!file.isNull() && file.tag()) {
            Tag *tag = file.tag();

            return tag->year();
        } 
        return 0;
    }

    std::string getSoundComment(const std::string& soundPath) {
        std::string cmd = "exiftool -s -s -s -UserDefinedText \"" + soundPath + "\" | awk '{print $2}'";
        return LyssaUtils::getCommandOutput(cmd);
    }
    bool isValidSoundFile(const std::string &path) {
        TagLib::FileRef file(path.c_str());
        return !file.isNull() && file.audioProperties();
    }
    SoundMetadata getSoundMetadata(const std::string& soundPath) {
        SoundMetadata metadata;
        metadata.thumbnailData = getSoundThubmnailData(soundPath, (vec2s){120, 80});
        metadata.duration = SoundHandler::getSoundDuration(soundPath);

        FileRef file(soundPath.c_str());

        if (!file.isNull() && file.tag()) {
            Tag *tag = file.tag();

            metadata.artist = tag->artist().toWString() == L"" ? L"-" : tag->artist().toWString();
            metadata.releaseYear = tag->year();
        } else {
            metadata.artist = L"-";
            metadata.releaseYear = 0;
        }
        metadata.comment = getSoundComment(soundPath);
        
        return metadata;
    }
}
