#pragma once

#include <string>
#include <algorithm>
#include <fstream>
#include <stdint.h>

namespace LyssaUtils {
  static std::string getCommandOutput(const std::string& cmd) {
    FILE* pipe = popen(cmd.c_str(), "r");
    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
      if (fgets(buffer, 128, pipe) != NULL)
        result += buffer;
    }
    pclose(pipe);
    if (!result.empty() && result[result.length() - 1] == '\n') {
      result.erase(result.length() - 1);
    }
    return result;
  }
  static uint32_t getLineCountFile(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    uint32_t lineCount = 0;

    if (file.is_open()) {
      while (std::getline(file, line)) {
        lineCount++;
      }
      file.close();
    } else {
      return 0;
    }

    return lineCount;
  }
  static uint32_t getPlaylistFileCountURL(const std::string& url) {
    std::string cmd = "yt-dlp --compat-options no-youtube-unavailable-videos --flat-playlist -j --no-warnings \"" + url + "\"" + "| jq -c .n_entries | head -n 1";
    return (uint32_t)std::stoi(getCommandOutput(cmd));
  }
  static std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c){ return std::tolower(c); });
    return result;
  }
  static std::wstring toLowerW(const std::wstring& str) {
    std::wstring result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](wchar_t c){ return std::towlower(c); });
    return result;
  }
}
