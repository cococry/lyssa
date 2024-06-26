#pragma once 

#include "config.hpp"
#include <filesystem>

extern "C" {
#include <leif/leif.h>
}

#include <string>
#include <vector>

enum class FileStatus {
  None = 0,
  Success,
  Failed,
  AlreadyExists, 
};
struct SoundFile {
  std::filesystem::path path;
  std::string artist, title;
  uint32_t releaseYear;
  int32_t duration;
  LfTexture thumbnail;
  bool loaded;

  float renderPosY;

  bool operator==(const SoundFile& other) const {
    return path == other.path;
  }
};
struct Playlist {
  std::vector<SoundFile> musicFiles;

  std::string name, desc, url;
  // Moving the file that is being dragged  
  std::filesystem::path path, thumbnailPath;
  LfTexture thumbnail;
  int32_t playingFile = -1, selectedFile = -1;

  bool loaded = false;

  bool operator==(const Playlist& other) const { 
    return path == other.path;
  }
  float scroll = 0.0f, scrollVelocity = 0.0f;

  static FileStatus create(const std::string& name, const std::string& desc, const std::string& url = "",
      const std::filesystem::path& thumbnailPath = "");
  static FileStatus rename(const std::string& name, uint32_t playlistIndex);
  static FileStatus remove(uint32_t playlistIndex);
  static FileStatus save(uint32_t playlistIndex);
  static FileStatus changeDesc(const std::string& desc, uint32_t playlistIndex);
  static FileStatus changeThumbnail(const std::filesystem::path& thumbnailPath, uint32_t playlistIndex);
  static FileStatus addFile(const std::filesystem::path& path, uint32_t playlistIndex);
  static FileStatus removeFile(const std::filesystem::path& path, uint32_t playlistIndex);

  static bool containsFile(const std::filesystem::path& path, uint32_t playlistIndex);
  static bool metadataContainsFile(const std::string& path, uint32_t playlistIndex);

};

namespace PlaylistMetadata {
  std::string getName(const std::filesystem::directory_entry& playlistDir); 
  std::string getDesc(const std::filesystem::directory_entry& playlistDir); 
  std::string getUrl(const std::filesystem::directory_entry& playlistDir); 
  std::string getThumbnailPath(const std::filesystem::directory_entry& playlistDir); 
  std::vector<std::string> getFilepaths(const std::filesystem::directory_entry& playlistDir); 
}


