#include "playlists.hpp"
#include "global.hpp"
#include "soundHandler.hpp"
#include "soundTagParser.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>


FileStatus Playlist::create(const std::string& name, const std::string& desc, const std::string& url) {
    std::string folderPath = LYSSA_DIR + "/playlists/" + name;
    if(!std::filesystem::exists(folderPath)) {
        if(!std::filesystem::create_directory(folderPath) )
            return FileStatus::Failed;
    } else {
        return FileStatus::AlreadyExists;
    }

    std::ofstream metadata(folderPath + "/.metadata");
    if(metadata.is_open()) {
        metadata << "name: " << name << "\n";
        metadata << "desc: " << desc << "\n";
        metadata << "url: " << url << "\n";
        metadata << "files: ";
    } else {
        return FileStatus::Failed;
    }
    metadata.close();

    return FileStatus::Success;
}
FileStatus Playlist::rename(const std::string& name, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    playlist.name = name; 
    return Playlist::save(playlistIndex);
}
FileStatus Playlist::remove(uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];

    if(!std::filesystem::exists(playlist.path) || !std::filesystem::is_directory(playlist.path)) return FileStatus::Failed;

    std::filesystem::remove_all(playlist.path);
    state.playlists.erase(std::find(state.playlists.begin(), state.playlists.end(), playlist));

    return FileStatus::Success;
}

FileStatus Playlist::changeDesc(const std::string& desc, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    playlist.desc = desc; 
    return Playlist::save(playlistIndex);
}

FileStatus Playlist::save(uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    std::ofstream metdata(playlist.path.string() + "/.metadata", std::ios::trunc);

    if(!metdata.is_open()) return FileStatus::Failed;

    metdata << "name: " << playlist.name << "\n";
    metdata << "desc: " << playlist.desc << "\n";
    metdata << "url: " << playlist.url << "\n";
    metdata << "files: ";

    for(auto& file : playlist.musicFiles) {
        metdata << "\"" << file.path.string() << "\" ";
    }
    return FileStatus::Success;

}
FileStatus Playlist::addFile(const std::filesystem::path& path, uint32_t playlistIndex) {
    if(Playlist::containsFile(path, playlistIndex)) return FileStatus::AlreadyExists;

    Playlist& playlist = state.playlists[playlistIndex];

    std::ofstream metadata(playlist.path.string() + "/.metadata", std::ios::app);

    if(!metadata.is_open()) return FileStatus::Failed;

    std::ifstream playlistFile(path);
    if(!playlistFile.good()) return FileStatus::Failed;

    metadata.seekp(0, std::ios::end);

    metadata << "\"" << path << "\" ";
    metadata.close();

    state.loadedPlaylistFilepaths.emplace_back(path);

    playlist.musicFiles.emplace_back((SoundFile){
            .path = path,  
            .duration = static_cast<int32_t>(SoundHandler::getSoundDuration(path)),
            .thumbnail = SoundTagParser::getSoundThubmnail(path, (vec2s){0.1, 0.1})
            });

    return FileStatus::Success;
}

FileStatus Playlist::removeFile(const std::filesystem::path& path, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    if(!Playlist::containsFile(path, playlistIndex)) return FileStatus::Failed;

    for(auto& file : playlist.musicFiles) {
        if(file.path == path) {
            playlist.musicFiles.erase(std::find(playlist.musicFiles.begin(), playlist.musicFiles.end(), file));
            state.loadedPlaylistFilepaths.erase(std::find(state.loadedPlaylistFilepaths.begin(), state.loadedPlaylistFilepaths.end(), path));
            break;
        }
    }
    return Playlist::save(playlistIndex);
}

bool Playlist::containsFile(const std::filesystem::path& path, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    for(auto& file : playlist.musicFiles) {
        if(file.path == path) return true;
    }
    return false;
}
bool Playlist::metadataContainsFile(const std::string& path, uint32_t playlistIndex) {
    std::ifstream file(state.playlists[playlistIndex].path.string() + "/.metadata");
    std::string line;

    if (file.is_open()) {
        while (std::getline(file, line)) {
            if (line.find(path) != std::string::npos) {
                file.close();
                return true;
            }
        }
        file.close();
    } else {
        LOG_ERROR("[Error] Failed to open the metadata of playlist on path '%s'\n", state.playlists[playlistIndex].path.c_str());
    }

    return false;
}
std::string PlaylistMetadata::getName(const std::filesystem::directory_entry& playlistDir) {
    std::ifstream metadata(playlistDir.path().string() + "/.metadata");

    if(!metadata.is_open()) {
        LOG_ERROR("Failed to open the metadata of playlist on path '%s'\n", playlistDir.path().string().c_str());
        return "No valid name";
    }

    std::string name;
    std::string line;
    while(std::getline(metadata, line)) {
        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if(key == "name:") {
            std::getline(iss, name);
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
        }
    }

    return name;
}

std::string PlaylistMetadata::getDesc(const std::filesystem::directory_entry& playlistDir) {
    std::ifstream metadata(playlistDir.path().string() + "/.metadata");

    if(!metadata.is_open()) {
        LOG_ERROR("Failed to open the metadata of playlist on path '%s'\n", playlistDir.path().string().c_str());
        return "No valid description";
    }

    std::string desc;
    std::string line;
    while(std::getline(metadata, line)) {
        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if(key == "desc:") {
            std::getline(iss, desc);
            desc.erase(0, desc.find_first_not_of(" \t"));
            desc.erase(desc.find_last_not_of(" \t") + 1);
        }
    }
    return desc;
}

std::string PlaylistMetadata::getUrl(const std::filesystem::directory_entry& playlistDir) {
    std::ifstream metadata(playlistDir.path().string() + "/.metadata");

    if(!metadata.is_open()) {
        LOG_ERROR("Failed to open the metadata of playlist on path '%s'\n", playlistDir.path().string().c_str());
        return "";
    }

    std::string url = "";
    std::string line;
    while(std::getline(metadata, line)) {
        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if(key == "url:") {
            std::getline(iss, url);
            url.erase(0, url.find_first_not_of(" \t"));
            url.erase(url.find_last_not_of(" \t") + 1);
        }
    }
    return url;
}
std::vector<std::string> PlaylistMetadata::getFilepaths(const std::filesystem::directory_entry& playlistDir) {
    std::ifstream metadata(playlistDir.path().string() + "/.metadata");
    std::vector<std::string> filepaths{};

    if(!metadata.is_open()) {
        LOG_ERROR("Failed to open the metadata of playlist on path '%s'\n", playlistDir.path().string().c_str());
        return filepaths;
    }

    std::string line;
    while(std::getline(metadata, line)) {
        std::istringstream iss(line);
        std::string key;
        iss >> key;

        if (line.find("files:") != std::string::npos) {
            // If the line contains "files:", extract paths from the rest of the line
            std::istringstream iss(line.substr(line.find("files:") + std::string("files:").length()));
            std::string path;

            while (iss >> std::quoted(path)) {
                filepaths.emplace_back(path);
            }
        }
    }
    return filepaths;
}

