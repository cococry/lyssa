#pragma once

#include <string>
#include <algorithm>
#include <fstream>
#include <iostream>

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
  
  static bool isValidInt(const std::string& str) {
    if (str.empty()) {
      return false;
    }

    size_t pos = 0;
    if (str[0] == '-' || str[0] == '+') {
      pos = 1;
    }

    if (pos == str.size()) {
      return false;
    }

    for (; pos < str.size(); ++pos) {
      if (!std::isdigit(str[pos])) {
        return false;
      }
    }

    try {
      std::size_t lastChar;
      int number = std::stoi(str, &lastChar);
      if (lastChar != str.size()) {
        return false; 
      }
    } catch (const std::invalid_argument&) {
      return false; 
    } catch (const std::out_of_range&) {
      return false; 
    }

    return true;
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
    std::string cmd = LYSSA_DIR + "/scripts/count-files.sh " + "\"" +  url + "\"";
    std::string output = getCommandOutput(cmd);
    if(isValidInt(output)) {
      return std::stoi(output);
    } else {
      return 0;
    }
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
