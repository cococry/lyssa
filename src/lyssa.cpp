#include "config.hpp"
#include "leif.h"
#include "log.hpp"
#include "soundHandler.hpp"
#include "soundTagParser.hpp"
#include "window.hpp"
#include "utils.hpp"

#include <chrono>
#include <cstddef>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/mpegheader.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/tfile.h>

#include <miniaudio.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>


#ifdef _WIN32
#define HOMEDIR "USERPROFILE"
#else
#define HOMEDIR "HOME"
#endif

#define LYSSA_DIR std::string(getenv(HOMEDIR)) + std::string("/.lyssa")


#define LF_PTR (vec2s){lf_get_ptr_x(), lf_get_ptr_y()}

#define MAX_PLAYLIST_NAME_LENGTH 16
#define MAX_PLAYLIST_DESC_LENGTH 512 

#define INPUT_BUFFER_SIZE 512

#define MAX(a, b) a > b ? a : b
#define MIN(a, b) a < b ? a : b

using namespace TagLib;

enum class GuiTab {
    Dashboard = 0, 
    CreatePlaylist,
    CreatePlaylistFromFolder,
    DownloadPlaylist,
    OnPlaylist,
    OnTrack,
    PlaylistAddFromFile,
    PlaylistAddFromFolder,
    TabCount
};

enum class FileStatus {
    None = 0,
    Success,
    Failed,
    AlreadyExists, 
};

struct SoundFile {
    std::wstring path;
    std::string pathStr;
    int32_t duration;
    LfTexture thumbnail;
    bool loaded;

    bool operator==(const SoundFile& other) const {
        return path == other.path;
    }
};
struct Playlist {
    std::vector<SoundFile> musicFiles;

    std::string name, path, desc, url;
    int32_t playingFile = -1;

    bool ordered = !ASYNC_PLAYLIST_LOADING, loaded = false;

    bool operator==(const Playlist& other) const { 
        return path == other.path;
    }
    float scroll = 0.0f, scrollVelocity = 0.0f;
};

struct InputField {
    LfInputField input;
    char buffer[INPUT_BUFFER_SIZE] = {0};
};

struct CreatePlaylistState {
    InputField nameInput, descInput;  

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

typedef void (*PopupRenderCallback)();

struct PlaylistFileDialoguePopup {
    std::string path;
    vec2s pos;
};

struct Popup {
    PopupRenderCallback renderCb;
    bool render;
};

struct AorBPopup {
    uint32_t width;
    std::string title;
    std::string aStr, bStr;
    std::function<void()> aCb;
    std::function<void()> bCb;
    bool render;
};

enum class PopupID {
    EditPlaylistPopup = 0,
    PlaylistFileDialoguePopup,
    PopupCount
};

struct GlobalState {
    Window* win = NULL;
    float deltaTime, lastTime;

    ma_engine soundEngine;

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
    std::vector<Popup> popups;
    AorBPopup aOrBPopup;

    std::unordered_map<std::string, LfTexture> icons;

    CreatePlaylistState createPlaylistTab;
    PlaylistAddFromFileTab playlistAddFromFileTab;
    PlaylistAddFromFolderTab playlistAddFromFolderTab;
    OnTrackTab onTrackTab;

    PlaylistFileDialoguePopup removeFilePopup;

    int32_t currentPlaylist = -1, playingPlaylist = -1;

    float soundPosUpdateTimer = 1.0f;
    float soundPosUpdateTime = 0.0f;

    LfSlider trackProgressSlider;
    LfSlider volumeSlider;
    bool showVolumeSliderTrackDisplay = false, showVolumeSliderOverride = false;

    float volumeBeforeMute = VOLUME_INIT;


    // Async loading
    std::vector<std::future<void>> playlistFileFutures;
    std::vector<std::string> loadedPlaylistFilepaths;
    std::vector<TextureData> playlistFileThumbnailData;
    std::mutex mutex;

    bool playlistDownloadRunning = false, playlistDownloadFinished = false;
    std::string downloadingPlaylistName;
    uint32_t downloadPlaylistFileCount;
};

static GlobalState state;

void miniaudioDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ma_decoder_read_pcm_frames(&state.soundHandler.decoder, pOutput, frameCount, NULL);

    float* outputBuffer = (float*)pOutput;
    for (ma_uint32 i = 0; i < frameCount * pDevice->playback.channels; ++i) {
        outputBuffer[i] *= state.soundHandler.volume / (float)VOLUME_MAX;
    }
}

static void                     winResizeCb(GLFWwindow* window, float width, float height);
static void                     initWin(float width, float height);
static void                     initUI();
static void                     handleTabKeyStrokes();

static void                     renderDashboard();
static void                     renderCreatePlaylist(std::function<void()> onCreateCb = nullptr, std::function<void()> clientUICb = nullptr, std::function<void()> backButtonCb = nullptr);
static void                     renderCreatePlaylistFromFolder();
static void                     renderDownloadPlaylist();
static void                     renderOnPlaylist();
static void                     renderOnTrack();
static void                     renderPlaylistAddFromFile();
static void                     renderPlaylistAddFromFolder();
static void                     renderFileDialogue(
                                    std::function<void(std::filesystem::directory_entry)> clickedEntryCb, 
                                    std::function<void()> clickedBackCb, 
                                    std::function<void()> renderTopBarCb, 
                                    std::function<void(std::filesystem::directory_entry, bool)> renderIconCb, 
                                    std::function<bool(std::filesystem::directory_entry, bool)> renderPerEntryCb, 
                                    const std::vector<std::filesystem::directory_entry>& folderContents, 
                                    bool renderDirectoriesOnly);

static void                     renderAorBPopup(const std::string& title, float popupWidth, const std::string& aStr, const std::string& bStr, void (*aCb)(void), void (*bCb)(void));
static void                     renderEditPlaylistPopup();
static void                     renderPlaylistFileDialoguePopup();


static void                     renderTrackDisplay();

static void                     renderTrackVolumeControl();
static void                     renderTrackProgress();
static void                     renderTrackMenu();

static void                     backButtonTo(GuiTab tab, const std::function<void()>& clickCb = nullptr);
static void                     changeTabTo(GuiTab tab);

static FileStatus               createPlaylist(const std::string& name, const std::string& desc, const std::string& url = "");
static FileStatus               renamePlaylist(const std::string& name, uint32_t playlistIndex);
static FileStatus               deletePlaylist(uint32_t playlistIndex);
static FileStatus               changePlaylistDesc(const std::string& desc, uint32_t playlistIndex);
static FileStatus               savePlaylist(uint32_t playlistIndex);
static FileStatus               addFileToPlaylist(const std::string& path, uint32_t playlistIndex);
static FileStatus               removeFileFromPlaylist(const std::string& path, uint32_t playlistIndex);

static bool                     isFileInPlaylist(const std::string& path, uint32_t playlistIndex);
static bool                     isFileInPlaylistMetadata(const std::string& path, uint32_t playlistIndex);
static bool                     isFileInPlaylistW(const std::wstring& path, uint32_t playlistIndex);
static bool                     isValidSoundFile(const std::string& path); 

static void                     loadPlaylists();
static void                     loadPlaylistFileAsync(std::vector<SoundFile>* files, std::string path);
static void                     addFileToPlaylistAsync(std::vector<SoundFile>* files, std::string path, uint32_t playlistIndex);

static std::string              getPlaylistName(const std::filesystem::directory_entry& folder);
static std::string              getPlaylistDesc(const std::filesystem::directory_entry& folder);
static std::string              getPlaylistUrl(const std::filesystem::directory_entry& folder);
static std::vector<std::string> getPlaylistFilepaths(const std::filesystem::directory_entry& folder);
static std::vector<std::wstring> getPlaylistDisplayNamesW(const std::filesystem::directory_entry& folder);

static std::vector<std::wstring> loadFilesFromFolder(const std::filesystem::path& folderPath);
static void                     playlistPlayFileWithIndex(uint32_t i, uint32_t playlistIndex);

static void                     skipSoundUp(uint32_t playlistIndex);
static void                     skipSoundDown(uint32_t playlistIndex);

static std::string              formatDurationToMins(int32_t duration);
static void                     updateSoundProgress();

static std::string              removeFileExtension(const std::string& filename);
static std::wstring             removeFileExtensionW(const std::wstring& filename);

static void                     loadIcons();

static std::string              wStrToStr(const std::wstring& wstr);
static std::wstring             strToWstr(const std::string& str);

static bool                     playlistFileOrderCorrect(uint32_t playlistIndex, const std::vector<std::string>& paths); 
static std::vector<SoundFile>   reorderPlaylistFiles(const std::vector<SoundFile>& soundFiles, const std::vector<std::string>& paths); 

static void                     handleAsyncPlaylistLoading();
static void                     loadPlaylistAsync(Playlist& playlist);


static double                   getSoundDuration(const std::string& soundPath); 

std::vector<std::filesystem::directory_entry> loadFolderContents(const std::wstring& folderpath) {
    std::vector<std::filesystem::directory_entry> contents;
    for (const auto& entry : std::filesystem::directory_iterator(folderpath)) {
        contents.emplace_back(entry);
    }
    return contents;
}

static void winResizeCb(GLFWwindow* window, int32_t width, int32_t height) {
    lf_resize_display(width, height);
    glViewport(0, 0, width, height);
    state.win->setWidth(width);
    state.win->setHeight(height);

    state.trackProgressSlider._init = false;
}
void initWin(float width, float height) {
    if(!glfwInit()) {
        LOG_ERROR("Failed to initialize GLFW.\n");
    }

    state.win = new Window("Lyssa Music Player", (uint32_t)width, (uint32_t)height);

    lf_init_glfw(width, height, state.win->getRawWindow());   
    lf_set_text_wrap(true);
    lf_set_theme(ui_theme());

    glfwSetFramebufferSizeCallback(state.win->getRawWindow(), winResizeCb);

    glViewport(0, 0, width, height);

    state.currentSoundPos = 0.0;
    ma_result result = ma_engine_init(NULL, &state.soundEngine);
    if (result != MA_SUCCESS) {
        std::cerr << "[Error]: Failed to initialize miniaudio.\n";
    } 
}

void initUI() {
    state.h1Font = lf_load_font("../assets/fonts/inter-bold.ttf", 48);
    state.h2Font = lf_load_font("../assets/fonts/inter-bold.ttf", 40);
    state.h3Font = lf_load_font("../assets/fonts/inter-bold.ttf", 36);
    state.h4Font = lf_load_font("../assets/fonts/inter.ttf", 30);
    state.h5Font = lf_load_font("../assets/fonts/inter.ttf", 24);
    state.h6Font = lf_load_font("../assets/fonts/inter.ttf", 20);
    state.h7Font = lf_load_font("../assets/fonts/inter.ttf", 18);
    state.musicTitleFont = lf_load_font_ex("../assets/fonts/inter-bold.ttf", 72, 3072, 3072, 1536); 

    loadIcons();

    state.createPlaylistTab.nameInput.input = (LfInputField){
        .width = 600, 
            .buf = state.createPlaylistTab.nameInput.buffer, 
            .buf_size = INPUT_BUFFER_SIZE,  
            .placeholder = (char*)"Name",
    };

    state.createPlaylistTab.descInput.input = (LfInputField){
        .width = 600, 
            .buf = state.createPlaylistTab.descInput.buffer, 
            .buf_size = INPUT_BUFFER_SIZE,  
            .placeholder = (char*)"Description",
    };

    state.playlistAddFromFileTab.pathInput.input = (LfInputField){
        .width = 600, 
            .buf = state.playlistAddFromFileTab.pathInput.buffer, 
            .buf_size = INPUT_BUFFER_SIZE,  
            .placeholder = (char*)"Path",
    };

    state.trackProgressSlider = (LfSlider){
        .val = reinterpret_cast<int32_t*>(&state.currentSoundPos), 
            .min = 0, 
            .max = 0,
            .width = 400,
            .height = 5,
    };

    state.volumeSlider = (LfSlider){
        .val = reinterpret_cast<int32_t*>(&state.soundHandler.volume), 
            .min = 0, 
            .max = VOLUME_MAX,
            .width = 100,
            .height = 5,
    };
}

void handleTabKeyStrokes() {
    if(state.soundHandler.isInit) {
        if(lf_key_event().pressed && lf_key_event().happened) {
            switch(lf_key_event().keycode) {
                case GLFW_KEY_SPACE: 
                    {
                        if(state.soundHandler.isPlaying)
                            state.soundHandler.stop();
                        else 
                            state.soundHandler.play();
                        break;
                    }
                case GLFW_KEY_N: 
                    {
                        if(lf_key_is_down(GLFW_KEY_LEFT_SHIFT)) {
                            skipSoundDown(state.currentPlaylist);
                        } else {
                            skipSoundUp(state.currentPlaylist);
                        }
                        break;
                    }
                case GLFW_KEY_LEFT: 
                    {
                        if(state.soundHandler.getPositionInSeconds() - 5 >= 0) {
                            state.soundHandler.setPositionInSeconds(state.soundHandler.getPositionInSeconds() - 5);
                            state.currentSoundPos = state.soundHandler.getPositionInSeconds();
                            state.trackProgressSlider._init = false;
                        }
                        break;
                    }
                case GLFW_KEY_RIGHT: 
                    {
                        if(state.soundHandler.getPositionInSeconds() + 5 <= state.soundHandler.lengthInSeconds) {
                            state.soundHandler.setPositionInSeconds(state.soundHandler.getPositionInSeconds() + 5);
                            state.currentSoundPos = state.soundHandler.getPositionInSeconds();
                            state.trackProgressSlider._init = false;
                        }
                        break;
                    }
                case GLFW_KEY_DOWN: 
                    {
                        state.volumeSlider._init = false;
                        state.showVolumeSliderTrackDisplay = true;
                        state.showVolumeSliderOverride = true;
                        if(*(int32_t*)state.volumeSlider.val - VOLUME_TOGGLE_STEP >= 0)
                            *(int32_t*)state.volumeSlider.val -= VOLUME_TOGGLE_STEP; 
                        else 
                            *(int32_t*)state.volumeSlider.val = 0;
                        break;
                    }
                case GLFW_KEY_UP: 
                    {
                        state.volumeSlider._init = false;
                        state.showVolumeSliderTrackDisplay = true;
                        state.showVolumeSliderOverride = true;
                        if(*(int32_t*)state.volumeSlider.val + VOLUME_TOGGLE_STEP <= VOLUME_MAX)
                            *(int32_t*)state.volumeSlider.val += VOLUME_TOGGLE_STEP; 
                        else 
                            *(int32_t*)state.volumeSlider.val = VOLUME_MAX;
                        break;
                    }
                case GLFW_KEY_M: 
                    {
                        if(state.soundHandler.volume != 0.0f) 
                            state.volumeBeforeMute = state.soundHandler.volume;

                        state.soundHandler.volume = (state.soundHandler.volume != 0.0f) ? 0.0f : state.volumeBeforeMute;
                        state.volumeSlider._init = false;
                        state.showVolumeSliderTrackDisplay = true;
                        state.showVolumeSliderOverride = true;
                        break;
                    }
            }
        }
    }
}


static bool area_hovered(vec2s pos, vec2s size) {
    bool hovered = lf_get_mouse_x() <= (pos.x + size.x) && lf_get_mouse_x() >= (pos.x) && 
        lf_get_mouse_y() <= (pos.y + size.y) && lf_get_mouse_y() >= (pos.y);
    return hovered;
}


void renderDashboard() {

    lf_push_font(&state.h1Font);
    lf_text("Your Playlists");
    lf_pop_font();

    if(!state.playlists.empty()) {
        {
            const float width = 170;
            const float height = -1;
            LfUIElementProps props = primary_button_style();
            props.margin_right = 0;
            props.margin_left = 0;
            lf_push_style_props(props);
            lf_push_font(&state.h1Font);
            lf_set_ptr_x_absolute(state.win->getWidth() - ((width + props.padding * 2.0f) * 2.0f) - DIV_START_X * 2);
            lf_pop_font();

            if(lf_button_fixed("Download Playlist", width, height) == LF_CLICKED) { 
                changeTabTo(GuiTab::DownloadPlaylist);
            }
            props.margin_left = 10;
            lf_push_style_props(props);
            if(lf_button_fixed("Add Playlist", width, height) == LF_CLICKED) {
                state.aOrBPopup.render = !state.aOrBPopup.render;
                state.aOrBPopup.title = "How do you want to add a Playlist?";
                state.aOrBPopup.aStr = "Create New";
                state.aOrBPopup.bStr = "From Folder";
                state.aOrBPopup.width = 400;
                state.aOrBPopup.aCb = [&](){
                    changeTabTo(GuiTab::CreatePlaylist);
                    state.aOrBPopup.render = false;
                };
                state.aOrBPopup.bCb = [&](){
                    if(state.playlistAddFromFolderTab.currentFolderPath.empty()) {
                        state.playlistAddFromFolderTab.currentFolderPath = strToWstr(std::string(getenv(HOMEDIR)));
                        state.playlistAddFromFolderTab.folderContents = loadFolderContents(state.playlistAddFromFolderTab.currentFolderPath);
                    }
                    changeTabTo(GuiTab::CreatePlaylistFromFolder);
                    state.aOrBPopup.render = false;
                };
            }
            lf_pop_style_props();
        }
    }

    lf_next_line();
    if(state.playlists.empty()) {
        {
            const char* text = "You don't have any playlists.";
            float textWidth = lf_text_dimension(text).x;
            lf_set_ptr_x((state.win->getWidth() - textWidth) / 2.0f - DIV_START_X);
            LfUIElementProps props = lf_get_theme().text_props;
            props.margin_top = 40;
            props.margin_left = 0;
            props.margin_right = 0;
            lf_push_style_props(props);
            lf_text(text);
            lf_pop_style_props();
        }
        lf_next_line();
        {
            const float width = 200;
            lf_set_ptr_x((state.win->getWidth() - ((width + (lf_get_theme().button_props.padding * 2)) + 5) * 2) / 2.0f - DIV_START_X);
            LfUIElementProps props = primary_button_style();
            props.margin_right = 5;
            props.margin_left = 5;
            props.margin_top = 15;
            props.corner_radius = 12;
            lf_push_style_props(props);
            if(lf_button_fixed("Create Playlist", width, 50) == LF_CLICKED)  {
                changeTabTo(GuiTab::CreatePlaylist);
            }
            if(lf_button_fixed("Download Playlist", width, 50) == LF_CLICKED)  {
                changeTabTo(GuiTab::DownloadPlaylist);
            }
            lf_pop_style_props();
        }
    } else {
        // Constants
        const float width = 180;
        float height = 260;
        const float paddingTop = 50;

        int32_t playlistIndex = 0;
        for(auto& playlist : state.playlists) {
            bool overDiv = area_hovered((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + paddingTop}, (vec2s){width, height + 20}); 
            // Div
            LfUIElementProps props = lf_get_theme().div_props;
            const LfColor divColor = GRAY;
            props.color = overDiv ? lf_color_brightness(divColor, 0.6) : lf_color_brightness(divColor, 0.4);
            props.corner_radius = 5.0f;
            props.padding = 0;
            lf_push_style_props(props);

            lf_push_style_props(props);
            lf_set_div_hoverable(true);

            lf_push_element_id(playlistIndex);
            lf_div_begin(((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + paddingTop}), ((vec2s){width, overDiv ? height + 20 : height}), false);
            lf_pop_element_id();
            lf_set_div_hoverable(false);
            lf_pop_style_props();

            // Playlist Thumbnail
            {
                LfUIElementProps props = lf_get_theme().image_props;
                props.margin_left = 12.5f;
                props.margin_top = 12.5f;
                props.corner_radius = 5.0f;
                lf_push_style_props(props);
                lf_image((LfTexture){.id = state.icons["music_note"].id, 
                        .width = (uint32_t)(width - props.margin_left * 2), 
                        .height = (uint32_t)(width - props.margin_top * 2)});
                lf_pop_style_props();
            }
            // Playlist Name
            lf_next_line();
            {
                LfUIElementProps props = lf_get_theme().text_props;
                props.margin_left = 12.5f; 
                props.padding = 0;
                props.border_width = 0;
                lf_push_style_props(props);
                lf_text(playlist.name.c_str());
                lf_pop_style_props();
            }

            // Playlist Description
            lf_next_line();
            {
                LfUIElementProps props = lf_get_theme().text_props;
                props.margin_left = 12.5f; 
                props.padding = 0;
                props.border_width = 0;
                props.text_color = (LfColor){150, 150, 150, 255};
                lf_push_style_props(props);
                lf_push_font(&state.h6Font);
                lf_text(playlist.desc.c_str());
                lf_pop_font();
                lf_pop_style_props();
            }
            // Buttons
            bool overButton = false;
            vec2s buttonSize = (vec2s){20, 20};
            if(overDiv)
            {
                LfUIElementProps props = lf_get_theme().button_props;
                props.color = LF_NO_COLOR;
                props.border_color = LF_NO_COLOR;
                props.border_width = 0;
                props.padding = 0;
                lf_push_style_props(props);

                float margin = 5;
                float ptr_y = lf_get_ptr_y();

                lf_set_ptr_y(height + 20 - margin * 6);
                lf_set_ptr_x(width - buttonSize.x - props.margin_right - props.margin_left - buttonSize.x - props.margin_right - margin);


                lf_set_image_color(LYSSA_RED);
                LfClickableItemState deleteButton = lf_image_button(((LfTexture){.id = state.icons["delete"].id, 
                            .width = (uint32_t)buttonSize.x, .height = (uint32_t)buttonSize.y}));
                lf_unset_image_color();

                if(deleteButton == LF_CLICKED) {
                    if(state.currentSoundFile != nullptr) {
                        state.soundHandler.stop();
                        state.soundHandler.uninit();
                        state.currentSoundFile = nullptr;
                        state.currentPlaylist = -1;
                    }
                    deletePlaylist(playlistIndex);
                }

                LfClickableItemState renameButton = lf_image_button(((LfTexture){.id = state.icons["edit"].id, 
                            .width = (uint32_t)buttonSize.x, .height = (uint32_t)buttonSize.y})); 

                if(renameButton == LF_CLICKED) {
                    memset(state.createPlaylistTab.nameInput.input.buf, 0, INPUT_BUFFER_SIZE);
                    strcpy(state.createPlaylistTab.nameInput.input.buf, playlist.name.c_str());

                    memset(state.createPlaylistTab.nameInput.buffer, 0, INPUT_BUFFER_SIZE);
                    strcpy(state.createPlaylistTab.nameInput.buffer, playlist.name.c_str());

                    memset(state.createPlaylistTab.descInput.input.buf, 0, INPUT_BUFFER_SIZE);
                    strcpy(state.createPlaylistTab.descInput.input.buf, playlist.desc.c_str());

                    memset(state.createPlaylistTab.descInput.buffer, 0, INPUT_BUFFER_SIZE);
                    strcpy(state.createPlaylistTab.descInput.buffer, playlist.desc.c_str());

                    state.createPlaylistTab.nameInput.input.cursor_index = strlen(playlist.name.c_str());

                    state.createPlaylistTab.descInput.input.cursor_index = strlen(playlist.desc.c_str());

                    state.currentPlaylist = playlistIndex;
                    state.popups[(int32_t)PopupID::EditPlaylistPopup].render = true;
                }
                overButton = (renameButton != LF_IDLE || deleteButton != LF_IDLE);

                lf_pop_style_props();
                lf_set_ptr_y(ptr_y - lf_get_current_div().aabb.size.y);
            }
            if(lf_get_current_div().interact_state == LF_CLICKED && lf_get_current_div().id == lf_get_selected_div().id 
                    && !overButton) {
                state.currentPlaylist = playlistIndex;
                if(!playlist.loaded) {
                    state.loadedPlaylistFilepaths.clear();
                    state.loadedPlaylistFilepaths.shrink_to_fit();

                    state.loadedPlaylistFilepaths = getPlaylistFilepaths(std::filesystem::directory_entry(playlist.path));
                    loadPlaylistAsync(playlist);
                    playlist.loaded = true;
                }
                changeTabTo(GuiTab::OnPlaylist);
            } 

            lf_next_line();
            lf_div_end();

            lf_set_ptr_x(lf_get_ptr_x() + width + props.margin_left);
            if(lf_get_ptr_x() + width + props.margin_left >= state.win->getWidth() - DIV_START_X * 2.0f - props.margin_right) {
                lf_set_ptr_x(0);
                lf_set_ptr_y(lf_get_ptr_y() + height + props.margin_bottom);
            }
            playlistIndex++;
        }
    }

    lf_set_ptr_y(state.win->getHeight() - BACK_BUTTON_HEIGHT - 45 - DIV_START_Y * 2);
    lf_set_ptr_x(DIV_START_X);
    renderTrackMenu();

    lf_div_end();
}
void renderCreatePlaylist(std::function<void()> onCreateCb, std::function<void()> clientUICb, std::function<void()> backButtonCb) {
    // Heading
    {
        LfUIElementProps props = lf_get_theme().text_props;
        props.text_color = LF_WHITE;
        props.margin_bottom = 15;
        lf_push_style_props(props);
        lf_push_font(&state.h1Font);
        lf_text("Create Playlist");
        lf_pop_style_props();
        lf_pop_font();
    }
    // Form Input
    {
        lf_next_line();
        state.createPlaylistTab.nameInput.input.width = state.win->getWidth() / 2.0f;
        LfUIElementProps props = input_field_style();
        lf_push_style_props(props);
        lf_input_text(&state.createPlaylistTab.nameInput.input);
        lf_pop_style_props();
        lf_next_line();
    }
    {
        lf_next_line();
        state.createPlaylistTab.descInput.input.width = state.win->getWidth() / 2.0f;
        LfUIElementProps props = input_field_style();
        lf_push_style_props(props);
        lf_input_text(&state.createPlaylistTab.descInput.input);
        lf_pop_style_props();
        lf_next_line();
    }

    // Create Button
    {
        lf_next_line();
        LfUIElementProps props = call_to_action_button_style();
        props.margin_top = 10;
        lf_push_style_props(props);
        if(lf_button_fixed("Create", 150, -1) == LF_CLICKED) {
            state.createPlaylistTab.createFileStatus = createPlaylist(std::string(state.createPlaylistTab.nameInput.buffer), std::string(state.createPlaylistTab.descInput.buffer)); 
            memset(state.createPlaylistTab.nameInput.input.buf, 0, INPUT_BUFFER_SIZE);
            memset(state.createPlaylistTab.nameInput.buffer, 0, INPUT_BUFFER_SIZE);

            memset(state.createPlaylistTab.descInput.input.buf, 0, INPUT_BUFFER_SIZE);
            memset(state.createPlaylistTab.descInput.buffer, 0, INPUT_BUFFER_SIZE);

            state.createPlaylistTab.createFileMessageTimer = 0.0f;
            if(onCreateCb) 
                onCreateCb();
        }
        lf_pop_style_props();
    }

    if(clientUICb)
        clientUICb();

    // File Status Message
    if(state.createPlaylistTab.createFileStatus != FileStatus::None) {
        if(state.createPlaylistTab.createFileMessageTimer < state.createPlaylistTab.createFileMessageShowTime) {
            state.createPlaylistTab.createFileMessageTimer += state.deltaTime;
            lf_next_line();
            lf_push_font(&state.h4Font);
            LfUIElementProps props = lf_get_theme().button_props;
            switch(state.createPlaylistTab.createFileStatus) {
                case FileStatus::Failed:
                    props.text_color = LYSSA_RED;
                    lf_push_style_props(props);
                    lf_text("Failed to create playlist.");
                    break;
                case FileStatus::AlreadyExists:
                    props.text_color = LYSSA_RED;
                    lf_push_style_props(props);
                    lf_text("Playlist already exists.");
                    break;
                case FileStatus::Success:
                    props.text_color = LYSSA_GREEN;
                    lf_push_style_props(props);
                    lf_text("Created playlist.");
                    break;
                default:
                    break;
            }
            lf_pop_font();
        }
    }

    backButtonTo(GuiTab::Dashboard, [&](){
            state.createPlaylistTab.createFileMessageTimer = state.createPlaylistTab.createFileMessageShowTime;
            loadPlaylists();
            if(backButtonCb)
                backButtonCb();
            });
    renderTrackMenu();
}

void renderCreatePlaylistFromFolder() {
    static bool selectedFolder = false;

    if(!selectedFolder) {
        // Heading
        {
            LfUIElementProps props = lf_get_theme().text_props;
            props.text_color = LF_WHITE;
            props.margin_bottom = 15;
            lf_push_style_props(props);
            lf_push_font(&state.h1Font);
            lf_text("Create Playlist from Folder");
            lf_pop_style_props();
            lf_pop_font();
            lf_next_line();
            lf_text("Select a Folder from which to add the files.");
        }
        lf_next_line();
        renderFileDialogue(
                [&](std::filesystem::directory_entry entry){
                PlaylistAddFromFolderTab& tab = state.playlistAddFromFolderTab;
                tab.currentFolderPath = entry.path().wstring();
                tab.folderContents.clear();
                tab.folderContents = loadFolderContents(tab.currentFolderPath);
                lf_set_current_div_scroll(0.0f);
                lf_set_current_div_scroll_velocity(0.0f);
                },
                [&](){
                PlaylistAddFromFolderTab& tab = state.playlistAddFromFolderTab;
                tab.currentFolderPath = std::filesystem::path(tab.currentFolderPath).parent_path().wstring();
                tab.folderContents.clear();
                tab.folderContents = loadFolderContents(tab.currentFolderPath);
                },
                nullptr,
                [&](std::filesystem::directory_entry entry, bool hovered){
                LfUIElementProps props = lf_get_theme().button_props;
                props.margin_top = 0.0f;
                props.color = LF_NO_COLOR;
                props.padding = 2.5f;
                props.border_width = 0.0f;
                lf_set_image_color(LF_WHITE);
                lf_push_style_props(props);
                const vec2s iconSize = (vec2s){25, 25};
                LfTexture icon = (LfTexture){
                    .id = (entry.is_directory()) ? state.icons["folder"].id : state.icons["file"].id, 
                        .width = (uint32_t)iconSize.x, 
                        .height = (uint32_t)iconSize.y
                };
                lf_image_button(icon);
                lf_pop_style_props();
                lf_unset_image_color();
                }, 
                [&](std::filesystem::directory_entry entry, bool hovered){
                    if(hovered) {
                        LfUIElementProps props = primary_button_style();
                        props.margin_top = 1.5;
                        props.padding = 5;
                        lf_push_style_props(props);
                        LfClickableItemState button = lf_button_fixed("Select", 100, -1);
                        if(button == LF_CLICKED) {
                            selectedFolder = true;
                            state.playlistAddFromFolderTab.currentFolderPath = entry.path().wstring();
                            strcpy(state.createPlaylistTab.nameInput.buffer, entry.path().filename().c_str());
                            strcpy(state.createPlaylistTab.nameInput.input.buf, entry.path().filename().c_str());
                        }
                        lf_pop_style_props();
                        return button != LF_IDLE;
                    }
                    return false;
                }, 
                state.playlistAddFromFolderTab.folderContents, true);
        backButtonTo(GuiTab::Dashboard, [&](){
                state.createPlaylistTab.createFileMessageTimer = state.createPlaylistTab.createFileMessageShowTime;
                loadPlaylists();
                });
        renderTrackMenu();
    } else {
        renderCreatePlaylist([&](){
                loadPlaylists();
                PlaylistAddFromFolderTab& tab = state.playlistAddFromFolderTab;
                uint32_t playlist = state.playlists.size() - 1;
                std::ofstream metadata(state.playlists[playlist].path + "/.metadata", std::ios::app);
                std::cout << state.playlists[playlist].path + "/.metadata" << "\n"; 
                metadata.seekp(0, std::ios::end);
                for(const auto& entry : std::filesystem::directory_iterator(tab.currentFolderPath)) {
                if(!entry.is_directory() && isValidSoundFile(entry.path().string())) {
                        metadata << "\"" << entry.path().string() << "\" ";
                    }
                }
                metadata.close();
        }, 
        [&](){
            LfUIElementProps props = call_to_action_button_style();
            props.margin_top = 10;
            props.color = LYSSA_RED;
            props.text_color = LF_WHITE;
            lf_push_style_props(props);
            if(lf_button_fixed("Change Folder", 150, -1) == LF_CLICKED) {
                selectedFolder = false;
            }
            lf_pop_style_props();
        }, 
        [&](){
            selectedFolder = false;
        });
    }

}

void renderDownloadPlaylist() {
    std::string downloadedPlaylistDir = LYSSA_DIR + "/downloaded_playlists/" + state.downloadingPlaylistName; 
    uint32_t downloadedFileCount = LyssaUtils::getLineCountFile(downloadedPlaylistDir + "/archive.txt");

    static std::string url;

    if(state.playlistDownloadFinished) {
        // Heading
        {
            LfUIElementProps props = lf_get_theme().text_props;
            props.text_color = LF_WHITE;
            lf_push_style_props(props);
            lf_push_font(&state.h1Font);

            std::string text = "Finished Downloading Playlist";
            lf_set_ptr_x_absolute((state.win->getWidth() - lf_text_dimension(text.c_str()).x) / 2.0f);
            lf_text(text.c_str());
            lf_pop_style_props();
            lf_pop_font();
        }

        {
            lf_next_line();
            lf_push_font(&state.h6Font);
            LfUIElementProps props = lf_get_theme().text_props;
            props.margin_top = 15;
            props.color = GRAY;
            lf_push_style_props(props);
            std::string text = "Downloading of playlist \"" + state.downloadingPlaylistName + "\" with " + std::to_string(state.downloadPlaylistFileCount) + " files finished.";
            lf_set_ptr_x_absolute((state.win->getWidth() - lf_text_dimension(text.c_str()).x) / 2.0f);
            lf_text(text.c_str());
            lf_pop_style_props();
            lf_pop_font();
        }

        {
            lf_next_line();
            LfUIElementProps props = call_to_action_button_style();
            props.margin_left = 0;
            props.margin_top = 15;
            props.margin_right = 0; 

            lf_push_style_props(props);
            lf_set_ptr_x_absolute((state.win->getWidth() - (180 + props.padding * 2.0f)) / 2.0f);
            if(lf_button_fixed("Open Playlist", 180, -1) == LF_CLICKED) {
                loadPlaylists();
                state.playlistDownloadRunning = false;
                state.playlistDownloadFinished = false;
                uint32_t playlistIndex = 0;
                for(uint32_t i = 0; i < state.playlists.size(); i++) {
                    if(state.playlists[i].name == state.downloadingPlaylistName) {
                        playlistIndex = i;
                        break;
                    }
                }
                state.currentPlaylist = playlistIndex;
                auto& playlist = state.playlists[state.currentPlaylist];
                if(!playlist.loaded) {
                    state.loadedPlaylistFilepaths.clear();
                    state.loadedPlaylistFilepaths.shrink_to_fit();

                    state.loadedPlaylistFilepaths = getPlaylistFilepaths(std::filesystem::directory_entry(playlist.path));
                    loadPlaylistAsync(playlist);
                    playlist.loaded = true;
                }
                changeTabTo(GuiTab::OnPlaylist);
            }
        }
        backButtonTo(GuiTab::Dashboard, [&](){
                state.playlistDownloadRunning = false;
                state.playlistDownloadFinished = false;
                loadPlaylists();
                });
        renderTrackMenu();
        return;
    }
    if(!state.playlistDownloadRunning) {
        // Heading
        {
            LfUIElementProps props = lf_get_theme().text_props;
            props.text_color = LF_WHITE;
            props.margin_bottom = 15;
            lf_push_style_props(props);
            lf_push_font(&state.h1Font);
            lf_text("Download Playlist");
            lf_pop_style_props();
            lf_pop_font();
        }

        {
            lf_next_line();
            lf_push_font(&state.h6Font);
            LfUIElementProps props = lf_get_theme().text_props;
            props.margin_top = 0;
            props.color = GRAY;
            lf_push_style_props(props);
            lf_text("Download a playlist from YouTube");
            lf_pop_style_props();
            lf_pop_font();
        }

        lf_next_line();
        static char urlInput[INPUT_BUFFER_SIZE] ={0};

        {
            LfUIElementProps props = input_field_style();
            props.margin_top = 15;
            lf_push_style_props(props);
            lf_input_text_inl_ex(urlInput, INPUT_BUFFER_SIZE, 600, "URL");
            lf_pop_style_props();
        }

        lf_next_line();

        {
            lf_push_style_props(call_to_action_button_style());
            if(lf_button_fixed("Download", 150, -1) == LF_CLICKED) {
                std::string cmd = LYSSA_DIR + "/scripts/download-yt.sh \"" + urlInput + "\" " + LYSSA_DIR + "/downloaded_playlists/"; 
                std::string execCmd = cmd + " &";
                system(execCmd.c_str());
                state.playlistDownloadRunning = true;
                state.downloadingPlaylistName = LyssaUtils::getCommandOutput(std::string("yt-dlp \"" + std::string(urlInput) + "\" --flat-playlist --dump-single-json --no-warnings | jq -r .title &"));
                state.downloadPlaylistFileCount = LyssaUtils::getPlaylistFileCountURL(urlInput);
                url = urlInput;
                memset(urlInput, 0, INPUT_BUFFER_SIZE);
            }
            lf_pop_style_props();
        }
    } else  {
        state.playlistDownloadFinished = (downloadedFileCount == state.downloadPlaylistFileCount) || LyssaUtils::getCommandOutput("pgrep yt-dlp") == ""; 
        if(state.playlistDownloadFinished) {
            FileStatus createStatus = createPlaylist(state.downloadingPlaylistName, "Playlist from YouTube", url);

            if(createStatus != FileStatus::AlreadyExists) {
                std::string playlistDir = LYSSA_DIR + "/playlists/" + state.downloadingPlaylistName; 
                std::ofstream metadata(playlistDir + "/.metadata", std::ios::app);

                metadata.seekp(0, std::ios::end);

                for (const auto& entry : std::filesystem::directory_iterator(downloadedPlaylistDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".mp3") {
                        metadata << "\"" << entry.path().string() << "\" ";
                    }
                }
                metadata.close();
            }
        }
        {
            std::string title = std::string("Downloading \"" + state.downloadingPlaylistName + "\"...");
            LfUIElementProps props = lf_get_theme().text_props;
            props.text_color = LF_WHITE;
            props.margin_bottom = 10;
            lf_push_style_props(props);
            lf_push_font(&state.h1Font);
            lf_set_ptr_x_absolute((state.win->getWidth() - lf_text_dimension(title.c_str()).x) / 2.0f);
            lf_text(title.c_str());
            lf_pop_style_props();
            lf_pop_font();
            lf_next_line();

            std::string subtitle = std::string("This can take a while.");
            props.margin_bottom = 15;
            lf_push_style_props(props);
            lf_set_ptr_x_absolute((state.win->getWidth() - lf_text_dimension(subtitle.c_str()).x) / 2.0f);
            lf_text(subtitle.c_str());
            lf_pop_style_props();
            lf_next_line();
            
        }

        lf_next_line();

        {
            const vec2s progressBarSize = (vec2s){400, 6};

            LfUIElementProps props = lf_get_theme().slider_props;
            props.border_width = 0;
            props.color = GRAY;
            props.text_color = BLUE_GRAY;  
            props.corner_radius = 1.5f;
            props.margin_top = 15;
            props.margin_left = 0;
            props.margin_right = 0;

            {
                vec2s textDim = lf_text_dimension(std::to_string(downloadedFileCount).c_str());
                lf_set_ptr_x_absolute((state.win->getWidth() - progressBarSize.x - textDim.x) / 2.0f);

                LfUIElementProps props = lf_get_theme().text_props;
                props.color = lf_color_brightness(GRAY, 1.5);
                props.margin_top = 15 - (textDim.y - progressBarSize.y) / 2.0f;

                lf_push_style_props(props);
                lf_push_font(&state.h6Font);
                lf_text(std::to_string(downloadedFileCount).c_str());
                lf_pop_font();
            }

            lf_push_style_props(props);
            lf_progress_bar_int(downloadedFileCount, 0, (float)state.downloadPlaylistFileCount, progressBarSize.x, progressBarSize.y);
            lf_pop_style_props();

            {
                std::string totalFileCount = std::to_string(state.downloadPlaylistFileCount);
                static float totalFileCountHeight = lf_text_dimension(totalFileCount.c_str()).y;

                LfUIElementProps props = lf_get_theme().text_props;
                props.color = lf_color_brightness(GRAY, 1.5);
                props.margin_top = 15 - (totalFileCountHeight - progressBarSize.y) / 2.0f;

                lf_push_style_props(props);
                lf_push_font(&state.h6Font);
                lf_text(totalFileCount.c_str());
                lf_pop_font();
            }
        }

        lf_next_line();

        {
            const float buttonSize = 180.0f;
            LfUIElementProps props = call_to_action_button_style();
            props.color = LYSSA_RED;
            props.margin_left = 0;
            props.margin_right = 0;
            props.corner_radius = 8;
            props.margin_top = 15;
            props.text_color = LF_WHITE;
            lf_push_style_props(props);
            lf_set_ptr_x_absolute((state.win->getWidth() - (buttonSize + props.padding * 2.0f)) / 2.0f);
            if(lf_button_fixed("Cancle Download", buttonSize, -1) == LF_CLICKED) {
                state.playlistDownloadRunning = false;
                system("pkill yt-dlp &");
            }
            lf_pop_style_props();
        }
    }
    backButtonTo(GuiTab::Dashboard, [&](){
            loadPlaylists();
            });
    renderTrackMenu();
}


bool isProcessRunning(const char *processName) {
    FILE *fp;
    char command[1024];
    char line[1024];

    // Build the command to check for the process
    snprintf(command, sizeof(command), "ps aux | grep -v grep | grep '%s'", processName);

    // Open the command for reading
    fp = popen(command, "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
        exit(1);
    }

    // Read the output of the command
    while (fgets(line, sizeof(line), fp) != NULL) {
        // If any output is found, the process is running
        pclose(fp);
        return true;
    }

    // Close the file pointer
    pclose(fp);
    return false;
}


void renderOnPlaylist() {
    if(state.currentPlaylist == -1) return;

    auto& currentPlaylist = state.playlists[state.currentPlaylist];

    static bool showPlaylistSettings = false;
    static bool clearedPlaylist = false;

    if(state.playlistDownloadRunning) {
        if(!clearedPlaylist) {
            currentPlaylist.musicFiles.clear();
            state.loadedPlaylistFilepaths.clear();
            state.playlistFileThumbnailData.clear();
            savePlaylist(state.currentPlaylist);
            clearedPlaylist = true;
        }

        if(state.downloadPlaylistFileCount == LyssaUtils::getLineCountFile(LYSSA_DIR + "/downloaded_playlists/" + state.downloadingPlaylistName + "/archive.txt") && state.downloadPlaylistFileCount != 0) {
            state.playlistDownloadRunning = false;
            for (const auto& entry : std::filesystem::directory_iterator(LYSSA_DIR + "/downloaded_playlists/" + state.downloadingPlaylistName)) {
                if (entry.is_regular_file() && 
                        !isFileInPlaylist(entry.path().string(), state.currentPlaylist) && 
                        isValidSoundFile(entry.path().string()) && entry.path().extension() == ".mp3") {
                    state.playlistFileFutures.emplace_back(std::async(std::launch::async, addFileToPlaylistAsync, 
                                &state.playlists[state.currentPlaylist].musicFiles, entry.path().string(), state.currentPlaylist));
                }
            }
        }
    }

    // Playlist Heading
    {
        lf_push_font(&state.musicTitleFont);
        vec2s titleSize = lf_text_dimension(currentPlaylist.name.c_str());
        bool onTitleArea = lf_hovered(LF_PTR, (vec2s){(float)state.win->getWidth(), titleSize.y});
        // Title 
        {
            LfUIElementProps props = lf_get_theme().text_props;
            props.text_color = LYSSA_PLAYLIST_COLOR;
            lf_push_style_props(props);
            lf_text(currentPlaylist.name.c_str());
            lf_pop_style_props();
        }
        lf_pop_font();
        LfUIElementProps props = lf_get_theme().button_props;
        props.color = LF_NO_COLOR;
        props.border_width = 0;
        props.margin_left = 10;
        props.margin_right = 0;
        props.padding = 0;
        props.margin_top = 0; 
        lf_push_style_props(props);
        lf_set_image_color(LF_WHITE);

        if(onTitleArea && currentPlaylist.url != "") {
            if(lf_image_button(((LfTexture){.id = state.icons["more"].id, .width = 30, .height = 30})) == LF_CLICKED) {
                showPlaylistSettings = !showPlaylistSettings; 
            }
        } else if(!onTitleArea && currentPlaylist.url != "") {
            lf_image_button(((LfTexture){.id = state.icons["more"].id, .width = 20, .height = 20}));
        }
        lf_unset_image_color();
        lf_pop_style_props();

        // "Add More" button
        if(!currentPlaylist.musicFiles.empty())
        {
            lf_push_font(&state.h5Font);
            const char* text = "Add more music";

            float textWidth = lf_text_dimension(text).x;

            LfUIElementProps props = primary_button_style();  
            props.margin_left = 0;
            props.margin_right = 0;

            // Rendering the button in the top right
            lf_set_ptr_x_absolute(state.win->getWidth() - (textWidth + props.padding * 2.0f) - DIV_START_X);

            lf_push_style_props(props);
            float ptr_x = lf_get_ptr_x();
            float ptr_y = lf_get_ptr_y();
            if(lf_button(text) == LF_CLICKED) {
                state.aOrBPopup.render = !state.aOrBPopup.render;
                state.aOrBPopup.title = "How do you want to add Music?";
                state.aOrBPopup.aStr = "From File";
                state.aOrBPopup.bStr = "From Folder";
                state.aOrBPopup.width = 400;
                state.aOrBPopup.aCb = [](){
                    changeTabTo(GuiTab::PlaylistAddFromFile);
                    state.aOrBPopup.render = false;
                };
                state.aOrBPopup.bCb = [](){
                    if(state.playlistAddFromFolderTab.currentFolderPath.empty()) {
                        state.playlistAddFromFolderTab.currentFolderPath = strToWstr(std::string(getenv(HOMEDIR)));
                        state.playlistAddFromFolderTab.folderContents =  loadFolderContents(state.playlistAddFromFolderTab.currentFolderPath);
                    }
                    changeTabTo(GuiTab::PlaylistAddFromFolder);
                    state.aOrBPopup.render = false; 
                };
            }  
            lf_pop_style_props();
        }

        if(showPlaylistSettings) {
            lf_next_line();
            LfUIElementProps props = secondary_button_style();
            props.margin_top = 15;
            lf_push_style_props(props);
            if(lf_button("Sync Downloads") == LF_CLICKED) {
                if(state.soundHandler.isInit) {
                    state.soundHandler.stop();
                    state.soundHandler.uninit();
                    state.currentSoundFile = nullptr;
                }
                system(std::string(LYSSA_DIR + "/scripts/download-yt.sh \"" + currentPlaylist.url + "\" " + LYSSA_DIR + "/downloaded_playlists/ &").c_str());

                state.playlistDownloadRunning = true;
                state.downloadingPlaylistName = std::filesystem::path(currentPlaylist.path).filename().string();
                std::cout << "PRINT: " << state.downloadingPlaylistName << "\n";

                state.downloadPlaylistFileCount = LyssaUtils::getPlaylistFileCountURL(currentPlaylist.url);

                clearedPlaylist = false;
            }
            if(!currentPlaylist.ordered && state.playlistFileFutures.empty() && !currentPlaylist.musicFiles.empty() && !state.loadedPlaylistFilepaths.empty()){
                const char* text = "Order playlist";

                float textWidth = lf_text_dimension(text).x;

                lf_push_style_props(props);
                float ptr_x = lf_get_ptr_x();
                float ptr_y = lf_get_ptr_y();
                if(lf_button(text) == LF_CLICKED) {
                    if(state.soundHandler.isInit) {
                        state.soundHandler.stop();
                        state.soundHandler.uninit();
                        state.currentSoundFile = nullptr;
                    }
                    currentPlaylist.musicFiles = reorderPlaylistFiles(currentPlaylist.musicFiles, state.loadedPlaylistFilepaths);
                    currentPlaylist.ordered = true;
                    state.loadedPlaylistFilepaths.clear();
                    state.loadedPlaylistFilepaths.shrink_to_fit();
                }  
                lf_pop_style_props();
            }
        }

    }

    if(state.playlistDownloadRunning) {
        lf_set_ptr_y(100);
        lf_push_font(&state.h5Font);
        const char* text = "Syncing playlist downloads...";
        float textWidth = lf_text_dimension(text).x;

        // Centering the text
        lf_set_ptr_x((state.win->getWidth() - textWidth) / 2.0f - DIV_START_X);

        LfUIElementProps props = lf_get_theme().text_props;
        props.margin_top = 80;
        props.margin_left = 0;
        props.margin_right = 0;
        lf_push_style_props(props);
        lf_text(text);
        lf_pop_style_props();
        lf_pop_font();
    } else if(currentPlaylist.musicFiles.empty()) {
        // Text
        {
            lf_set_ptr_y(100);
            lf_push_font(&state.h5Font);
            const char* text = "There is no music in this playlist.";
            float textWidth = lf_text_dimension(text).x;

            // Centering the text
            lf_set_ptr_x((state.win->getWidth() - textWidth) / 2.0f - DIV_START_X);

            LfUIElementProps props = lf_get_theme().text_props;
            props.margin_top = 80;
            props.margin_left = 0;
            props.margin_right = 0;
            lf_push_style_props(props);
            lf_text(text);
            lf_pop_style_props();
            lf_pop_font();
        }
        lf_next_line();
        // Buttons 
        {
            lf_push_font(&state.h6Font);
            const float buttonWidth = 175;
            LfUIElementProps props = lf_get_theme().button_props;
            props.color = (LfColor){240, 240, 240, 255};
            props.text_color = LF_BLACK;
            props.corner_radius = 10; 
            props.border_width = 0;
            props.margin_top = 20;

            // Centering the buttons
            lf_set_ptr_x((state.win->getWidth() - 
                        (buttonWidth + props.padding * 2.0f) * 2.0f - (props.margin_right + props.margin_left) * 2.f) / 2.0f - DIV_START_X);

            lf_push_style_props(props);
            if(lf_button_fixed("Add from file", buttonWidth, 40) == LF_CLICKED) {
                changeTabTo(GuiTab::PlaylistAddFromFile);
            }
            if(lf_button_fixed("Add from Folder", buttonWidth, 40) == LF_CLICKED) {
                if(state.playlistAddFromFolderTab.currentFolderPath.empty()) {
                    state.playlistAddFromFolderTab.currentFolderPath = strToWstr(std::string(getenv(HOMEDIR)));
                    state.playlistAddFromFolderTab.folderContents =  loadFolderContents(state.playlistAddFromFolderTab.currentFolderPath);
                }
                changeTabTo(GuiTab::PlaylistAddFromFolder);
            }
            lf_pop_style_props();
            lf_pop_font();
        }
    } else {
        lf_next_line();
        {
            lf_push_font(&state.h3Font);
            LfUIElementProps props = lf_get_theme().text_props;
            props.margin_bottom = 20;
            props.margin_top = 80;
            lf_push_style_props(props);
            lf_text("Files");
            lf_pop_style_props();
            lf_pop_font();
        }
        lf_next_line();


        /* Heading */
        {
            LfUIElementProps props = lf_get_theme().text_props;
            props.margin_bottom = 20;
            lf_push_style_props(props);
            lf_text("#");

            lf_set_ptr_x(lf_get_ptr_x() + state.win->getWidth() / 4.0f - (lf_text_dimension("#").x + lf_get_theme().text_props.margin_right + lf_get_theme().text_props.margin_left));
            lf_text("Track");

            lf_set_ptr_x(state.win->getWidth() - (lf_text_dimension("Duration").x) -  DIV_START_X * 2 - lf_get_theme().text_props.margin_left);
            lf_text("Duration");
            lf_pop_style_props();

            lf_next_line();
        }
        // Seperator
        {
            LfUIElementProps props = lf_get_theme().button_props;
            props.color = lf_color_brightness(GRAY, 0.4);
            lf_push_style_props(props);
            lf_seperator();
            lf_pop_style_props();
        }

        // Begin a new div container for the files
        lf_div_begin_ex(LF_PTR, ((vec2s){(float)state.win->getWidth() - DIV_START_X * 2, 
                    (float)state.win->getHeight() - DIV_START_Y * 2 - lf_get_ptr_y() - 
                    (BACK_BUTTON_HEIGHT + BACK_BUTTON_MARGIN_BOTTOM)}), true, &currentPlaylist.scroll, 
                &currentPlaylist.scrollVelocity);

        lf_next_line();

        bool onPlayButton = false;
        for(uint32_t i = 0; i < currentPlaylist.musicFiles.size(); i++) {
            SoundFile& file = currentPlaylist.musicFiles[i];
            {
                vec2s thumbnailContainerSize = (vec2s){48, 48};
                float marginBottomThumbnail = 10.0f, marginTopThumbnail = 5.0f;


                LfAABB fileAABB = (LfAABB){
                    .pos = LF_PTR,
                        .size = (vec2s){(float)state.win->getWidth() - DIV_START_X * 2, (float)thumbnailContainerSize.y + marginBottomThumbnail - marginTopThumbnail}
                };

                bool hoveredTextDiv = lf_hovered(fileAABB.pos, fileAABB.size);
                if(hoveredTextDiv && lf_mouse_button_is_released(GLFW_MOUSE_BUTTON_RIGHT)) {
                    state.removeFilePopup.path = file.pathStr;
                    state.removeFilePopup.pos = (vec2s){(float)lf_get_mouse_x(), (float)lf_get_mouse_y()};
                    state.popups[(int32_t)PopupID::PlaylistFileDialoguePopup].render = true;
                }
                if(currentPlaylist.playingFile == i) {
                    lf_rect_render(fileAABB.pos, fileAABB.size, lf_color_brightness(GRAY, 0.75),
                            LF_NO_COLOR, 0.0f, 5.0f);
                } 

                // Index 
                {
                    std::stringstream indexSS;
                    indexSS << i;
                    std::string indexStr = indexSS.str();
                    LfUIElementProps props = lf_get_theme().text_props;
                    props.margin_top = (thumbnailContainerSize.y - lf_text_dimension(indexStr.c_str()).y) / 2.0f;
                    lf_push_style_props(props);
                    lf_text(indexStr.c_str());
                    lf_pop_style_props();

                    // Pointer for Title heading
                    lf_set_ptr_x(lf_get_ptr_x() + state.win->getWidth() / 4.0f - (lf_text_dimension(indexStr.c_str()).x +
                                lf_get_theme().text_props.margin_right + lf_get_theme().text_props.margin_left));
                }

                // Play Button 
                if(hoveredTextDiv)
                {
                    LfUIElementProps props = lf_get_theme().button_props;
                    props.margin_right = 4;
                    props.margin_left = -thumbnailContainerSize.x;
                    props.border_width = 0;
                    props.color = lf_color_brightness(LYSSA_PLAYLIST_COLOR, 0.8); 
                    props.corner_radius = 12;
                    lf_push_style_props(props);

                    LfClickableItemState playButton = lf_image_button(((LfTexture){.id = state.icons["play"].id, .width = 24, .height = 24}));
                    onPlayButton = playButton != LF_IDLE;

                    if(playButton == LF_CLICKED) {
                        playlistPlayFileWithIndex(i, state.currentPlaylist);
                        state.currentSoundFile = &file;
                    }
                    lf_pop_style_props();
                }

                // Thumbnail + Title
                {
                    LfTexture thumbnail = (file.thumbnail.width == 0) ? state.icons["music_note"] : file.thumbnail;
                    float aspect = (float)thumbnail.width / (float)thumbnail.height;
                    float thumbnailHeight = thumbnailContainerSize.y / aspect; 

                    lf_rect_render((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + marginTopThumbnail}, thumbnailContainerSize, 
                            PLAYLIST_FILE_THUMBNAIL_COLOR, LF_NO_COLOR, 0.0f, PLAYLIST_FILE_THUMBNAIL_CORNER_RADIUS);

                    lf_image_render((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + 
                            (thumbnailContainerSize.y - thumbnailHeight) / 2.0f + marginTopThumbnail}, LF_WHITE,
                            (LfTexture){.id = thumbnail.id, .width = 
                            (uint32_t)thumbnailContainerSize.x, .height = (uint32_t)thumbnailHeight}, LF_NO_COLOR, 0.0f, 0.0f);  

                    lf_set_ptr_x(lf_get_ptr_x() + thumbnailContainerSize.x);
                    lf_set_line_height(thumbnailContainerSize.y + marginBottomThumbnail);

                    std::filesystem::path fsPath(file.path);
                    std::wstring filename = removeFileExtensionW(fsPath.filename().wstring());

                    lf_set_cull_end_x(state.win->getWidth() - DIV_START_X * 2.0f - 100);

                    lf_text_render_wchar((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + (thumbnailContainerSize.y - lf_text_dimension_wide(filename.c_str()).y) / 2.0f}, filename.c_str(), lf_get_theme().font, -1, false, 
                            hoveredTextDiv ? lf_color_brightness(LF_WHITE, 0.7) : LF_WHITE);

                    lf_unset_cull_end_x();
                } 

                if(lf_mouse_button_is_released(GLFW_MOUSE_BUTTON_LEFT) && hoveredTextDiv && !onPlayButton) {
                    if(i != currentPlaylist.playingFile) {
                        playlistPlayFileWithIndex(i, state.currentPlaylist);
                    }
                    state.currentSoundFile = &file;
                    if(state.onTrackTab.trackThumbnail.width != 0) 
                        lf_free_texture(state.onTrackTab.trackThumbnail);
                    state.onTrackTab.trackThumbnail = getSoundThubmnail(state.currentSoundFile->pathStr);
                    changeTabTo(GuiTab::OnTrack);
                }

                // Duration 
                {
                    lf_set_ptr_x(state.win->getWidth() - (lf_text_dimension("Duration").x) -  DIV_START_X * 2 - lf_get_theme().text_props.margin_left);
                    LfUIElementProps props = lf_get_theme().text_props;
                    std::string durationText = formatDurationToMins(file.duration);
                    props.margin_top = (thumbnailContainerSize.y - lf_text_dimension(durationText.c_str()).y) / 2.0f;
                    lf_push_style_props(props);
                    lf_text(durationText.c_str());
                    lf_pop_style_props();
                }
                lf_next_line(); 
            }
        }
        lf_div_end();
    }


    backButtonTo(GuiTab::Dashboard, [&](){
            showPlaylistSettings = false;
            });
    renderTrackMenu();
}
void renderOnTrack() {
    const float iconSizeSm = 48;
    const float iconSizeXsm = 36;
    OnTrackTab& tab = state.onTrackTab;

    lf_div_begin(((vec2s){DIV_START_X, DIV_START_Y}), ((vec2s){(float)state.win->getWidth(), (float)state.win->getHeight()}), false);

    // Sound Thumbnail
    vec2s thumbnailContainerSize = (vec2s){state.win->getHeight() / 2.0f, state.win->getHeight() / 2.0f};
    if(state.win->getWidth() - 200 < thumbnailContainerSize.x) {
        thumbnailContainerSize = (vec2s){state.win->getWidth() / 2.0f, state.win->getWidth() / 2.0f};
    }

    float ptr_y = lf_get_ptr_y();
    if((state.win->getWidth() - 100 < thumbnailContainerSize.x || state.win->getWidth() > WIN_START_W) && state.win->getHeight() > WIN_START_H) {
        // Center the elements
        lf_set_ptr_y_absolute((state.win->getHeight() - BACK_BUTTON_MARGIN_BOTTOM - BACK_BUTTON_HEIGHT * 2 - 
                    (thumbnailContainerSize.y + lf_get_theme().font.font_size * 6 + state.trackProgressSlider.height + iconSizeSm)) / 2.0f);
    }
    {
        lf_set_ptr_x(((state.win->getWidth() - thumbnailContainerSize.x) / 2.0f - DIV_START_X));

        float ptrX = lf_get_ptr_x();
        float ptrY = lf_get_ptr_y();
        lf_rect_render(LF_PTR, 
                thumbnailContainerSize, PLAYLIST_FILE_THUMBNAIL_COLOR, LF_NO_COLOR, 0.0f, PLAYLIST_FILE_THUMBNAIL_CORNER_RADIUS * 2.0f);

        float aspect = (float)tab.trackThumbnail.width / (float)tab.trackThumbnail.height;

        float thumbnailHeight = thumbnailContainerSize.y / aspect; 

        lf_image_render((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + (thumbnailContainerSize.y - thumbnailHeight) / 2.0f}, 
                LF_WHITE, (LfTexture){.id = tab.trackThumbnail.id, .width = (uint32_t)thumbnailContainerSize.x, .height = (uint32_t)thumbnailHeight}, LF_NO_COLOR, 0.0f, 0.0f);   

        lf_set_ptr_x(ptrX);
        lf_set_ptr_y(ptrY + thumbnailContainerSize.y);
    }
    // Sound Title
    {
        std::filesystem::path fsPath(state.currentSoundFile->path);
        std::wstring filename = removeFileExtensionW(fsPath.filename().wstring());
        lf_set_text_wrap(false);
        if(state.win->getWidth() <= WIN_START_W / 3.0f)
            lf_push_font(&state.h6Font);
        float titleWidth = lf_text_dimension_wide(filename.c_str()).x;
        float titlePosX = (state.win->getWidth() - titleWidth) / 2.0f - DIV_START_X;
        lf_set_ptr_x(titlePosX);
        lf_text_wide(filename.c_str());
        lf_set_text_wrap(true);
        if(state.win->getWidth() <= WIN_START_W / 3.0f)
            lf_pop_font();
        lf_next_line();
    }
    // Playlist Display
    {
        float imageSize = 32;
        lf_next_line();
        lf_set_ptr_x((state.win->getWidth() - (lf_text_dimension(state.playlists[state.playingPlaylist].name.c_str()).x + 
                        lf_get_theme().image_props.margin_right * 2 + imageSize * 2)) / 2.0f);
        lf_set_ptr_y(lf_get_ptr_y() - 2.5f);
        {
            LfUIElementProps props = lf_get_theme().button_props;
            props.margin_left = 0;
            props.margin_top = -5;
            props.margin_bottom = 0;
            props.padding = 0;
            props.corner_radius = 3;
            lf_push_style_props(props);
            if(lf_image_button(((LfTexture){.id = state.icons["music_note"].id, .width = (uint32_t)imageSize, .height = (uint32_t)imageSize})) == LF_CLICKED) {
                state.currentPlaylist = state.playingPlaylist;
                changeTabTo(GuiTab::OnPlaylist);
            }
            lf_pop_style_props();
        }
        {
            LfUIElementProps props = lf_get_theme().text_props;
            props.text_color = LYSSA_PLAYLIST_COLOR;
            props.margin_top = 2.5f;
            lf_push_style_props(props);
            lf_text(state.playlists[state.playingPlaylist].name.c_str());
            lf_pop_style_props();
        }
        lf_set_ptr_y(lf_get_ptr_y() + 2.5);
    }

    vec2s progressBarSize = {MAX(state.win->getWidth() / 3.0f, 200), 10}; 
    // Progress position in seconds
    {
        lf_push_font(&state.h6Font);
        std::string durationMins = formatDurationToMins(state.soundHandler.getPositionInSeconds());
        lf_set_ptr_x_absolute((state.win->getWidth() - progressBarSize.x) / 2.0f - lf_text_dimension(durationMins.c_str()).x - 15);
        LfUIElementProps props = lf_get_theme().text_props;
        props.margin_top = 20 - (lf_text_dimension(durationMins.c_str()).y - state.trackProgressSlider.handle_size) / 2.0;
        lf_push_style_props(props);
        lf_text(durationMins.c_str());
        lf_pop_style_props();
        lf_pop_font();
    }
    // Total sound duration
    {
        lf_push_font(&state.h6Font);
        lf_set_ptr_x_absolute((state.win->getWidth() - progressBarSize.x) / 2.0f + state.trackProgressSlider.width + 5);
        LfUIElementProps props = lf_get_theme().text_props;
        std::string durationMins = formatDurationToMins(state.soundHandler.lengthInSeconds); 
        props.margin_top = 20 - (lf_text_dimension(durationMins.c_str()).y - state.trackProgressSlider.handle_size) / 2.0;
        lf_push_style_props(props);
        lf_text(durationMins.c_str());
        lf_pop_style_props();
        lf_pop_font();
    }
    // Progress Bar 
    {
        lf_set_ptr_x((state.win->getWidth() - progressBarSize.x) / 2.0f - DIV_START_X);

        LfUIElementProps props = lf_get_theme().slider_props;
        props.margin_top = 20;
        props.corner_radius = 1.5;
        props.color = (LfColor){255, 255, 255, 30};
        props.text_color = LF_WHITE;
        props.border_width = 0;
        lf_push_style_props(props);

        state.trackProgressSlider.width = progressBarSize.x;

        vec2s posPtr = (vec2s){lf_get_ptr_x() + props.margin_left, lf_get_ptr_y() + props.margin_top};

        LfClickableItemState slider = lf_slider_int(&state.trackProgressSlider);

        lf_rect_render(posPtr, (vec2s){(float)state.trackProgressSlider.handle_pos, (float)state.trackProgressSlider.height}, LF_WHITE, LF_NO_COLOR, 0.0f, props.corner_radius);

        if(slider == LF_RELEASED || slider == LF_CLICKED) {
            state.soundHandler.setPositionInSeconds(state.currentSoundPos);
        }

        lf_pop_style_props();
    }

    lf_next_line();

    // Sound Controls
    {

        LfUIElementProps props = lf_get_theme().button_props;
        props.margin_left = 15;
        props.margin_right = 15;

        lf_set_ptr_x((state.win->getWidth() - ((iconSizeSm + props.padding * 2.0f) + (iconSizeXsm * 2.0f) + (props.margin_left + props.margin_right) * 3.0f)) / 2.0f - DIV_START_X);

        props.color = LF_NO_COLOR;
        props.border_width = 0; 
        props.corner_radius = 0; 
        props.margin_top = iconSizeXsm / 2.0f;
        props.padding = 0;

        lf_push_style_props(props);
        if(lf_image_button(((LfTexture){.id = state.icons["skip_down"].id, .width = (uint32_t)iconSizeXsm, .height = (uint32_t)iconSizeXsm})) == LF_CLICKED) {
            skipSoundDown(state.currentPlaylist);
        }
        lf_pop_style_props();

        {
            props.color = LF_WHITE;
            props.corner_radius = 16;
            props.padding = 10;
            props.margin_top = 0;
            lf_push_style_props(props);
            if(lf_image_button(((LfTexture){.id = state.soundHandler.isPlaying ? state.icons["pause"].id : state.icons["play"].id, .width =(uint32_t)iconSizeSm, .height =(uint32_t)iconSizeSm})) == LF_CLICKED) {
                if(state.soundHandler.isPlaying)
                    state.soundHandler.stop();
                else 
                    state.soundHandler.play();
            }
            lf_pop_style_props();
        }

        props.color = LF_NO_COLOR;
        props.border_width = 0;
        props.corner_radius = 0;
        props.margin_top = iconSizeXsm / 2.0f;
        props.padding = 0;

        lf_push_style_props(props);
        if(lf_image_button(((LfTexture){.id = state.icons["skip_up"].id, .width = (uint32_t)iconSizeXsm, .height = (uint32_t)iconSizeXsm})) == LF_CLICKED) {
            skipSoundUp(state.currentPlaylist);
        }
        lf_pop_style_props();
    } 

    lf_set_ptr_y_absolute(ptr_y);;

    backButtonTo(GuiTab::OnPlaylist, [&](){
            if(tab.trackThumbnail.width != 0)
            lf_free_texture(tab.trackThumbnail);
            });   
    renderTrackMenu();
}
void renderPlaylistAddFromFile() {
    // Heading
    {
        lf_push_font(&state.h1Font);
        std::string text = "Add File to " + state.playlists[state.currentPlaylist].name;
        lf_text(text.c_str());
        lf_pop_font();
    }

    // Form Input 
    {
        lf_next_line();
        LfUIElementProps props = input_field_style();
        lf_push_style_props(props);
        lf_input_text(&state.playlistAddFromFileTab.pathInput.input);
        lf_pop_style_props();
        lf_next_line();
    }
    // Add Button 
    {
        lf_next_line();
        lf_push_style_props(call_to_action_button_style());
        if(lf_button_fixed("Add", 90, -1) == LF_CLICKED) {
            state.playlistAddFromFileTab.addFileStatus = addFileToPlaylist(state.playlistAddFromFileTab.pathInput.input.buf, state.currentPlaylist);
            memset(state.playlistAddFromFileTab.pathInput.input.buf, 0, INPUT_BUFFER_SIZE);
            memset(state.playlistAddFromFileTab.pathInput.buffer, 0, INPUT_BUFFER_SIZE);
            state.playlistAddFromFileTab.addFileMessageTimer = 0.0f;
        }
        lf_pop_style_props();
    } 

    // File Status Message
    if(state.playlistAddFromFileTab.addFileStatus != FileStatus::None) {
        if(state.playlistAddFromFileTab.addFileMessageTimer < state.playlistAddFromFileTab.addFileMessageShowTime) {
            state.playlistAddFromFileTab.addFileMessageTimer += state.deltaTime;
            lf_next_line();
            lf_push_font(&state.h4Font);
            LfUIElementProps props = lf_get_theme().button_props;
            switch(state.playlistAddFromFileTab.addFileStatus) {
                case FileStatus::Failed:
                    props.text_color = LYSSA_RED;
                    lf_push_style_props(props);
                    lf_text("Failed to add file to playlist.");
                    break;
                case FileStatus::AlreadyExists:
                    props.text_color = LYSSA_RED;
                    lf_push_style_props(props);
                    lf_text("File already exists in playlist.");
                    break;
                case FileStatus::Success:
                    props.text_color = LYSSA_GREEN;
                    lf_push_style_props(props);
                    lf_text("Added file to playlist.");
                    break;
                default:
                    break;
            }
            lf_pop_font();
        }
    }
    backButtonTo(GuiTab::OnPlaylist);
    renderTrackMenu();
}

static void renderTopBarAddFromFolder() {
    PlaylistAddFromFolderTab& tab = state.playlistAddFromFolderTab;
    LfUIElementProps props = lf_get_theme().text_props;
    const LfColor barColor = lf_color_brightness(GRAY, 0.5);

    props.color = barColor;
    props.corner_radius = 4.0f;
    props.padding = 12.0f;
    props.border_width = 0.0f;
    props.margin_right = 0.0f;
    lf_push_style_props(props);
    lf_text_wide(state.playlistAddFromFolderTab.currentFolderPath.c_str());
    lf_pop_style_props();

    props = primary_button_style();
    props.padding = 12.0f;
    props.margin_left = 0.0f;
    props.margin_right = 0;
    lf_push_style_props(props);
    LfClickableItemState addAllButton = lf_button("Add All");
    if(addAllButton != LF_IDLE) {
        lf_text("Adds all files from the current folder");
    }
    if(addAllButton == LF_CLICKED) {
        if(!state.playlistFileThumbnailData.empty()) {
            state.playlistFileThumbnailData.clear();
        }
        Playlist& currentPlaylist = state.playlists[state.currentPlaylist];
        std::ofstream metadata(currentPlaylist.path + "/.metadata", std::ios::app);
        metadata.seekp(0, std::ios::end);
        for(const auto& entry : tab.folderContents) {
            if(!entry.is_directory() && 
                    !isFileInPlaylistMetadata(entry.path().string(), state.currentPlaylist) && 
                    isValidSoundFile(entry.path().string())) {
                metadata << "\"" << entry.path().string() << "\" ";
                state.loadedPlaylistFilepaths.push_back(entry.path().string());
            }
        }
        metadata.close();
    }
    lf_pop_style_props();
}
void renderPlaylistAddFromFolder() {
    {
        lf_push_font(&state.h1Font);
        LfUIElementProps props = lf_get_theme().text_props;
        props.margin_bottom = 15;
        lf_push_style_props(props);
        lf_text("Add files from folder");
        lf_pop_style_props();
        lf_pop_font();
    }

    lf_next_line();
    renderFileDialogue(
            [&](std::filesystem::directory_entry entry){
                PlaylistAddFromFolderTab& tab = state.playlistAddFromFolderTab;
                tab.currentFolderPath = entry.path().wstring();
                tab.folderContents.clear();
                tab.folderContents = loadFolderContents(state.playlistAddFromFolderTab.currentFolderPath);
                lf_set_current_div_scroll(0.0f);
                lf_set_current_div_scroll_velocity(0.0f);
            },
            [&](){
                PlaylistAddFromFolderTab& tab = state.playlistAddFromFolderTab;
                tab.currentFolderPath = std::filesystem::path(tab.currentFolderPath).parent_path().wstring();
                tab.folderContents.clear();
                tab.folderContents = loadFolderContents(state.playlistAddFromFolderTab.currentFolderPath);
            },
            renderTopBarAddFromFolder,
            [&](std::filesystem::directory_entry entry, bool hovered){
                (void)hovered;
                LfUIElementProps props = lf_get_theme().button_props;
                props.margin_top = 0.0f;
                props.color = LF_NO_COLOR;
                props.padding = 2.5f;
                props.border_width = 0.0f;
                lf_set_image_color((isFileInPlaylistMetadata(entry.path().string(), state.currentPlaylist) && !entry.is_directory()) ? LYSSA_GREEN : LF_WHITE);
                lf_push_style_props(props);
                const vec2s iconSize = (vec2s){25, 25};
                LfTexture icon = (LfTexture){
                    .id = (entry.is_directory()) ? state.icons["folder"].id : state.icons["file"].id, 
                        .width = (uint32_t)iconSize.x, 
                        .height = (uint32_t)iconSize.y
                };
                if(lf_image_button(icon) == LF_CLICKED && !entry.is_directory() && 
                        !isFileInPlaylistMetadata(entry.path().string(), state.currentPlaylist) && 
                        isValidSoundFile(entry.path().string())) {
                    std::ofstream metadata(state.playlists[state.currentPlaylist].path + "/.metadata", std::ios::app);
                    metadata.seekp(0, std::ios::end);
                    metadata << "\"" << entry.path().string() << "\" ";
                    state.loadedPlaylistFilepaths.push_back(entry.path().string());
                }
                lf_pop_style_props();
                lf_unset_image_color();
            },
            nullptr,
            state.playlistAddFromFolderTab.folderContents, false);

    backButtonTo(GuiTab::OnPlaylist, [&](){
            state.playlists[state.currentPlaylist].musicFiles.clear();
            loadPlaylistAsync(state.playlists[state.currentPlaylist]);
    });
    renderTrackMenu();
}

void renderFileDialogue(
        std::function<void(std::filesystem::directory_entry)> clickedEntryCb, 
        std::function<void()> clickedBackCb, 
        std::function<void()> renderTopBarCb, 
        std::function<void(std::filesystem::directory_entry, bool)> renderIconCb,
        std::function<bool(std::filesystem::directory_entry, bool)> renderPerEntryCb,
        const std::vector<std::filesystem::directory_entry>& folderContents, 
        bool renderDirectoriesOnly)
{
    bool clickedBackBtn = false;
    LfTexture backIcon = (LfTexture){
        .id = state.icons["back"].id, 
            .width = BACK_BUTTON_WIDTH / 2,
            .height = BACK_BUTTON_HEIGHT / 2
    };

    const LfColor barColor = lf_color_brightness(GRAY, 0.5);
    LfUIElementProps props = lf_get_theme().button_props;
    props.border_width = 0.0f;
    props.corner_radius = 4.0f;
    props.color = barColor; 
    lf_push_style_props(props);
    if(lf_image_button_fixed(backIcon, 50, -1) == LF_CLICKED) {
        if(clickedBackCb)
            clickedBackCb();
        clickedBackBtn = true;
    }
    lf_pop_style_props();
    if(renderTopBarCb)
        renderTopBarCb();
    lf_next_line();


    LfUIElementProps divProps = lf_get_theme().div_props;
    divProps.color = lf_color_brightness(GRAY, 0.4);
    divProps.corner_radius = 10.0f;
    divProps.padding = 10.0f;

    float divMarginButton = 15;

    lf_push_style_props(divProps);
    lf_div_begin(((vec2s){lf_get_ptr_x() + DIV_START_X - lf_get_theme().button_props.margin_left, lf_get_ptr_y() + DIV_START_Y}), (((vec2s){(float)state.win->getWidth() - (DIV_START_X * 4),
                    (float)state.win->getHeight() - DIV_START_Y * 2 - lf_get_ptr_y() - (BACK_BUTTON_HEIGHT + BACK_BUTTON_MARGIN_BOTTOM) - divMarginButton})), true);

    vec2s initialPtr = LF_PTR;

    if(clickedBackBtn) {
        lf_set_current_div_scroll(0.0f);
        lf_set_current_div_scroll_velocity(0.0f);
    }

    const vec2s iconSize = (vec2s){25, 25};

    uint32_t directoryCount = 0;
    for(auto& entry : folderContents) {
        if(entry.is_directory())
            directoryCount++;
    }
    if(folderContents.empty() || (renderDirectoriesOnly && directoryCount == 0)) {
        lf_text("This directory is empty.");
    }
    for(auto& entry : folderContents) {
        if(!entry.is_directory() && renderDirectoriesOnly) continue;
        LfAABB aabb = (LfAABB){
            .pos = (vec2s){lf_get_current_div().aabb.pos.x, lf_get_ptr_y()},
                .size = (vec2s){lf_get_current_div().aabb.size.x, iconSize.y}
        };

        bool hoveredEntry = lf_hovered(aabb.pos, aabb.size);
        if(hoveredEntry) {
            lf_rect_render(aabb.pos, (vec2s){aabb.size.x, aabb.size.y + 5.0f}, (LfColor){100, 100, 100, 255}, LF_NO_COLOR, 0.0f, 3.0f);
        }

        bool onClientUI = false;
        if(renderPerEntryCb)
            onClientUI = renderPerEntryCb(entry, hoveredEntry);


        if(renderIconCb) {
            renderIconCb(entry, hoveredEntry);
        } else {
            LfTexture icon = (LfTexture){
                .id = (entry.is_directory()) ? state.icons["folder"].id : state.icons["file"].id, 
                    .width = (uint32_t)iconSize.x, 
                    .height = (uint32_t)iconSize.y
            };
            LfUIElementProps props = lf_get_theme().button_props;
            props.margin_top = 0.0f;
            props.color = LF_NO_COLOR;
            props.padding = 2.5f;
            props.border_width = 0.0f;
            lf_push_style_props(props);
            lf_image_button(icon); 
            lf_pop_style_props();
        }

        props = lf_get_theme().text_props;
        props.margin_top = (iconSize.y - lf_text_dimension_wide(entry.path().filename().wstring().c_str()).y) / 2.0f;
        lf_push_style_props(props);
        lf_text_wide(entry.path().filename().wstring().c_str());
        lf_pop_style_props();

        if(hoveredEntry && lf_mouse_button_went_down(GLFW_MOUSE_BUTTON_LEFT) && entry.is_directory()) {
            if(clickedEntryCb && !onClientUI)
                clickedEntryCb(entry);
            break;
        }
        lf_next_line();
    }

    lf_div_end();
    lf_pop_style_props();
}

void renderAorBPopup(AorBPopup& popup) {
    if(!popup.render) return;
    // Beginning a new div
    const vec2s popupSize = (vec2s){(float)popup.width, 100.0f};
    LfUIElementProps props = lf_get_theme().div_props;
    props.color = lf_color_brightness(GRAY, 0.45);
    props.padding = 0;
    props.corner_radius = 10;
    lf_push_style_props(props);
    // Centering the div/popup
    lf_div_begin(((vec2s){(state.win->getWidth() - popupSize.x) / 2.0f, (state.win->getHeight() - popupSize.y) / 2.0f}), popupSize, false); 
    if(!lf_div_grabbed()) {
        lf_div_grab(lf_get_current_div());
    }

    // Close Button
    {
        // Styling
        LfUIElementProps props = lf_get_theme().button_props;
        props.margin_left = 0;
        props.margin_right = 0;
        props.margin_top = 0;
        props.margin_bottom = 0;
        props.text_color = (LfColor){255, 255, 255, 255};
        props.color = LF_NO_COLOR;
        props.border_width = 0;

        lf_push_style_props(props);
        if(lf_button("X") == LF_CLICKED) {
            state.aOrBPopup.render = false;
            lf_div_ungrab();
        }
        lf_pop_style_props();
        lf_next_line();
        lf_set_ptr_y(20);
    }
    // Popup Title
    {
        const char* text = popup.title.c_str();
        lf_push_font(&state.h6Font);
        float textWidth = lf_text_dimension(text).x;
        lf_set_ptr_x((lf_get_current_div().aabb.size.x - textWidth) / 2.0f);
        lf_text(text);
        lf_pop_font();
    }
    // Popup Buttons
    lf_next_line();
    {
        // Styling
        LfUIElementProps bprops = primary_button_style();
        lf_push_style_props(bprops);

        // Make the buttons stretch the entire div
        float halfDivWidth = lf_get_current_div().aabb.size.x / 2.0f - bprops.padding * 2.0f - bprops.border_width * 2.0f  - (bprops.margin_left + bprops.margin_right);
        if(lf_button_fixed(popup.aStr.c_str(), halfDivWidth, -1) == LF_CLICKED) {
            popup.aCb();
        }
        if(lf_button_fixed(popup.bStr.c_str(), halfDivWidth, -1) == LF_CLICKED) {
            popup.bCb();
        }
        lf_pop_style_props();
    }
    lf_div_end();
    lf_pop_style_props();
}
void renderEditPlaylistPopup() {
    // Beginning a new div
    const vec2s popupSize = (vec2s){500.0f, 350.0f};
    LfUIElementProps div_props = lf_get_theme().div_props;
    div_props.color = lf_color_brightness(GRAY, 0.7); 
    div_props.border_width = 0;
    div_props.padding = 5;
    div_props.corner_radius = 10;
    lf_push_style_props(div_props);
    // Centering the div/popup
    lf_div_begin(((vec2s){(state.win->getWidth() - popupSize.x) / 2.0f, (state.win->getHeight() - popupSize.y) / 2.0f}), popupSize, false);
    if(!lf_div_grabbed()) {
        lf_div_grab(lf_get_current_div());
    }
    // Close Button
    {
        // Put the X Button in the top left of the div 

        // Styling
        LfUIElementProps props = lf_get_theme().button_props;
        props.margin_left = 0;
        props.margin_right = 0;
        props.margin_top = 0;
        props.margin_bottom = 0;
        props.border_width = 0;
        props.text_color = LF_WHITE;
        props.color = LF_NO_COLOR;

        lf_push_style_props(props);
        if(lf_button("X") == LF_CLICKED) {
            state.popups[(int32_t)PopupID::EditPlaylistPopup].render = false;
            lf_div_ungrab();

            memset(state.createPlaylistTab.nameInput.input.buf, 0, INPUT_BUFFER_SIZE);
            memset(state.createPlaylistTab.descInput.input.buf, 0, INPUT_BUFFER_SIZE);

            memset(state.createPlaylistTab.nameInput.buffer, 0, INPUT_BUFFER_SIZE);
            memset(state.createPlaylistTab.descInput.buffer, 0, INPUT_BUFFER_SIZE);
        }
        lf_pop_style_props();
    }

    lf_next_line();
    {
        LfUIElementProps props = lf_get_theme().text_props;
        props.margin_left = 20;
        props.margin_bottom = 5;
        lf_push_style_props(props);
        lf_text("Name");
        lf_pop_style_props();
    }
    lf_next_line();
    { 
        state.createPlaylistTab.nameInput.input.width = 457;
        LfUIElementProps props = input_field_style();
        props.color = lf_color_brightness(GRAY, 0.4);
        lf_push_style_props(props);
        lf_input_text(&state.createPlaylistTab.nameInput.input);
        lf_pop_style_props();
        lf_next_line();
    }

    lf_next_line();
    {
        LfUIElementProps props = lf_get_theme().text_props;
        props.margin_left = 20;
        props.margin_bottom = 5;
        props.margin_top = 15;
        lf_push_style_props(props);
        lf_text("Description");
        lf_pop_style_props();
    }
    lf_next_line();
    { 
        state.createPlaylistTab.descInput.input.width = 457;
        LfUIElementProps props = input_field_style(); 
        props.color = lf_color_brightness(GRAY, 0.4);
        lf_push_style_props(props);
        lf_input_text(&state.createPlaylistTab.descInput.input);
        lf_pop_style_props();
        lf_next_line();
    }
    lf_next_line();
    {
        LfUIElementProps props = primary_button_style();
        props.margin_top = 15;
        lf_push_style_props(props);
        if(lf_button_fixed("Done", 150, -1) == LF_CLICKED) {
            renamePlaylist(std::string(state.createPlaylistTab.nameInput.input.buf), state.currentPlaylist);
            changePlaylistDesc(std::string(state.createPlaylistTab.descInput.input.buf), state.currentPlaylist);
            state.popups[(int32_t)PopupID::EditPlaylistPopup].render = false;
            memset(state.createPlaylistTab.nameInput.input.buf, 0, INPUT_BUFFER_SIZE);
            memset(state.createPlaylistTab.descInput.input.buf, 0, INPUT_BUFFER_SIZE);

            memset(state.createPlaylistTab.nameInput.buffer, 0, INPUT_BUFFER_SIZE);
            memset(state.createPlaylistTab.descInput.buffer, 0, INPUT_BUFFER_SIZE);
        }
    }

    lf_div_end();
    lf_pop_style_props();
}

void renderPlaylistFileDialoguePopup() {
    PlaylistFileDialoguePopup& popup = state.removeFilePopup;
    const vec2s popupSize =(vec2s){200, 200};

    LfUIElementProps props = lf_get_theme().div_props;
    props.color = lf_color_brightness(GRAY, 0.35);
    props.corner_radius = 4;
    lf_push_style_props(props);
    lf_div_begin(popup.pos, popupSize, false);
    if(!lf_div_grabbed()) {
        lf_div_grab(lf_get_current_div());
    }
    lf_pop_style_props();

    static bool showPlaylistPopup = false;
    static bool onPlaylistPopup = false;

    const uint32_t options_count = 4;
    static const char* options[options_count] = {
        "Add to playlist",
        "Remove",
        "Add to favourites",
        "Open URL..."
    };

    int32_t clickedIndex = -1;
    for(uint32_t i = 0; i < options_count; i++) {
        // Option
        props = lf_get_theme().text_props;
        props.hover_text_color = lf_color_brightness(GRAY, 2);
        lf_push_style_props(props);
        if(lf_button(options[i]) == LF_CLICKED) {
            clickedIndex = i;
        }
        lf_pop_style_props();

        // Seperator
        props = lf_get_theme().button_props;
        props.color = lf_color_brightness(GRAY, 0.7);
        lf_push_style_props(props);
        lf_seperator();
        lf_pop_style_props();

        lf_next_line();
    }

    switch(clickedIndex) {
        case 0:
            {
                showPlaylistPopup = !showPlaylistPopup;
                break;
            }
        case 1: /* Remove */
            {
                if(state.currentSoundFile != nullptr) {
                    if(state.currentSoundFile->path == strToWstr(popup.path)) {
                        state.soundHandler.stop();
                        state.soundHandler.uninit();
                        state.currentSoundFile = nullptr;
                    }
                }
                removeFileFromPlaylist(popup.path, state.currentPlaylist);
                state.popups[(int32_t)PopupID::PlaylistFileDialoguePopup].render = false;
                break;
            }
        case 2: /* Add to favourites */
            {
                break;
            }
        case 3:
            {
                std::string url = getSoundComment(popup.path);
                if(url != "") {
                    std::string cmd = "xdg-open " + url + "& ";
                    system(cmd.c_str());
                }
                break;
            }
        default:
            break;
    }

    if(lf_get_current_div().id != lf_get_selected_div().id && !onPlaylistPopup && lf_mouse_button_is_released(GLFW_MOUSE_BUTTON_LEFT)) {
        state.popups[(int32_t)PopupID::PlaylistFileDialoguePopup].render = false;
        showPlaylistPopup = false;
        lf_div_ungrab();
    }
    lf_div_end();

    if(showPlaylistPopup) {
        LfUIElementProps div_props = lf_get_theme().div_props;
        div_props.color = lf_color_brightness(GRAY, 0.35);
        div_props.corner_radius = 4;
        div_props.padding = 10;
        lf_push_style_props(div_props);
        lf_div_begin(((vec2s){popup.pos.x + popupSize.x + div_props.padding, popup.pos.y}), popupSize, true);
        onPlaylistPopup = lf_get_current_div().id == lf_get_selected_div().id;
        lf_pop_style_props();

        lf_set_cull_end_x(popup.pos.x + popupSize.x * 2 + div_props.padding * 2);

        uint32_t renderedItems = 0;
        for(uint32_t i = 0; i < state.playlists.size(); i++) {
            Playlist& playlist = state.playlists[i];
            std::vector<std::string> playlistFiles = getPlaylistFilepaths(std::filesystem::directory_entry(playlist.path));

            if(i == state.currentPlaylist || std::find(playlistFiles.begin(), playlistFiles.end(), popup.path) != playlistFiles.end()) continue;
            // Playlist
            props = lf_get_theme().text_props;
            props.hover_text_color = lf_color_brightness(GRAY, 2);
            lf_push_style_props(props);
            if(lf_button(playlist.name.c_str()) == LF_CLICKED) {
                if(playlist.loaded) {
                    addFileToPlaylist(popup.path, i);
                    playlist.loaded = false;
                } else {
                    std::ofstream metadata(playlist.path + "/.metadata", std::ios::app);
                    metadata.seekp(0, std::ios::end);

                    metadata << "\"" << popup.path << "\" ";
                    metadata.close();
                    playlist.loaded = false;
                }
                state.popups[(int32_t)PopupID::PlaylistFileDialoguePopup].render = false;
                showPlaylistPopup = false;
            }
            lf_pop_style_props();

            // Seperator
            props = lf_get_theme().button_props;
            props.color = lf_color_brightness(GRAY, 0.7);
            lf_push_style_props(props);
            lf_seperator();
            lf_pop_style_props();

            lf_next_line();
            renderedItems++;
        }
        if(renderedItems == 0) {
            lf_text("No other playlists");
        }
        lf_unset_cull_end_x();
        lf_div_end();
    }
}

void renderTrackDisplay() {
    if(state.currentSoundFile == NULL) return;
    const float margin = DIV_START_X;
    const float marginThumbnail = 15;
    const vec2s thumbnailContainerSize = (vec2s){48, 48};
    const float padding = 10;

    std::filesystem::path filepath = state.currentSoundFile->path;

    std::wstring filename = removeFileExtensionW(filepath.filename().wstring());
    std::wstring artist = getSoundArtist(filepath.string());

    const SoundFile& playlingFile = state.playlists[state.playingPlaylist].musicFiles[state.playlists[state.playingPlaylist].playingFile];

    // Container 
    float containerPosX = (float)(state.win->getWidth() - state.trackProgressSlider.width) / 2.0f + state.trackProgressSlider.width + 
        lf_text_dimension(formatDurationToMins(state.soundHandler.lengthInSeconds).c_str()).x + margin;
    vec2s containerSize = (vec2s){
        .x = (state.win->getWidth() - margin) - containerPosX,
            .y = thumbnailContainerSize.y + padding * 2.0f
    };

    vec2s containerPos = (vec2s){
        containerPosX,
            (float)state.win->getHeight() - containerSize.y - margin
    };

    lf_rect_render(containerPos, containerSize, lf_color_brightness(GRAY, 0.5), LF_NO_COLOR, 0.0f, 4.5f); 

    // Thumbnail
    {
        lf_rect_render((vec2s){containerPos.x + padding, containerPos.y + padding}, 
                thumbnailContainerSize, PLAYLIST_FILE_THUMBNAIL_COLOR, LF_NO_COLOR, 0.0f, PLAYLIST_FILE_THUMBNAIL_CORNER_RADIUS);


        float aspect = (float)playlingFile.thumbnail.width / (float)playlingFile.thumbnail.height;

        float thumbnailHeight = thumbnailContainerSize.y / aspect; 


        lf_set_ptr_x_absolute(containerPos.x + padding);
        lf_set_ptr_y_absolute(containerPos.y + padding + (thumbnailContainerSize.y - thumbnailHeight) / 2.0f);

        LfUIElementProps props = lf_get_theme().button_props;
        props.margin_top = 0;
        props.margin_bottom = 0;
        props.margin_left = 0;
        props.margin_right = 0;
        props.padding = 0;
        lf_push_style_props(props);
        if(lf_image_button((
                    (LfTexture){
                        .id = playlingFile.thumbnail.id, 
                        .width = (uint32_t)thumbnailContainerSize.x, 
                        .height = (uint32_t)thumbnailHeight})) == LF_CLICKED) { 
            if(state.onTrackTab.trackThumbnail.width != 0 && state.currentSoundFile) 
                lf_free_texture(state.onTrackTab.trackThumbnail);
            state.onTrackTab.trackThumbnail = getSoundThubmnail(state.currentSoundFile->pathStr);
            changeTabTo(GuiTab::OnTrack);
        }
        lf_pop_style_props();
    }
    // Name + Artist
    {
        float textLabelHeight = lf_text_dimension_wide(filename.c_str()).y + lf_text_dimension_wide(artist.c_str()).y + 5.0f;
        lf_set_line_should_overflow(false);
        lf_set_cull_end_x(containerPos.x + containerSize.x - padding);
        lf_text_render_wchar(
                (vec2s){containerPos.x + padding + thumbnailContainerSize.x + marginThumbnail, 
                containerPos.y + padding + (thumbnailContainerSize.y -  textLabelHeight) / 2.0f},
                filename.c_str(), state.h6Font, -1, false, LF_WHITE);

        lf_text_render_wchar(
                (vec2s){containerPos.x + padding + thumbnailContainerSize.x + marginThumbnail, 
                containerPos.y + padding + (thumbnailContainerSize.y - textLabelHeight) / 2.0f + lf_text_dimension_wide(artist.c_str()).y + 5.0f},
                artist.c_str(), state.h6Font, -1, false, lf_color_brightness(GRAY, 1.4));
        lf_unset_cull_end_x();
        lf_set_line_should_overflow(true);
    }
}
void renderTrackProgress() {
    if(state.currentSoundFile == NULL) return;
    // Progress position in seconds
    {
        lf_push_font(&state.h6Font);
        std::string durationMins = formatDurationToMins(state.soundHandler.getPositionInSeconds());
        lf_set_ptr_x_absolute((state.win->getWidth() - state.trackProgressSlider.width) / 2.0f - lf_text_dimension(durationMins.c_str()).x - 15);
        LfUIElementProps props = lf_get_theme().text_props;
        props.margin_top = 40 - (lf_text_dimension(durationMins.c_str()).y - state.trackProgressSlider.handle_size) / 2.0;
        lf_push_style_props(props);
        lf_text(durationMins.c_str());
        lf_pop_style_props();
        lf_pop_font();
    }
    // Total sound duration
    {
        lf_push_font(&state.h6Font);
        lf_set_ptr_x_absolute((state.win->getWidth() - state.trackProgressSlider.width) / 2.0f + state.trackProgressSlider.width + 5);
        LfUIElementProps props = lf_get_theme().text_props;
        std::string durationMins = formatDurationToMins(state.soundHandler.lengthInSeconds); 
        props.margin_top = 40 - (lf_text_dimension(durationMins.c_str()).y - state.trackProgressSlider.handle_size) / 2.0;
        lf_push_style_props(props);
        lf_text(durationMins.c_str());
        lf_pop_style_props();
        lf_pop_font();
    }
    // Progress Bar 
    {
        state.trackProgressSlider.width = state.win->getWidth() / 2.5f;

        lf_set_ptr_x_absolute((state.win->getWidth() - state.trackProgressSlider.width) / 2.0f);

        LfUIElementProps props = lf_get_theme().slider_props;
        props.margin_top = 40;
        props.margin_left = 0;
        props.margin_right = 0;
        props.corner_radius = 1.5;
        props.color = (LfColor){255, 255, 255, 30};
        props.text_color = LF_WHITE;
        props.border_width = 0;
        lf_push_style_props(props);

        vec2s posPtr = (vec2s){lf_get_ptr_x() + props.margin_left, lf_get_ptr_y() + props.margin_top};

        LfClickableItemState progressBar = lf_slider_int(&state.trackProgressSlider);

        lf_rect_render(posPtr, (vec2s){(float)state.trackProgressSlider.handle_pos, (float)state.trackProgressSlider.height}, LF_WHITE, LF_NO_COLOR, 0.0f, props.corner_radius);

        if(progressBar == LF_RELEASED || progressBar == LF_CLICKED) {
            state.soundHandler.setPositionInSeconds(state.currentSoundPos);
        }

        lf_pop_style_props();
    }
    lf_next_line();

    {
        vec2s iconSize = (vec2s){36, 36};
        vec2s iconSizeSm = (vec2s){18, 18};
        float iconMargin = 20;
        float controlWidth = (iconSizeSm.x) * 2 + iconSize.x + (iconMargin * 2);
        LfUIElementProps props = lf_get_theme().button_props;
        props.color = LF_NO_COLOR;
        props.border_width = 0; 
        props.corner_radius = 0; 
        props.margin_top = (iconSize.x - iconSizeSm.x) / 2.0f - 5;
        props.margin_left = 0;
        props.margin_right = iconMargin;
        props.padding = 0;

        lf_set_ptr_x_absolute((state.win->getWidth() - controlWidth) / 2.0f);
        lf_push_style_props(props);
        lf_set_image_color(lf_color_brightness(GRAY, 2));
        if(lf_image_button(((LfTexture){.id = state.icons["skip_down"].id, .width = (uint32_t)iconSizeSm.x, .height = (uint32_t)iconSizeSm.y})) == LF_CLICKED) {
            skipSoundDown(state.playingPlaylist);
        }
        lf_unset_image_color();
        {
            LfUIElementProps playProps = props; 
            playProps.color = LF_WHITE;
            playProps.corner_radius = 9;
            playProps.margin_top = -5;
            playProps.padding = 0;
            lf_push_style_props(playProps);    
            if(lf_image_button(((LfTexture){.id = state.soundHandler.isPlaying ? state.icons["pause"].id : state.icons["play"].id, .width = (uint32_t)iconSize.x, .height = (uint32_t)iconSize.y})) == LF_CLICKED) {
                if(state.soundHandler.isPlaying)
                    state.soundHandler.stop();
                else 
                    state.soundHandler.play();
            }
            lf_pop_style_props();
        }
        props.margin_right = 0;
        lf_push_style_props(props);
        lf_set_image_color(lf_color_brightness(GRAY, 2));
        if(lf_image_button(((LfTexture){.id = state.icons["skip_up"].id, .width = (uint32_t)iconSizeSm.x, .height = (uint32_t)iconSizeSm.y})) == LF_CLICKED) {
            skipSoundUp(state.playingPlaylist);
        }
        lf_unset_image_color();
        lf_pop_style_props();
    }
}
void renderTrackMenu() {
    renderTrackVolumeControl();
    if(state.currentTab != GuiTab::OnTrack) {
        renderTrackProgress();
        renderTrackDisplay();
    }
}

void renderTrackVolumeControl() {
    // Sound Control
    {
        LfUIElementProps props = lf_get_theme().button_props;
        const vec2s iconSize = (vec2s){state.icons["sound"].width / 10.0f, state.icons["sound"].height / 10.0f};

        props.color = LF_NO_COLOR;
        props.border_color = LF_NO_COLOR;
        props.border_width = 0.0f;
        props.margin_top = 12.5;
        lf_push_style_props(props);

        uint32_t buttonIcon = (state.soundHandler.volume == 0.0) ? state.icons["sound_off"].id : state.icons["sound"].id; 
        bool overControlArea = lf_hovered((vec2s){lf_get_ptr_x() + props.margin_left, lf_get_ptr_y() + props.margin_top}, (vec2s){
                (float)state.win->getWidth(), iconSize.y + props.margin_top + props.margin_bottom});

        LfClickableItemState soundButton = lf_image_button(((LfTexture){.id = buttonIcon, .width = (uint32_t)iconSize.x, .height = (uint32_t)iconSize.y}));

        if(!state.showVolumeSliderTrackDisplay) {
            overControlArea = (soundButton == LF_HOVERED);
        }
        if(overControlArea && !state.showVolumeSliderTrackDisplay) {
            state.showVolumeSliderTrackDisplay = true; 
        }
        else if(!overControlArea && !state.volumeSlider.held && !state.showVolumeSliderOverride) {
            state.showVolumeSliderTrackDisplay = false;
        }
        else if(soundButton == LF_CLICKED) { 
            if(state.soundHandler.volume != 0.0f) {
                state.volumeBeforeMute = state.soundHandler.volume;
            }
            state.soundHandler.volume = (state.soundHandler.volume != 0.0f) ? 0.0f : state.volumeBeforeMute;
            state.volumeSlider._init = false;
        }
        lf_pop_style_props();
    }
    if(state.showVolumeSliderTrackDisplay) {
        LfUIElementProps props = lf_get_theme().slider_props;
        props.corner_radius = 1.5;
        props.color = (LfColor){255, 255, 255, 30};
        props.text_color = LF_WHITE;
        props.border_width = 0;
        props.margin_top = 40;
        lf_push_style_props(props);

        lf_rect_render((vec2s){lf_get_ptr_x() + props.margin_left, lf_get_ptr_y() + props.margin_top}, (vec2s){(float)state.volumeSlider.handle_pos, (float)state.volumeSlider.height}, 
                props.text_color, LF_NO_COLOR, 0.0f, props.corner_radius);
        lf_slider_int(&state.volumeSlider);
        lf_pop_style_props();
    } 
}
void backButtonTo(GuiTab tab, const std::function<void()>& clickCb ) {
    lf_next_line();

    lf_set_ptr_y(state.win->getHeight() - BACK_BUTTON_MARGIN_BOTTOM - BACK_BUTTON_HEIGHT * 2);
    LfUIElementProps props = lf_get_theme().button_props;
    props.color = (LfColor){0, 0, 0, 0};
    props.border_width = 0;
    lf_push_style_props(props);

    if(lf_image_button(((LfTexture){.id = state.icons["back"].id, .width = BACK_BUTTON_WIDTH, .height = BACK_BUTTON_HEIGHT})) == LF_CLICKED) {
        if(clickCb)
            clickCb();
        changeTabTo(tab);
    }

    lf_pop_style_props();
}
void changeTabTo(GuiTab tab) {
    if(state.currentTab == tab) return;
    state.currentTab = tab;
}

FileStatus createPlaylist(const std::string& name, const std::string& desc, const std::string& url) {
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
FileStatus renamePlaylist(const std::string& name, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    playlist.name = name; 
    return savePlaylist(playlistIndex);
}
FileStatus deletePlaylist(uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];

    if(!std::filesystem::exists(playlist.path) || !std::filesystem::is_directory(playlist.path)) return FileStatus::Failed;

    std::filesystem::remove_all(playlist.path);
    state.playlists.erase(std::find(state.playlists.begin(), state.playlists.end(), playlist));

    return FileStatus::Success;
}

FileStatus changePlaylistDesc(const std::string& desc, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    playlist.desc = desc; 
    return savePlaylist(playlistIndex);
}

FileStatus savePlaylist(uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    std::ofstream metdata(playlist.path + "/.metadata", std::ios::trunc);

    if(!metdata.is_open()) return FileStatus::Failed;

    metdata << "name: " << playlist.name << "\n";
    metdata << "desc: " << playlist.desc << "\n";
    metdata << "url: " << playlist.url << "\n";
    metdata << "files: ";

    for(auto& file : playlist.musicFiles) {
        metdata << "\"" << file.path << "\" ";
    }
    return FileStatus::Success;

}
FileStatus addFileToPlaylist(const std::string& path, uint32_t playlistIndex) {
    if(isFileInPlaylist(path, playlistIndex)) return FileStatus::AlreadyExists;

    Playlist& playlist = state.playlists[playlistIndex];

    std::ofstream metadata(playlist.path + "/.metadata", std::ios::app);

    if(!metadata.is_open()) return FileStatus::Failed;

    std::ifstream playlistFile(path);
    if(!playlistFile.good()) return FileStatus::Failed;

    metadata.seekp(0, std::ios::end);

    metadata << "\"" << path << "\" ";
    metadata.close();

    state.loadedPlaylistFilepaths.emplace_back(path);

    playlist.musicFiles.emplace_back((SoundFile){
            .path = strToWstr(path), 
            .pathStr = path, 
            .duration = static_cast<int32_t>(getSoundDuration(path)),
            .thumbnail = getSoundThubmnail(path, (vec2s){0.075f, 0.075f})
            });

    return FileStatus::Success;
}

FileStatus removeFileFromPlaylist(const std::string& path, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    if(!isFileInPlaylist(path, playlistIndex)) return FileStatus::Failed;

    for(auto& file : playlist.musicFiles) {
        if(file.path == strToWstr(path)) {
            playlist.musicFiles.erase(std::find(playlist.musicFiles.begin(), playlist.musicFiles.end(), file));
            state.loadedPlaylistFilepaths.erase(std::find(state.loadedPlaylistFilepaths.begin(), state.loadedPlaylistFilepaths.end(), path));
            break;
        }
    }
    return savePlaylist(playlistIndex);
}

bool isFileInPlaylist(const std::string& path, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    for(auto& file : playlist.musicFiles) {
        if(file.path == strToWstr(path)) return true;
    }
    return false;
}
bool isFileInPlaylistMetadata(const std::string& path, uint32_t playlistIndex) {
    std::ifstream file(state.playlists[playlistIndex].path + "/.metadata");
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
        std::cerr << "[Error] Failed to open the metadata of playlist on path '" <<  state.playlists[playlistIndex].path << "'\n";
    }

    return false;
}

bool isFileInPlaylistW(const std::wstring& path, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    for(auto& file : playlist.musicFiles) {
        if(file.path == path) return true;
    }
    return false;
}

static bool isValidSoundFile(const std::string& path) {
    TagLib::FileRef file(path.c_str());
    return !file.isNull() && file.audioProperties();
}
std::string getPlaylistName(const std::filesystem::directory_entry& folder) {
    std::ifstream metadata(folder.path().string() + "/.metadata");

    if(!metadata.is_open()) {
        std::cerr << "[Error] Failed to open the metadata of playlist on path '" << folder.path().string() << "'\n";
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

std::string getPlaylistDesc(const std::filesystem::directory_entry& folder) {
    std::ifstream metadata(folder.path().string() + "/.metadata");

    if(!metadata.is_open()) {
        std::cerr << "[Error] Failed to open the metadata of playlist on path '" << folder.path().string() << "'\n";
        return "No valid name";
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

std::string getPlaylistUrl(const std::filesystem::directory_entry& folder) {
    std::ifstream metadata(folder.path().string() + "/.metadata");

    if(!metadata.is_open()) {
        std::cerr << "[Error] Failed to open the metadata of playlist on path '" << folder.path().string() << "'\n";
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
std::vector<std::string> getPlaylistFilepaths(const std::filesystem::directory_entry& folder) {
    std::ifstream metadata(folder.path().string() + "/.metadata");
    std::vector<std::string> filepaths{};

    if(!metadata.is_open()) {
        std::cerr << "[Error] Failed to open the metadata of playlist on path '" << folder.path().string() << "'\n";
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
std::vector<std::wstring> getPlaylistDisplayNamesW(const std::filesystem::directory_entry& folder) {
    std::vector<std::wstring> displayNames;
    std::wifstream wmetadata(folder.path().string() + "/.metadata");
    std::wstring wline;

    while(std::getline(wmetadata, wline)) {
        if (wline.find(L"files:") != std::wstring::npos) {
            std::wistringstream iss(wline.substr(wline.find(L"files:") + std::wstring(L"files:").length()));
            std::wstring path;

            while (iss >> std::quoted(path)) {
                displayNames.emplace_back(path);
            }
        }
    }
    wmetadata.close();
    return displayNames;
}

void loadPlaylists() {
    uint32_t playlistI = 0;
    for (const auto& folder : std::filesystem::directory_iterator(LYSSA_DIR + "/playlists/")) {
        Playlist playlist{};
        playlist.path = folder.path().string();
        playlist.name = getPlaylistName(folder);
        playlist.desc = getPlaylistDesc(folder);
        playlist.url = getPlaylistUrl(folder);
        if(std::find(state.playlists.begin(), state.playlists.end(), playlist) == state.playlists.end()) {
            state.playlists.emplace_back(playlist);
        }
    }
}

void loadPlaylistFileAsync(std::vector<SoundFile>* files, std::string path) {
    std::lock_guard<std::mutex> lock(state.mutex);
    SoundFile file{};
    if(std::filesystem::exists(path)) {
        file.path = strToWstr(path);
        file.pathStr = path;
        file.thumbnail = (LfTexture){0};
        file.duration = getSoundDuration(path);
    } else {
        file.path = L"File cannot be loaded";
        file.pathStr = path;
        file.thumbnail = (LfTexture){0};
        file.duration = 0; 
    }
    files->emplace_back(file);
    if(std::filesystem::exists(path))
        state.playlistFileThumbnailData.emplace_back(getSoundThubmnailData(path, (vec2s){60, 40}));
    else 
        state.playlistFileThumbnailData.emplace_back((TextureData){0});
}

void addFileToPlaylistAsync(std::vector<SoundFile>* files, std::string path, uint32_t playlistIndex) {
    std::lock_guard<std::mutex> lock(state.mutex);
    Playlist& playlist = state.playlists[playlistIndex];

    std::ofstream metadata(playlist.path + "/.metadata", std::ios::app);

    if(!metadata.is_open()) return;

    std::ifstream playlistFile(path);
    if(!playlistFile.good()) return;

    metadata << std::string("\"" + path + "\" ");
    metadata.close();

    SoundFile file{};
    file.path = strToWstr(path);
    file.pathStr = path;
    file.thumbnail = (LfTexture){0};
    file.duration = getSoundDuration(path);
    files->emplace_back(file);
    state.loadedPlaylistFilepaths.emplace_back(path);
    state.playlistFileThumbnailData.emplace_back(getSoundThubmnailData(path, (vec2s){60, 40}));
}

std::vector<std::wstring> loadFilesFromFolder(const std::filesystem::path& folderPath) {
    std::vector<std::wstring> files;
    for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        if (std::filesystem::is_directory(entry.path()) && entry.path().filename().string()[0] != '.') {
            files = loadFilesFromFolder(entry.path());
        } else if (std::filesystem::is_regular_file(entry.path())) {
            files.emplace_back(entry.path().wstring());
        }
    } 
    return files;
}

void playlistPlayFileWithIndex(uint32_t i, uint32_t playlistIndex) {
    if(!state.playlistFileFutures.empty()) return;
    Playlist& playlist = state.playlists[playlistIndex];
    playlist.playingFile = i;

    if(state.soundHandler.isPlaying)
        state.soundHandler.stop();

    if(state.soundHandler.isInit)
        state.soundHandler.uninit();

    state.soundHandler.init(playlist.musicFiles[i].pathStr, miniaudioDataCallback);
    state.soundHandler.play();

    state.currentSoundPos = 0.0;
    state.trackProgressSlider._init = false;
    state.trackProgressSlider.max = state.soundHandler.lengthInSeconds;
    state.playingPlaylist = playlistIndex;
}

void skipSoundUp(uint32_t playlistInedx) {
    Playlist& playlist = state.playlists[playlistInedx];

    if(playlist.playingFile + 1 < playlist.musicFiles.size())
        playlist.playingFile++;
    else 
        playlist.playingFile = 0;

    state.currentSoundFile = &playlist.musicFiles[playlist.playingFile];
    if(state.onTrackTab.trackThumbnail.width != 0) {
        lf_free_texture(state.onTrackTab.trackThumbnail);
    }
    state.onTrackTab.trackThumbnail = getSoundThubmnail(state.currentSoundFile->pathStr);

    playlistPlayFileWithIndex(playlist.playingFile, playlistInedx);

}

void skipSoundDown(uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];

    if(playlist.playingFile - 1 >= 0)
        playlist.playingFile--;
    else 
        playlist.playingFile = playlist.musicFiles.size() - 1; 

    state.currentSoundFile = &playlist.musicFiles[playlist.playingFile];
    if(state.onTrackTab.trackThumbnail.width != 0)
        lf_free_texture(state.onTrackTab.trackThumbnail);
    state.onTrackTab.trackThumbnail = getSoundThubmnail(state.currentSoundFile->pathStr);

    playlistPlayFileWithIndex(playlist.playingFile, playlistIndex);
}
void updateSoundProgress() {
    if(!state.soundHandler.isInit) {
        return;
    }

    if(state.currentSoundPos + 1 <= state.soundHandler.lengthInSeconds && state.soundHandler.isPlaying) {
        state.soundPosUpdateTime += state.deltaTime;
        if(state.soundPosUpdateTime >= state.soundPosUpdateTimer) {
            state.soundPosUpdateTime = 0.0f;
            state.currentSoundPos++;
            state.trackProgressSlider._init = false;
        }
    }

    if(state.currentSoundPos >= (uint32_t)state.soundHandler.lengthInSeconds && !state.trackProgressSlider.held) {
        skipSoundUp(state.currentPlaylist);
    }
}
std::string removeFileExtension(const std::string& filename) {
    // Find the last dot (.) in the filename
    size_t lastDotIndex = filename.rfind('.');

    if (lastDotIndex != std::string::npos && lastDotIndex > 0) {
        return filename.substr(0, lastDotIndex);
    } else {
        return filename;
    }
}

std::wstring removeFileExtensionW(const std::wstring& filename) {
    // Find the last dot (.) in the filename
    size_t lastDotIndex = filename.rfind('.');

    if (lastDotIndex != std::wstring::npos && lastDotIndex > 0) {
        return filename.substr(0, lastDotIndex);
    } else {
        return filename;
    }
}

void loadIcons() {
    for (const auto& entry : std::filesystem::directory_iterator("../assets/textures/")) {
        if (entry.path().extension() == ".png") {
            state.icons[entry.path().stem()] = lf_load_texture(entry.path().c_str(), true, LF_TEX_FILTER_LINEAR); 
        }
    }
}

std::string wStrToStr(const std::wstring& wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
}

std::wstring strToWstr(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}
bool playlistFileOrderCorrect(uint32_t playlistIndex, const std::vector<std::string>& paths) {
    const std::vector<SoundFile>& soundFiles = state.playlists[playlistIndex].musicFiles;

    if (soundFiles.size() != paths.size()) {
        return false;
    }

    for (size_t i = 0; i < soundFiles.size(); ++i) {
        if (soundFiles[i].pathStr != paths[i]) {
            return false;
        }
    }

    return true;
}

std::vector<SoundFile> reorderPlaylistFiles(const std::vector<SoundFile>& soundFiles, const std::vector<std::string>& paths) {
    std::vector<SoundFile> reorderedVector;
    reorderedVector.reserve(soundFiles.size());

    for (const auto& path : paths) {
        for (const auto& file : soundFiles) {
            if (file.pathStr == path) {
                reorderedVector.emplace_back(file);
                break;
            }
        }
    }

    return reorderedVector;
}

void handleAsyncPlaylistLoading() {
    // Create OpenGL Textures for the thumbnails that were loaded
    for (uint32_t i = 0; i < state.playlistFileThumbnailData.size(); i++) {
        SoundFile& file = state.playlists[state.currentPlaylist].musicFiles[i];
        LfTexture& thumbnail = file.thumbnail;
        if(file.loaded) continue;

        TextureData data = state.playlistFileThumbnailData[i];

        lf_create_texture_from_image_data(LF_TEX_FILTER_LINEAR, &thumbnail.id, data.width, data.height, data.channels, data.data);

        thumbnail.width = data.width;
        thumbnail.height = data.height;
        file.loaded = true;
    }
    if(state.currentPlaylist != -1) {
        Playlist& currentPlaylist = state.playlists[state.currentPlaylist];
        if(state.loadedPlaylistFilepaths.size() == currentPlaylist.musicFiles.size() && !state.playlistFileFutures.empty()) {
            state.playlistFileFutures.clear();
            state.playlistFileFutures.shrink_to_fit();
            currentPlaylist.ordered = playlistFileOrderCorrect(state.currentPlaylist, state.loadedPlaylistFilepaths);
        }
    }
}

void loadPlaylistAsync(Playlist& playlist) {
    state.playlistFileThumbnailData.clear();
    state.playlistFileThumbnailData.shrink_to_fit();

    for(auto& path : state.loadedPlaylistFilepaths) {
        if(!isFileInPlaylist(path, state.currentPlaylist)){ 
            if(ASYNC_PLAYLIST_LOADING) {
                state.playlistFileFutures.emplace_back(std::async(std::launch::async, loadPlaylistFileAsync, &state.playlists[state.currentPlaylist].musicFiles, path));
            } else {
                SoundFile file;
                if(std::filesystem::exists(std::filesystem::path(path))) {
                    file = (SoundFile){
                        .path = strToWstr(path),
                            .pathStr = path,
                            .duration = static_cast<int32_t>(getSoundDuration(path)),
                            .thumbnail = getSoundThubmnail(path, (vec2s){0.075f, 0.075f})
                    };

                } else {
                    file = (SoundFile){
                        .path = L"File cannot be loaded",
                            .pathStr = path,
                            .duration = 0, 
                            .thumbnail = (LfTexture){0}
                    };
                }
                playlist.musicFiles.emplace_back(file);
            }
        }
    }
}

double getSoundDuration(const std::string& soundPath) {
    SoundHandler sound; 
    sound.init(soundPath, miniaudioDataCallback);
    double duration = sound.lengthInSeconds;
    sound.uninit();
    return duration;
}

std::string formatDurationToMins(int32_t duration) {
    int32_t minutes = duration / 60;
    int32_t seconds = duration % 60;
    std::stringstream format;
    format  << std::setw(2) << std::setfill('0') << minutes << ":" 
        << std::setw(2) << std::setfill('0') << seconds;
    return format.str();
}

int main(int argc, char* argv[]) {
    // Initialization 
    initWin(WIN_START_W, WIN_START_H); 
    initUI();


    if(!std::filesystem::exists(LYSSA_DIR)) { 
        std::filesystem::create_directory(LYSSA_DIR);
    }
    loadPlaylists();

    // Creating the popups
    state.popups.reserve((int32_t)PopupID::PopupCount);
    state.aOrBPopup.render = false;
    state.popups[(int32_t)PopupID::EditPlaylistPopup] = (Popup){.renderCb = renderEditPlaylistPopup, .render = false};
    state.popups[(int32_t)PopupID::PlaylistFileDialoguePopup] = (Popup){.renderCb = renderPlaylistFileDialoguePopup, .render = false};

    vec4s clearColor = lf_color_to_zto(LYSSA_BACKGROUND_COLOR); 

    while(!state.win->shouldClose()) { 
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        if(ASYNC_PLAYLIST_LOADING)
            handleAsyncPlaylistLoading();

        // Updating the timestamp of the currently playing sound
        if(!lf_input_grabbed())
            updateSoundProgress();
        handleTabKeyStrokes();

        // Delta-Time calculation
        float currentTime = glfwGetTime();
        state.deltaTime = currentTime - state.lastTime;
        state.lastTime = currentTime;

        // OpenGL color clearing 
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);

        lf_begin();
        lf_div_begin(((vec2s){DIV_START_X, DIV_START_Y}), ((vec2s){(float)state.win->getWidth() - DIV_START_X, (float)state.win->getHeight() - DIV_START_Y}), false);

        switch(state.currentTab) {
            case GuiTab::Dashboard:
                renderDashboard();
                break;
            case GuiTab::CreatePlaylist:
                renderCreatePlaylist();
                break;
            case GuiTab::CreatePlaylistFromFolder:
                renderCreatePlaylistFromFolder();
                break;
            case GuiTab::DownloadPlaylist: 
                renderDownloadPlaylist();
                break;
            case GuiTab::OnPlaylist:
                renderOnPlaylist();
                break;
            case GuiTab::OnTrack:
                renderOnTrack();
                break;
            case GuiTab::PlaylistAddFromFile:
                renderPlaylistAddFromFile();
                break;
            case GuiTab::PlaylistAddFromFolder:
                renderPlaylistAddFromFolder();
                break;
            default:
                lf_text("Page not found");
                break;
        }
        for(uint32_t i = 0; i < (uint32_t)PopupID::PopupCount; i++) {
            auto& popup = state.popups[i];
            if(popup.render) {
                popup.renderCb();
            } 
        }
        renderAorBPopup(state.aOrBPopup);

        lf_div_end();
        lf_end();

        glfwPollEvents();
        state.win->swapBuffers();
    }
    if(state.playlistDownloadRunning) {
        system("pkill yt-dlp");
    }
    return 0;
} 
