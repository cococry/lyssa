#pragma once

#include "config.hpp"
#include "window.hpp"
#include "soundHandler.hpp"
#include "textureData.hpp"
#include "popups.hpp"
#include "playlists.hpp"
#include "infoCard.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <future>
#include <filesystem>
#include <miniaudio.h>

extern "C" {
#include <leif/leif.h>
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
  TrackFullscreen,
  PlaylistAddFromFile,
  PlaylistAddFromFolder,
  PlaylistSetThumbnail,
  SearchPlaylist,
  SearchAll,
  TabCount
};

enum class DashboardTab {
  Home = 0,
  Favourites,
  Search
};

struct InputField {
  LfInputField input;
  char buffer[INPUT_BUFFER_SIZE] = {0};
};

struct CreatePlaylistState {
  InputField nameInput, descInput; 
  std::filesystem::path thumbnailPath;
};

struct PlaylistAddFromFileTab {
  InputField pathInput;

  FileStatus addFileStatus;
  float addFileMessageShowTime = 3.0f; 
  float addFileMessageTimer = 0.0f;
};

struct TrackFullscreenTab {
  float uiTime = FULLSCREEN_TRACK_UI_TIME;
  float uiTimer = 0.0f;
  bool showUI = true;
};

struct OnTrackTab {
  LfTexture trackThumbnail;
};

struct PlaylistAddFromFolderTab {
  std::vector<std::filesystem::directory_entry> folderContents;
  std::string currentFolderPath;
  bool addedFile;
};

enum class PopupID {
  EditPlaylistPopup = 0,
  PlaylistFileDialoguePopup,
  PopupCount
};

struct GlobalState {
  Window* win = NULL;
  float deltaTime, lastTime;

  float sideNavigationWidth; 

  SoundHandler soundHandler;
  InfoCardHandler infoCards;

  SoundFile* currentSoundFile = NULL, *previousSoundFile = NULL;
  int32_t currentSoundPos, previousSoundPos;

  LfFont musicTitleFont,
         h1Font,
         h2Font,
         h3Font,
         h4Font,
         h5Font,
         h5BoldFont, h6BoldFont,
         h6Font,
         h7Font;

  GuiTab currentTab = GuiTab::TabCount, previousTab = GuiTab::TabCount;
  DashboardTab dashboardTab;

  std::vector<Playlist> playlists;
  std::unordered_map<PopupType, std::unique_ptr<Popup>> popups;

  std::vector<uint32_t> alreadyPlayedTracks;
  uint32_t skipDownAmount;

  std::unordered_map<std::string, LfTexture> icons;

  CreatePlaylistState createPlaylistTab;
  PlaylistAddFromFileTab playlistAddFromFileTab;
  PlaylistAddFromFolderTab playlistAddFromFolderTab;
  OnTrackTab onTrackTab;
  TrackFullscreenTab trackFullscreenTab;


  int32_t currentPlaylist, playingPlaylist;

  float soundPosUpdateTimer;
  float soundPosUpdateTime;

  LfSlider trackProgressSlider;
  LfSlider volumeSlider;
  bool showVolumeSliderTrackDisplay, showVolumeSliderOverride;

  float volumeBeforeMute;


  // Async loading
  std::vector<std::future<void>> playlistFileFutures;
  std::vector<std::future<void>> playlistFutures;
  std::vector<std::string> loadedPlaylistFilepaths;
  std::vector<TextureData> playlistFileThumbnailData;
  std::vector<TextureData> playlistThumbnailData;
  std::mutex mutex;


  bool playlistDownloadRunning, playlistDownloadFinished;
  int32_t playlistThumbnailDownloadIndex;

  std::string downloadingPlaylistName;
  uint32_t downloadPlaylistFileCount;

  bool shuffle, replayTrack;

  InputField searchPlaylistInput;
  std::vector<SoundFile> searchPlaylistResults;
};

extern GlobalState state;

void miniaudioDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCoun);

void changeTabTo(GuiTab tab);
