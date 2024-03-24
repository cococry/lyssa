#include "config.hpp"
#include "leif.h"
#include "log.hpp"
#include "playlists.hpp"
#include "popups.hpp"
#include "soundHandler.hpp"
#include "soundTagParser.hpp"
#include "window.hpp"
#include "utils.hpp"
#include "global.hpp"

#include <cglm/types-struct.h>
#include <cstddef>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
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

static void                     renderTrackDisplay();

static void                     renderTrackVolumeControl();
static void                     renderTrackProgress();
static void                     renderTrackMenu();

static void                     backButtonTo(GuiTab tab, const std::function<void()>& clickCb = nullptr);
static void                     changeTabTo(GuiTab tab);

static void                     loadPlaylists();
static void                     loadPlaylistFileAsync(std::vector<SoundFile>* files, std::string path);
static void                     addFileToPlaylistAsync(std::vector<SoundFile>* files, std::string path, uint32_t playlistIndex);

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
}

void initUI() {
    state.h1Font = lf_load_font(std::string(LYSSA_DIR + "/assets/fonts/inter-bold.ttf").c_str(), 48);
    state.h2Font = lf_load_font(std::string(LYSSA_DIR + "/assets/fonts/inter-bold.ttf").c_str(), 40);
    state.h3Font = lf_load_font(std::string(LYSSA_DIR + "/assets/fonts/inter-bold.ttf").c_str(), 36);
    state.h4Font = lf_load_font(std::string(LYSSA_DIR + "/assets/fonts/inter.ttf").c_str(), 30);
    state.h5Font = lf_load_font(std::string(LYSSA_DIR + "/assets/fonts/inter.ttf").c_str(), 24);
    state.h6Font = lf_load_font(std::string(LYSSA_DIR + "/assets/fonts/inter.ttf").c_str(), 20);
    state.h7Font = lf_load_font(std::string(LYSSA_DIR + "/assets/fonts/inter.ttf").c_str(), 18);
    state.musicTitleFont = lf_load_font_ex(std::string(LYSSA_DIR + "/assets/fonts/inter-bold.ttf").c_str(), 72, 3072, 3072, 1536); 

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
                state.popups[PopupType::TwoChoicePopup] = std::make_unique<TwoChoicePopup>(
                        400, 
                        "How do you want to add a Playlist?", 
                        "Create New", 
                        "From Folder", 
                        [&](){
                        changeTabTo(GuiTab::CreatePlaylist);
                        state.popups[PopupType::TwoChoicePopup]->shouldRender = false;
                        lf_div_ungrab();
                        }, 
                        [&](){
                        if(state.playlistAddFromFolderTab.currentFolderPath.empty()) {
                        state.playlistAddFromFolderTab.currentFolderPath = strToWstr(std::string(getenv(HOMEDIR)));
                        state.playlistAddFromFolderTab.folderContents = loadFolderContents(state.playlistAddFromFolderTab.currentFolderPath);
                        }
                        changeTabTo(GuiTab::CreatePlaylistFromFolder);
                        state.popups[PopupType::TwoChoicePopup]->shouldRender = false;
                        lf_div_ungrab();
                        });
                state.popups[PopupType::TwoChoicePopup]->shouldRender = !state.popups[PopupType::TwoChoicePopup]->shouldRender;
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
            if(lf_button_fixed("Add Playlist", width, 50) == LF_CLICKED)  {
                state.popups[PopupType::TwoChoicePopup] = std::make_unique<TwoChoicePopup>(
                        400, 
                        "How do you want to add a Playlist?", 
                        "Create New", 
                        "From Folder", 
                        [&](){
                        changeTabTo(GuiTab::CreatePlaylist);
                        state.popups[PopupType::TwoChoicePopup]->shouldRender = false;
                        lf_div_ungrab();
                        }, 
                        [&](){
                        if(state.playlistAddFromFolderTab.currentFolderPath.empty()) {
                        state.playlistAddFromFolderTab.currentFolderPath = strToWstr(std::string(getenv(HOMEDIR)));
                        state.playlistAddFromFolderTab.folderContents = loadFolderContents(state.playlistAddFromFolderTab.currentFolderPath);
                        }
                        changeTabTo(GuiTab::CreatePlaylistFromFolder);
                        state.popups[PopupType::TwoChoicePopup]->shouldRender = false;
                        lf_div_ungrab();
                        });
                state.popups[PopupType::TwoChoicePopup]->shouldRender = !state.popups[PopupType::TwoChoicePopup]->shouldRender;
            }
            if(lf_button_fixed("Download Playlist", width, 50) == LF_CLICKED)  {
                changeTabTo(GuiTab::DownloadPlaylist);
            }
            lf_pop_style_props();
        }
    } else {
        // Constants

        int32_t playlistIndex = 0;
        for(auto& playlist : state.playlists) {
            const float paddingTop = 50;
            const float width = 180;
            LfTextProps nameProps = lf_text_render((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + paddingTop}, playlist.name.c_str(), 
                    lf_get_theme().font, LF_NO_COLOR, lf_get_ptr_x() + width, (vec2s){-1, -1}, true, 
                    false, -1, -1);
            LfTextProps descProps = lf_text_render((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + paddingTop + nameProps.height}, playlist.desc.c_str(), 
                    lf_get_theme().font, LF_NO_COLOR, lf_get_ptr_x() + width, (vec2s){-1, -1}, true, 
                    false, -1, -1);
            float height = (width - 25) + (10) + nameProps.height + (10) + descProps.height + 60;
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
            vec2s buttonSize = (vec2s){
                MAX((state.win->getWidth() / 100.0f) * 2.0f, 20), MAX((state.win->getWidth() / 100.0f) * 2.0f, 20)};
            if(overDiv)
            {
                LfUIElementProps props = lf_get_theme().button_props;
                props.color = LF_NO_COLOR;
                props.border_color = LF_NO_COLOR;
                props.border_width = 0;
                props.padding = 0;
                props.margin_top = 0;
                props.margin_bottom = 0;
                lf_push_style_props(props);

                float margin = 5;
                float ptr_y = lf_get_ptr_y();

                lf_set_ptr_y(height - buttonSize.y);
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
                    Playlist::remove(playlistIndex);
                }

                LfClickableItemState renameButton = lf_image_button(((LfTexture){.id = state.icons["edit"].id, 
                            .width = (uint32_t)buttonSize.x, .height = (uint32_t)buttonSize.y})); 

                if(renameButton == LF_CLICKED) {
                    state.currentPlaylist = playlistIndex;
                    state.popups[PopupType::EditPlaylistPopup] = std::make_unique<EditPlaylistPopup>();
                    state.popups[PopupType::EditPlaylistPopup]->shouldRender = true;
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

                    state.loadedPlaylistFilepaths = PlaylistMetadata::getFilepaths(std::filesystem::directory_entry(playlist.path));
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
            state.createPlaylistTab.createFileStatus = Playlist::create(std::string(state.createPlaylistTab.nameInput.buffer), std::string(state.createPlaylistTab.descInput.buffer)); 
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
                std::ofstream metadata(state.playlists[playlist].path.string() + "/.metadata", std::ios::app);
                metadata.seekp(0, std::ios::end);
                for(const auto& entry : std::filesystem::directory_iterator(tab.currentFolderPath)) {
                if(!entry.is_directory() && SoundTagParser::isValidSoundFile(entry.path().string())) {
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

                    state.loadedPlaylistFilepaths = PlaylistMetadata::getFilepaths(std::filesystem::directory_entry(playlist.path));
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
            lf_text("Download a playlist from a streaming service");
            lf_pop_style_props();
            lf_pop_font();
        }

        lf_next_line();
        static char urlInput[INPUT_BUFFER_SIZE] = {0};

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
            FileStatus createStatus = Playlist::create(state.downloadingPlaylistName, "Downloaded Playlist", url);

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
            Playlist::save(state.currentPlaylist);
            clearedPlaylist = true;
        }

        if(state.downloadPlaylistFileCount == LyssaUtils::getLineCountFile(LYSSA_DIR + "/downloaded_playlists/" + state.downloadingPlaylistName + "/archive.txt") && state.downloadPlaylistFileCount != 0) {
            state.playlistDownloadRunning = false;
            for (const auto& entry : std::filesystem::directory_iterator(LYSSA_DIR + "/downloaded_playlists/" + state.downloadingPlaylistName)) {
                if (entry.is_regular_file() && 
                        !Playlist::containsFile(entry.path().string(), state.currentPlaylist) && 
                        SoundTagParser::isValidSoundFile(entry.path().string()) && entry.path().extension() == ".mp3") {
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
            props.margin_bottom = 20;
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
                state.popups[PopupType::TwoChoicePopup] = std::make_unique<TwoChoicePopup>(
                        400, 
                        "How do you want to add Music?", 
                        "From File", 
                        "From Folder", 
                        [&](){
                            changeTabTo(GuiTab::PlaylistAddFromFile);
                            state.popups[PopupType::TwoChoicePopup]->shouldRender = false;
                            lf_div_ungrab();
                        }, 
                        [&](){
                            if(state.playlistAddFromFolderTab.currentFolderPath.empty()) {
                                state.playlistAddFromFolderTab.currentFolderPath = strToWstr(std::string(getenv(HOMEDIR)));
                                state.playlistAddFromFolderTab.folderContents = loadFolderContents(state.playlistAddFromFolderTab.currentFolderPath);
                            }
                            changeTabTo(GuiTab::PlaylistAddFromFolder);
                            state.popups[PopupType::TwoChoicePopup]->shouldRender = false;
                            lf_div_ungrab();
                        }); 
                state.popups[PopupType::TwoChoicePopup]->shouldRender = !state.popups[PopupType::TwoChoicePopup]->shouldRender;
            }  
            lf_pop_style_props();
        }

        if(showPlaylistSettings) {
            lf_next_line();
            LfUIElementProps props = secondary_button_style();
            props.margin_top = -10;
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

        lf_next_line();
        {
            std::string downloadedPlaylistDir = LYSSA_DIR + "/downloaded_playlists/" + state.downloadingPlaylistName; 
            uint32_t downloadedFileCount = LyssaUtils::getLineCountFile(downloadedPlaylistDir + "/archive.txt");
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


        /* Heading */
        {
            LfUIElementProps props = lf_get_theme().text_props;
            props.margin_bottom = 20;
            props.margin_right = 30;
            props.text_color = lf_color_brightness(GRAY, 1.6);
            lf_push_style_props(props);
            lf_text("#");

            props.margin_left = 0;
            props.margin_right = 0;
            lf_push_style_props(props);
            lf_text("Title");

            
            lf_set_ptr_x_absolute(state.win->getWidth() / 1.5f + 100);
            lf_text("Year");

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
                    .pos = (vec2s){lf_get_ptr_x(), lf_get_ptr_y()},
                    .size = (vec2s){(float)state.win->getWidth() - DIV_START_X * 2, (float)thumbnailContainerSize.y + marginBottomThumbnail}
                };

                bool hoveredTextDiv = lf_hovered(fileAABB.pos, fileAABB.size);
                if(hoveredTextDiv && lf_mouse_button_is_released(GLFW_MOUSE_BUTTON_RIGHT)) {
                    state.popups[PopupType::PlaylistFileDialoguePopup] = std::make_unique<PlaylistFileDialoguePopup>(file.path, (vec2s){(float)lf_get_mouse_x(), (float)lf_get_mouse_y()});
                    state.popups[PopupType::PlaylistFileDialoguePopup]->shouldRender = true;
                }
                if(currentPlaylist.playingFile == i) {
                    lf_rect_render(fileAABB.pos, fileAABB.size, lf_color_brightness(GRAY, 0.75),
                            LF_NO_COLOR, 0.0f, 3.0f);
                } 

                // Index 
                {
                    std::stringstream indexSS;
                    indexSS << i;
                    std::string indexStr = indexSS.str();
                    vec2s indexPos = (vec2s){lf_get_ptr_x() + 10, lf_get_ptr_y() + (thumbnailContainerSize.y - lf_text_dimension(indexStr.c_str()).y) / 2.0f};
                    if(hoveredTextDiv) {
                        bool hoveredPlayButton = lf_hovered((vec2s){indexPos.x - 5, indexPos.y}, 
                                (vec2s){(float)lf_get_theme().font.font_size, (float)lf_get_theme().font.font_size}); 
                        onPlayButton = hoveredPlayButton;
                        if(hoveredTextDiv && lf_mouse_button_is_released(GLFW_MOUSE_BUTTON_LEFT) && onPlayButton && state.playlistFileFutures.empty()) {
                            state.currentSoundFile = &file;
                            if(state.onTrackTab.trackThumbnail.width != 0) {
                                lf_free_texture(state.onTrackTab.trackThumbnail);
                            }
                            state.onTrackTab.trackThumbnail = SoundTagParser::getSoundThubmnail(state.currentSoundFile->path);

                            if(i != currentPlaylist.playingFile)
                                playlistPlayFileWithIndex(i, state.currentPlaylist);
                            changeTabTo(GuiTab::OnTrack);
                        }
                        lf_image_render((vec2s){indexPos.x - 5, indexPos.y}, LF_WHITE, 
                                (LfTexture){.id = (i == currentPlaylist.playingFile) ? state.icons["pause"].id : state.icons["play"].id, .width = lf_get_theme().font.font_size, .height = lf_get_theme().font.font_size}, 
                                LF_NO_COLOR, 0.0f, 0.0f);
                    } else {
                        lf_text_render(indexPos, 
                                indexStr.c_str(), lf_get_theme().font, LF_WHITE, -1, (vec2s){-1, -1}, false, false, -1, -1);
                    }
                    lf_set_ptr_x_absolute(lf_get_ptr_x() + 50);
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


                    lf_set_cull_end_x((state.win->getWidth() / 1.5f + 100) - 50);

                    lf_text_render_wchar((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + marginTopThumbnail}, filename.c_str(), state.h5Font, -1, false, 
                            hoveredTextDiv ? lf_color_brightness(LF_WHITE, 0.7) : LF_WHITE);
                    lf_text_render_wchar((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + marginTopThumbnail + state.h5Font.font_size}, file.artist.c_str(), state.h5Font, -1, false, 
                            lf_color_brightness(GRAY, 1.4));

                    lf_unset_cull_end_x();
                } 
                {
                    lf_set_ptr_x_absolute(state.win->getWidth() / 1.5f + 100);
                    LfUIElementProps props = lf_get_theme().text_props;
                    props.text_color = lf_color_brightness(GRAY, 1.6);
                    props.margin_left = 0;
                    props.margin_right = 0;
                    props.margin_top = (thumbnailContainerSize.y - lf_text_dimension(std::to_string(file.releaseYear).c_str()).y) / 2.0f;
                    lf_push_style_props(props);
                    if(file.releaseYear != 0)
                        lf_text(std::to_string(file.releaseYear).c_str());
                    else 
                        lf_text("-");
                    lf_pop_style_props();
                }

                if(lf_mouse_button_is_released(GLFW_MOUSE_BUTTON_LEFT) && hoveredTextDiv && !onPlayButton && state.playlistFileFutures.empty()) {
                    playlistPlayFileWithIndex(i, state.currentPlaylist);
                    state.currentSoundFile = &file;
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

        LfTexture thumbnail = (tab.trackThumbnail.width == 0) ? state.icons["music_note"] : tab.trackThumbnail;

        lf_image_render((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + (thumbnailContainerSize.y - thumbnailHeight) / 2.0f}, 
                LF_WHITE, (LfTexture){.id = thumbnail.id, .width = (uint32_t)thumbnailContainerSize.x, .height = (uint32_t)thumbnailHeight}, LF_NO_COLOR, 0.0f, 0.0f);   

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
            lf_set_image_color(LF_BLACK);
            if(lf_image_button(((LfTexture){.id = state.soundHandler.isPlaying ? state.icons["pause"].id : state.icons["play"].id, .width =(uint32_t)iconSizeSm, .height =(uint32_t)iconSizeSm})) == LF_CLICKED) {
                if(state.soundHandler.isPlaying)
                    state.soundHandler.stop();
                else 
                    state.soundHandler.play();
            }
            lf_unset_image_color();
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
            state.playlistAddFromFileTab.addFileStatus = Playlist::addFile(state.playlistAddFromFileTab.pathInput.input.buf, state.currentPlaylist);
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
        std::ofstream metadata(currentPlaylist.path.string() + "/.metadata", std::ios::app);
        metadata.seekp(0, std::ios::end);
        for(const auto& entry : tab.folderContents) {
            if(!entry.is_directory() && 
                    !Playlist::metadataContainsFile(entry.path().string(), state.currentPlaylist) && 
                    SoundTagParser::isValidSoundFile(entry.path().string())) {
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
                lf_set_image_color((Playlist::metadataContainsFile(entry.path().string(), state.currentPlaylist) && !entry.is_directory()) ? LYSSA_GREEN : LF_WHITE);
                lf_push_style_props(props);
                const vec2s iconSize = (vec2s){25, 25};
                LfTexture icon = (LfTexture){
                    .id = (entry.is_directory()) ? state.icons["folder"].id : state.icons["file"].id, 
                        .width = (uint32_t)iconSize.x, 
                        .height = (uint32_t)iconSize.y
                };
                if(lf_image_button(icon) == LF_CLICKED && !entry.is_directory() && 
                        !Playlist::metadataContainsFile(entry.path().string(), state.currentPlaylist) && 
                        SoundTagParser::isValidSoundFile(entry.path().string())) {
                    std::ofstream metadata(state.playlists[state.currentPlaylist].path.string() + "/.metadata", std::ios::app);
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

void renderTrackDisplay() {
    if(state.currentSoundFile == NULL) return;
    const float margin = DIV_START_X;
    const float marginThumbnail = 15;
    const vec2s thumbnailContainerSize = (vec2s){48, 48};
    const float padding = 10;

    std::filesystem::path filepath = state.currentSoundFile->path;

    std::wstring filename = removeFileExtensionW(filepath.filename().wstring());
    std::wstring artist = state.currentSoundFile->artist;

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
        LfTexture thumbnail = (playlingFile.thumbnail.width == 0) ? state.icons["music_note"] : playlingFile.thumbnail;
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
                        .id = thumbnail.id, 
                        .width = (uint32_t)thumbnailContainerSize.x, 
                        .height = (uint32_t)thumbnailHeight})) == LF_CLICKED) { 
            if(state.onTrackTab.trackThumbnail.width != 0 && state.currentSoundFile) 
                lf_free_texture(state.onTrackTab.trackThumbnail);
            state.onTrackTab.trackThumbnail = SoundTagParser::getSoundThubmnail(state.currentSoundFile->path.string());
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
            lf_set_image_color(LF_BLACK);
            if(lf_image_button(((LfTexture){.id = state.soundHandler.isPlaying ? state.icons["pause"].id : state.icons["play"].id, .width = (uint32_t)iconSize.x, .height = (uint32_t)iconSize.y})) == LF_CLICKED) {
                if(state.soundHandler.isPlaying)
                    state.soundHandler.stop();
                else 
                    state.soundHandler.play();
            } 
            lf_unset_image_color();
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

void loadPlaylists() {
    uint32_t playlistI = 0;
    for (const auto& folder : std::filesystem::directory_iterator(LYSSA_DIR + "/playlists/")) {
        Playlist playlist{};
        playlist.path = folder.path().string();
        playlist.name = PlaylistMetadata::getName(folder);
        playlist.desc = PlaylistMetadata::getDesc(folder);
        playlist.url = PlaylistMetadata::getUrl(folder);
        if(std::find(state.playlists.begin(), state.playlists.end(), playlist) == state.playlists.end()) {
            state.playlists.emplace_back(playlist);
        }
    }
}

void loadPlaylistFileAsync(std::vector<SoundFile>* files, std::string path) {
    std::lock_guard<std::mutex> lock(state.mutex);
    SoundFile file{};
    if(std::filesystem::exists(path)) {
        file.path = std::filesystem::path(path); 
        file.thumbnail = (LfTexture){0};
        file.duration = SoundHandler::getSoundDuration(path);
        file.artist = SoundTagParser::getSoundArtist(path);
        file.releaseYear = SoundTagParser::getSoundReleaseYear(path);
    } else {
        file.path = L"File cannot be loaded";
        file.thumbnail = (LfTexture){0};
        file.duration = 0; 
    }
    files->emplace_back(file);
    if(std::filesystem::exists(path))
        state.playlistFileThumbnailData.emplace_back(SoundTagParser::getSoundThubmnailData(path, (vec2s){100, 50}));
    else 
        state.playlistFileThumbnailData.emplace_back((TextureData){0});
}

void addFileToPlaylistAsync(std::vector<SoundFile>* files, std::string path, uint32_t playlistIndex) {
    std::lock_guard<std::mutex> lock(state.mutex);
    Playlist& playlist = state.playlists[playlistIndex];

    std::ofstream metadata(playlist.path.string() + "/.metadata", std::ios::app);

    if(!metadata.is_open()) return;

    std::ifstream playlistFile(path);
    if(!playlistFile.good()) return;

    metadata << std::string("\"" + path + "\" ");
    metadata.close();

    SoundFile file{};
    file.path = strToWstr(path);
    file.path = path;
    file.thumbnail = (LfTexture){0};
    file.duration = SoundHandler::getSoundDuration(path);
    file.artist = SoundTagParser::getSoundArtist(path);
    file.releaseYear = SoundTagParser::getSoundReleaseYear(path);
    files->emplace_back(file);
    state.loadedPlaylistFilepaths.emplace_back(path);
    state.playlistFileThumbnailData.emplace_back(SoundTagParser::getSoundThubmnailData(path, (vec2s){60, 40}));
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

    state.soundHandler.init(playlist.musicFiles[i].path.string(), miniaudioDataCallback);
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
    state.onTrackTab.trackThumbnail = SoundTagParser::getSoundThubmnail(state.currentSoundFile->path);

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
    state.onTrackTab.trackThumbnail = SoundTagParser::getSoundThubmnail(state.currentSoundFile->path);

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
    for (const auto& entry : std::filesystem::directory_iterator(LYSSA_DIR + "/assets/textures/")) {
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
        if (soundFiles[i].path != paths[i]) {
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
            if (file.path == path) {
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
        if(!Playlist::containsFile(path, state.currentPlaylist)){ 
            if(ASYNC_PLAYLIST_LOADING) {
                state.playlistFileFutures.emplace_back(std::async(std::launch::async, loadPlaylistFileAsync, &state.playlists[state.currentPlaylist].musicFiles, path));
            } else {
                SoundFile file;
                if(std::filesystem::exists(std::filesystem::path(path))) {
                    file = (SoundFile){
                            .path = path,
                            .artist = SoundTagParser::getSoundArtist(path),
                            .releaseYear = SoundTagParser::getSoundReleaseYear(path),
                            .duration = static_cast<int32_t>(SoundHandler::getSoundDuration(path)),
                            .thumbnail = SoundTagParser::getSoundThubmnail(path, (vec2s){0.1, 0.1}),
                    };

                } else {
                    file = (SoundFile){
                        .path = "File cannot be loaded",
                        .duration = 0, 
                        .thumbnail = (LfTexture){0}
                    };
                }
                playlist.musicFiles.emplace_back(file);
            }
        }
    }
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

    vec4s clearColor = lf_color_to_zto(LYSSA_BACKGROUND_COLOR); 

    while(!state.win->shouldClose()) { 
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
        if(ASYNC_PLAYLIST_LOADING)
            handleAsyncPlaylistLoading();

        // Updating the timestamp of the currently playing sound

        updateSoundProgress();

        // Delta-Time calculation
        float currentTime = glfwGetTime();
        state.deltaTime = currentTime - state.lastTime;
        state.lastTime = currentTime;

        // OpenGL color clearing 
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);

        lf_begin();
        lf_div_begin(((vec2s){DIV_START_X, DIV_START_Y}), ((vec2s){(float)state.win->getWidth() - DIV_START_X, (float)state.win->getHeight() - DIV_START_Y}), true);

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
        for(const auto& popup : state.popups) {
            if(popup.second->shouldRender) {
                popup.second->render();
            } 
        }
        if(!lf_input_grabbed())
            handleTabKeyStrokes();

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
