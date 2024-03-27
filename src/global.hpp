#pragma once

#include "config.hpp"
#include "window.hpp"
#include "soundHandler.hpp"
#include "textureData.hpp"
#include "popups.hpp"
#include "playlists.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <future>
#include <filesystem>
#include <miniaudio.h>

extern "C" {
    #include <leif.h>
}

#define MAX(a, b) a > b ? a : b
#define MIN(a, b) a < b ? a : b

enum class GuiTab {
    Dashboard = 0, 
    CreatePlaylist,
    CreatePlaylistFromFolder,
    DownloadPlaylist,
    OnPlaylist,
    OnTrack,
    PlaylistAddFromFile,
    PlaylistAddFromFolder,
    PlaylistSetThumbnail,
    TabCount
};

struct InputField {
    LfInputField input;
    char buffer[INPUT_BUFFER_SIZE] = {0};
};

struct CreatePlaylistState {
    InputField nameInput, descInput; 
    std::filesystem::path thumbnailPath;

    FileStatus createFileStatus;
    float createFileMessageShowTime = 3.0f; 
    float createFileMessageTimer = 0.0f;
};

struct PlaylistAddFromFileTab {
    InputField pathInput;

    FileStatus addFileStatus;
    float addFileMessageShowTime = 3.0f; 
    float addFileMessageTimer = 0.0f;
};

struct OnTrackTab {
    LfTexture trackThumbnail;
    bool showVolumeSlider;
};

struct PlaylistAddFromFolderTab {
    std::vector<std::filesystem::directory_entry> folderContents;
    std::wstring currentFolderPath;
};

enum class PopupID {
    EditPlaylistPopup = 0,
    PlaylistFileDialoguePopup,
    PopupCount
};

struct GlobalState {
    Window* win = NULL;
    float deltaTime, lastTime;

    SoundHandler soundHandler;
    SoundFile* currentSoundFile = NULL;
    int32_t currentSoundPos;

    LfFont musicTitleFont;
    LfFont h1Font;
    LfFont h2Font;
    LfFont h3Font;
    LfFont h4Font;
    LfFont h5Font;
    LfFont h6Font;
    LfFont h7Font;

    GuiTab currentTab;

    std::vector<Playlist> playlists;
    std::unordered_map<PopupType, std::unique_ptr<Popup>> popups;

    std::unordered_map<std::string, LfTexture> icons;

    CreatePlaylistState createPlaylistTab;
    PlaylistAddFromFileTab playlistAddFromFileTab;
    PlaylistAddFromFolderTab playlistAddFromFolderTab;
    OnTrackTab onTrackTab;


    int32_t currentPlaylist, playingPlaylist;

    float soundPosUpdateTimer;
    float soundPosUpdateTime;

    LfSlider trackProgressSlider;
    LfSlider volumeSlider;
    bool showVolumeSliderTrackDisplay, showVolumeSliderOverride;

    float volumeBeforeMute;


    // Async loading
    std::vector<std::future<void>> playlistFileFutures;
    std::vector<std::string> loadedPlaylistFilepaths;
    std::vector<TextureData> playlistFileThumbnailData;
    std::mutex mutex;

    bool playlistDownloadRunning, playlistDownloadFinished;
    std::string downloadingPlaylistName;
    uint32_t downloadPlaylistFileCount;
};

extern GlobalState state;

void miniaudioDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCoun);

void changeTabTo(GuiTab tab);
