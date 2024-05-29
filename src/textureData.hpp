#pragma once

#include <string>
#include <stdint.h>
#include <filesystem>

struct TextureData {
  unsigned char* data;
  uint32_t width, height; 
  int32_t channels;
  std::filesystem::path path;
};

