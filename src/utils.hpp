#pragma once

#include <string>
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
        std::string cmd = "yt-dlp --compat-options no-youtube-unavailable-videos --flat-playlist -j --no-warnings \"" + url + "\"" + " | grep -o '\"title\": \"[^\"]*' | wc -l";
        return (uint32_t)std::stoi(getCommandOutput(cmd));
    }
}
