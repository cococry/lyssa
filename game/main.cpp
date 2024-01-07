#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <vector>
#include <cstdlib> 

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../leif.h"

#define MINIAUDIO_IMPLEMENTATION
#include "../vendor/miniaudio/miniaudio.h"

#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/mpegheader.h>
#include <taglib/attachedpictureframe.h>

#ifdef _WIN32
#define HOMEDIR "USERPROFILE"
#else
#define HOMEDIR "HOME"
#endif

#define LYSSA_DIR std::string(getenv(HOMEDIR)) + std::string("/.lyssa")


#define RGB_COLOR(r, g, b) (vec4s){LF_RGBA(r, g, b, 255.0f)}
#define RGBA_COLOR(r, g, b, a) (vec4s){LF_RGBA(r, g, b, a)}

#define LYSSA_GREEN RGB_COLOR(13, 181, 108)
#define LYSSA_BLUE  RGB_COLOR(83, 150, 237) 
#define LYSSA_RED  RGB_COLOR(150, 12, 14) 

#define DIV_START_X 20  
#define DIV_START_Y 20

#define BACK_BUTTON_MARGIN_BOTTOM 50
#define BACK_BUTTON_WIDTH 20 
#define BACK_BUTTON_HEIGHT 40

#define LF_PTR (vec2s){lf_get_ptr_x(), lf_get_ptr_y()}

using namespace TagLib;

enum class GuiTab {
    Dashboard = 0, 
    CreatePlaylist,
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

struct Sound {
    ma_device device;
    ma_decoder decoder;

    bool isPlaying = false, isInit = false;
    double lengthInSeconds = 0;

    uint32_t volume = 100;

    void init(const std::string& filepath);
    void uninit();
    void play();
    void stop();
    
    double getPositionInSeconds();
    void setPositionInSeconds(double position);

};

struct SoundFile {
    std::string path; 
    int32_t duration;
    LfTexture thumbnail;

    bool operator==(const SoundFile& other) const {
        return path == other.path;
    }
};

struct Playlist {
    std::vector<SoundFile> musicFiles;
    
    std::string name, path;
    int32_t playingFile = -1;
};

struct CreatePlaylistState {
    LfInputField nameInput;  

    FileStatus createFileStatus;
    float createFileMessageShowTime = 3.0f; 
    float createFileMessageTimer = 0.0f;
};

struct PlaylistAddFromFileTab {
    LfInputField pathInput;

    FileStatus addFileStatus;
    float addFileMessageShowTime = 3.0f; 
    float addFileMessageTimer = 0.0f;
};

struct OnTrackTab {
    LfTexture trackThumbnail;
    LfSlider trackProgress;
    LfSlider volumeSlider;
    bool showVolumeSlider;
};

struct Folder {
    std::string path;
    std::vector<std::string> files;
};


struct PlaylistAddFromFolderTab {
    LfInputField pathInput;

    int32_t folderIndex = -1;
    std::vector<Folder> loadedFolders;

    LfDiv* fileContainer = NULL;
};


typedef void (*PopupRenderCallback)();

struct Popup {
    PopupRenderCallback renderCb;
    bool render;
};

enum class PopupID {
    FileOrFolderPopup = 0,
    PopupCount
};

struct GlobalState {
    GLFWwindow* win;
    uint32_t winWidth, winHeight;
    float deltaTime, lastTime;

    ma_engine soundEngine;

    Sound currentSound;
    SoundFile* currentSoundFile = NULL;
    int32_t currentSoundPos;

    LfFont musicTitleFont;
    LfFont poppinsBold;
    LfFont h1Font;
    LfFont h2Font;
    LfFont h3Font;
    LfFont h4Font;
    LfFont h5Font;
    LfFont h6Font;

    GuiTab currentTab;

    std::vector<Playlist> playlists;
    std::vector<Popup> popups;

    LfTexture backTexture, musicNoteTexture, downTexture, addTexture, tickTexture, 
              playTexture, pauseTexture, skipUpTexture, skipDownTexture, skipSongUpTexture, 
              skipSongDownTexture, soundIcon;

    CreatePlaylistState createPlaylistTab;
    PlaylistAddFromFileTab playlistAddFromFileTab;
    PlaylistAddFromFolderTab playlistAddFromFolderTab;
    OnTrackTab onTrackTab; 

    int32_t playlistFileOptionsIndex = -1;
    int32_t currentPlaylist = -1;

    float soundPosUpdateTimer = 1.0f;
    float soundPosUpdateTime = 0.0f;

};

static GlobalState state;

static void                     winResizeCb(GLFWwindow* window, int32_t width, int32_t height);
static void                     initWin(uint32_t width, uint32_t height);
static void                     initUI();
static void                     handleTabKeyStrokes();

static void                     renderDashboard();
static void                     renderCreatePlaylist();
static void                     renderOnPlaylist();
static void                     renderOnTrack();
static void                     renderPlaylistAddFromFile();
static void                     renderPlaylistAddFromFolder();

static void                     renderFileOrFolderPopup();

static void                     backButtonTo(GuiTab tab);
static void                     changeTabTo(GuiTab tab);

static FileStatus               createPlaylist(const std::string& name);
static FileStatus               addFileToPlaylist(const std::string& path, uint32_t playlistIndex);
static FileStatus               removeFileFromPlaylist(const std::string& path, uint32_t playlistIndex);
static bool                     isFileInPlaylist(const std::string& path, uint32_t playlistIndex);
static void                     loadPlaylist(const std::filesystem::directory_entry& folder);
static void                     loadPlaylists();
static std::vector<std::string> loadFilesFromFolder(const std::filesystem::path& folderPath);
static void                     playlistPlayFileWithIndex(uint32_t i, uint32_t playlistIndex);
static void                     skipSoundUp();
static void                     skipSoundDown();

static double                   getSoundDuration(const std::string& soundPath);
static std::string              formatDurationToMins(int32_t duration);
static LfTexture                getSoundThubmnail(const std::string& soundPath);
static void                     updateSoundProgress();

static std::string              removeFileExtension(const std::string& filename);


template<typename T>
static bool elementInVector(const std::vector<T>& v, const T& e) {
    for(auto& element : v) 
        if(element == e) 
            return true;
    return false;
}

static void enterOnPlaylistCallback() {
    Playlist& playlist = state.playlists[state.currentPlaylist];
    loadPlaylist(std::filesystem::directory_entry(playlist.path));
}

static void miniaudioDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ma_decoder_read_pcm_frames(&state.currentSound.decoder, pOutput, frameCount, NULL);

    // Adjust volume
    float* outputBuffer = (float*)pOutput;
    for (ma_uint32 i = 0; i < frameCount * pDevice->playback.channels; ++i) {
        outputBuffer[i] *= state.currentSound.volume / 100.0f;
    }
}

void Sound::init(const std::string& filepath) {
    if (ma_decoder_init_file(filepath.c_str(), NULL, &decoder) != MA_SUCCESS) {
        std::cerr << "[Error]: Failed to load Sound '" << filepath << "'.\n";
        return;
    }

    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = decoder.outputFormat;
    deviceConfig.playback.channels = decoder.outputChannels;
    deviceConfig.sampleRate = decoder.outputSampleRate;
    deviceConfig.dataCallback = miniaudioDataCallback;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        return;
    }
    ma_uint64 lengthInFrames;
    ma_decoder_get_length_in_pcm_frames(&decoder, &lengthInFrames);
    lengthInSeconds = (double)lengthInFrames / decoder.outputSampleRate;

    isInit = true;
}
void Sound::uninit() {
    if(!this->isInit) return;
    ma_device_stop(&device);
    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);
    isInit = false;
}

void Sound::play() {
    if(this->isPlaying) return;
    ma_device_start(&device);
    isPlaying = true;
}

void Sound::stop() {
    if(!this->isPlaying) return;
    ma_device_stop(&device);
    isPlaying = false;
}

double Sound::getPositionInSeconds() {
    ma_uint64 cursorInFrames;
    ma_decoder_get_cursor_in_pcm_frames(&decoder, &cursorInFrames);
    return (double)cursorInFrames / decoder.outputSampleRate;
}

void Sound::setPositionInSeconds(double position) {
     ma_uint64 targetFrame = (ma_uint64)(position * decoder.outputSampleRate);

    // Stop the device before seeking
    if(isPlaying)
        ma_device_stop(&device);

    if(ma_decoder_seek_to_pcm_frame(&decoder, targetFrame) != MA_SUCCESS) {
        std::cerr << "Sound position in seconds invalid.\n";
    }
    
    if(isPlaying)
        ma_device_start(&device);
}

static void winResizeCb(GLFWwindow* window, int32_t width, int32_t height) {
    lf_resize_display(width, height);
    glViewport(0, 0, width, height);
    state.winWidth = width;
    state.winHeight = height;
}
void initWin(uint32_t width, uint32_t height) {
    state.winWidth = width;
    state.winHeight = height;
    if(!glfwInit()) {
        std::cerr << "[Error]: Failed to initialize GLFW.\n";
    }

    state.win = glfwCreateWindow(width, height, "Lyssa Music" ,NULL, NULL);
    if(!state.win) {
        std::cerr << "[Error]: Failed to create GLFW window.\n";
    }
    glfwMakeContextCurrent(state.win);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "[Error]: Failed to initialize Glad.\n";
    }

    LfTheme theme = lf_default_theme();
    theme.div_props.color = (vec4s){LF_RGBA(0, 0, 0, 0)};
    theme.scrollbar_props.corner_radius = 1.5;
    lf_init_glfw(width, height, &theme, state.win);   
    lf_set_text_wrap(true);

    glfwSetFramebufferSizeCallback(state.win, winResizeCb);
    glViewport(0, 0, width, height);

    state.currentSoundPos = 0.0;

    ma_result result = ma_engine_init(NULL, &state.soundEngine);
    if (result != MA_SUCCESS) {
        std::cerr << "[Error]: Failed to initialize miniaudio.\n";
    } 
}

void initUI() {
    state.h1Font = lf_load_font("../game/fonts/inter-bold.ttf", 48);
    state.h2Font = lf_load_font("../game/fonts/inter-bold.ttf", 40);
    state.h3Font = lf_load_font("../game/fonts/inter-bold.ttf", 36);
    state.h4Font = lf_load_font("../game/fonts/inter-bold.ttf", 30);
    state.h5Font = lf_load_font("../game/fonts/inter-bold.ttf", 24);
    state.h6Font = lf_load_font("../game/fonts/inter-bold.ttf", 20);
    state.musicTitleFont = lf_load_font("../game/fonts/poppins.ttf", 90);
    state.poppinsBold = lf_load_font("../game/fonts/poppins.ttf", 30);

    state.backTexture = lf_load_texture("../game/textures/back.png", false, LF_TEX_FILTER_LINEAR);
    state.musicNoteTexture = lf_load_texture("../game/textures/music1.png", false, LF_TEX_FILTER_LINEAR);
    state.downTexture = lf_load_texture("../game/textures/down.png", false, LF_TEX_FILTER_LINEAR);
    state.addTexture = lf_load_texture("../game/textures/add-symbol.png", false, LF_TEX_FILTER_LINEAR);
    state.tickTexture = lf_load_texture("../game/textures/tick.png", false, LF_TEX_FILTER_LINEAR);

    state.playTexture = lf_load_texture("../game/textures/play.png", false, LF_TEX_FILTER_LINEAR);
    state.pauseTexture = lf_load_texture("../game/textures/pause.png", false, LF_TEX_FILTER_LINEAR);
    state.skipDownTexture = lf_load_texture("../game/textures/skip_down.png", false, LF_TEX_FILTER_LINEAR);
    state.skipUpTexture = lf_load_texture("../game/textures/skip_up.png", false, LF_TEX_FILTER_LINEAR);
    state.skipSongDownTexture = lf_load_texture("../game/textures/skip_song_down.png", false, LF_TEX_FILTER_LINEAR);
    state.skipSongUpTexture = lf_load_texture("../game/textures/skip_song_up.png", false, LF_TEX_FILTER_LINEAR);
    state.soundIcon = lf_load_texture("../game/textures/sound.png", false, LF_TEX_FILTER_LINEAR);

    static char bufName[512] = {0};
    state.createPlaylistTab.nameInput = (LfInputField){
        .width = 600, 
        .buf = bufName, 
        .placeholder = (char*)"Name",
    };

    static char bufPathFile[512] = {0};
    state.playlistAddFromFileTab.pathInput = (LfInputField){
        .width = 600, 
        .buf = bufPathFile, 
        .placeholder = (char*)"Path",
    };

    static char bufPathFolder[512] = {0};
    state.playlistAddFromFolderTab.pathInput = (LfInputField){
        .width = 600, 
        .buf = bufPathFolder, 
        .placeholder = (char*)"Path",
    };

    state.onTrackTab.trackProgress = (LfSlider){
        .val = reinterpret_cast<int32_t*>(&state.currentSoundPos), 
        .min = 0, 
        .max = 0,
        .width = 400,
        .height = 5,
    };

    state.onTrackTab.volumeSlider = (LfSlider){
        .val = reinterpret_cast<int32_t*>(&state.currentSound.volume), 
        .min = 0, 
        .max = 100,
        .width = 300,
        .height = 5,
    };
}

void handleTabKeyStrokes() {
    if(state.currentSound.isInit) {
        if(lf_key_went_down(GLFW_KEY_SPACE)) {
            if(state.currentSound.isPlaying)
                state.currentSound.stop();
            else 
                state.currentSound.play();
        }
        if(lf_key_went_down(GLFW_KEY_N)) {
            if(lf_key_is_down(GLFW_KEY_LEFT_SHIFT)) {
                skipSoundDown();
            } else {
                skipSoundUp();
            }
        }
    }
}

void renderDashboard() {
    lf_div_begin((vec2s){DIV_START_X, DIV_START_Y}, (vec2s){(float)state.winWidth, (float)state.winHeight});

    lf_push_font(&state.h1Font);
    lf_text("Your Playlists");
    lf_pop_font();

    if(!state.playlists.empty()) {
        {
            const uint32_t width = 200;
            const uint32_t height = 50;
            const uint32_t marginRight = 40;
            LfUIElementProps props = lf_theme()->button_props;
            props.margin_right = 0;
            props.margin_left = 0;
            props.corner_radius = 12;
            props.border_width = 0;
            props.color = LYSSA_GREEN;
            lf_push_style_props(props);
            lf_push_font(&state.h1Font);
            lf_set_ptr_x(state.winWidth - width - marginRight * 2);
            lf_pop_font();
            if(lf_button_fixed("Add Playlist", width, height) == LF_CLICKED) { 
                changeTabTo(GuiTab::CreatePlaylist);
                loadPlaylists();
            }
            lf_pop_style_props();
        }
    }

    lf_next_line();
    if(state.playlists.empty()) {
        {
            const char* text = "You don't have any playlists.";
            float textWidth = lf_text_dimension(text).x;
            lf_set_ptr_x((state.winWidth - textWidth) / 2.0f - DIV_START_X);
            LfUIElementProps props = lf_theme()->text_props;
            props.margin_top = 40;
            props.margin_left = 0;
            props.margin_right = 0;
            lf_push_style_props(props);
            lf_text(text);
            lf_pop_style_props();
        }
        lf_next_line();
        {
            const uint32_t width = 200;
            lf_set_ptr_x((state.winWidth - (width + (lf_theme()->button_props.padding * 2))) / 2.0f - DIV_START_X);
            LfUIElementProps props = lf_theme()->button_props;
            props.color = LYSSA_GREEN;
            props.border_width = 0;
            props.corner_radius = 15;
            props.margin_top = 20;
            props.margin_left = 0;
            lf_push_style_props(props);
            if(lf_button_fixed("Create Playlist", width, 50) == LF_CLICKED)  {
                changeTabTo(GuiTab::CreatePlaylist);
            }
            lf_pop_style_props();
        }
    } else {
        /* Render the List of Playlists */ 

        // Constants
        const uint32_t width = 140;
        const uint32_t height = 180;
        const float screenMargin = 50, padding = 20, imageMargin = 10;

        float posX = screenMargin;
        float posY = lf_get_ptr_y() + padding;
        for(uint32_t i = 0; i < state.playlists.size(); i++) {
            auto& playlist = state.playlists[i];

            // Retrieving the displayed Name of the playlist 
            std::string displayName;
            float containerWidth;
            {
                LfTextProps textProps = lf_text_render((vec2s){posX + imageMargin, posY + imageMargin + width}, 
                        playlist.name.c_str(), 
                        lf_theme()->font, posX + width - (imageMargin * 2.0f),
                        -1, -1, -1, -1, 2, 
                        true, 
                        (vec4s){1, 1, 1, 1});
                LfTextProps textPropsNoLimit = lf_text_render((vec2s){posX + imageMargin, posY + imageMargin + width}, 
                        playlist.name.c_str(), 
                        lf_theme()->font, posX + width - (imageMargin * 2.0f),
                        -1, -1, -1, -1, -1, 
                        true, (vec4s){1, 1, 1, 1});

                // Displayed name is equal to every character of the playlist name that can fit onto the container
                displayName = playlist.name.substr(0, textProps.char_count);

                // Adding a "..." indicator if necessarry
                if(textProps.reached_max_wraps && textPropsNoLimit.end_y >= posY + imageMargin + width) {
                    if(displayName.length() >= 3)
                        displayName = displayName.substr(0, displayName.length() - 3) + "...";
                }
                // Getting the coontainer width
                containerWidth = (textProps.width > width) ? textProps.width : width;
            }



            // Moving the Y position down if necessarry
            if(posX + containerWidth + padding >= state.winWidth - screenMargin) {
                posY += height + padding * 2.0f;
                posX = screenMargin;
            }
            
            bool hovered = lf_hovered((vec2s){posX, posY}, (vec2s){containerWidth, height});

            // Container
            lf_rect_render((vec2s){posX, posY}, (vec2s){containerWidth, height + (float)lf_theme()->font.font_size}, hovered ? RGB_COLOR(35, 35, 35) : RGB_COLOR(25, 25, 25), (vec4s){0, 0, 0, 0}, 0.0f, 5);

            // Image
            lf_image_render((vec2s){posX + imageMargin, posY + imageMargin}, RGB_COLOR(255, 255, 255),
                    (LfTexture){.id = state.musicNoteTexture.id, .width = (uint32_t)(width - (imageMargin * 2.0f)), .height = (uint32_t)(width - (imageMargin * 2.0f))}, (vec4s){0, 0, 0, 0}, 0.0f, 5);

            // Name
            lf_text_render((vec2s){posX + imageMargin, posY + imageMargin + width}, displayName.c_str(), lf_theme()->font, posX + (width) - (imageMargin * 2.0f), -1, -1, -1, -1, 2, false, (vec4s){1, 1, 1, 1});

            // If the user clicked on the playlist
            if(hovered && lf_mouse_button_went_down(GLFW_MOUSE_BUTTON_LEFT)) {
               state.currentPlaylist = i; 
               changeTabTo(GuiTab::OnPlaylist);
            }
            posX += width + padding;
        }
    }
    lf_div_end();
}
void renderCreatePlaylist() {
    lf_div_begin((vec2s){DIV_START_X, DIV_START_Y}, (vec2s){(float)state.winWidth, (float)state.winHeight});
    // Heading
    {
        LfUIElementProps props = lf_theme()->text_props;
        props.text_color = LYSSA_BLUE;
        lf_push_style_props(props);
        lf_push_font(&state.h1Font);
        lf_text("Create Playlist");
        lf_pop_style_props();
        lf_pop_font();
    }
    // Form Input
    {
        lf_next_line();
        LfUIElementProps props = lf_theme()->inputfield_props;
        props.padding = 15; 
        props.border_width = 1;
        props.color = (vec4s){0, 0, 0, 0};
        props.border_color = (vec4s){1, 1, 1, 1};
        props.corner_radius = 10;
        props.margin_top = 20;
        props.text_color = (vec4s){1, 1, 1, 1};
        lf_push_style_props(props);
        lf_input_text(&state.createPlaylistTab.nameInput);
        lf_next_line();
        lf_pop_style_props();
    }
    // Create Button
    {
        lf_next_line();
       LfUIElementProps props = lf_theme()->button_props;
       props.margin_top = 20;
       props.color = LYSSA_BLUE;
       props.corner_radius = 5;
       props.border_width = 0;
       lf_push_style_props(props);
       if(lf_button_fixed("Create", 150, 35) == LF_CLICKED) {
           state.createPlaylistTab.createFileStatus = createPlaylist(std::string(state.createPlaylistTab.nameInput.buf));
           memset(state.createPlaylistTab.nameInput.buf, 0, 512);
           state.createPlaylistTab.createFileMessageTimer = 0.0f;
       }
       lf_pop_style_props();
    }

    // File Status Message
    if(state.createPlaylistTab.createFileStatus != FileStatus::None) {
        if(state.createPlaylistTab.createFileMessageTimer < state.createPlaylistTab.createFileMessageShowTime) {
            state.createPlaylistTab.createFileMessageTimer += state.deltaTime;
            lf_next_line();
            lf_push_font(&state.h4Font);
            LfUIElementProps props = lf_theme()->button_props;
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

    backButtonTo(GuiTab::Dashboard);

    lf_div_end();
}


void renderOnPlaylist() {
    if(state.currentPlaylist == -1) return;

    lf_div_begin((vec2s){DIV_START_X, DIV_START_Y}, (vec2s){(float)state.winWidth, (float)state.winHeight});

    auto& currentPlaylist = state.playlists[state.currentPlaylist];

    // Playlist Heading
    {
        // Title 
        {
            lf_push_font(&state.musicTitleFont);
            LfUIElementProps props = lf_theme()->text_props;
            props.text_color = LYSSA_BLUE;
            lf_push_style_props(props);
            lf_text(currentPlaylist.name.c_str());
            lf_pop_style_props();
            lf_pop_font();
        }

        // "Add More" button
        {
            lf_push_font(&state.h5Font);
            const char* text = "Add more Music";
            float textWidth = lf_text_dimension(text).x;
            LfUIElementProps props = lf_theme()->button_props;
            props.color = (vec4s){0, 0, 0, 0};
            props.text_color = (vec4s){1, 1, 1, 1};
            props.padding = 10;
            props.border_width = 0;
            lf_set_ptr_x(state.winWidth - textWidth - 80);
            lf_push_style_props(props);
            float ptr_x = lf_get_ptr_x();
            float ptr_y = lf_get_ptr_y();
            if(lf_button(text) == LF_CLICKED) {
                state.popups[(int32_t)PopupID::FileOrFolderPopup].render = !state.popups[(int32_t)PopupID::FileOrFolderPopup].render;
            }  
            lf_pop_style_props();
        }
    }
    if(currentPlaylist.musicFiles.empty()) {
        // Text
        {
            lf_set_ptr_y(100);
            lf_push_font(&state.h5Font);
            const char* text = "There is no music in this playlist.";
            float textWidth = lf_text_dimension(text).x;

            // Centering the text
            lf_set_ptr_x((state.winWidth - textWidth) / 2.0f - DIV_START_X);

            LfUIElementProps props = lf_theme()->text_props;
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
            LfUIElementProps props = lf_theme()->button_props;
            props.color = RGB_COLOR(240, 240, 240);
            props.text_color = RGB_COLOR(0, 0, 0);
            props.corner_radius = 10; 
            props.border_width = 0;
            props.margin_top = 20;

            // Centering the buttons
            lf_set_ptr_x((state.winWidth - 
                        (buttonWidth + props.padding * 2.0f) * 2.0f - (props.margin_right + props.margin_left) * 2.f) / 2.0f - DIV_START_X);

            lf_push_style_props(props);
            if(lf_button_fixed("Add from file", buttonWidth, 40) == LF_CLICKED) {
                changeTabTo(GuiTab::PlaylistAddFromFile);
            }
            if(lf_button_fixed("Add from Folder", buttonWidth, 40) == LF_CLICKED) {
                changeTabTo(GuiTab::PlaylistAddFromFolder);
            }
            lf_pop_style_props();
            lf_pop_font();
        }
    } else {
        // Popup Variables
        vec2s popupPos = (vec2s){-1, -1};
        std::string popupFilePath = "";
        int32_t popupIndex = -1;

        lf_next_line();
        {
            lf_push_font(&state.h3Font);
            LfUIElementProps props = lf_theme()->text_props;
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
            LfUIElementProps props = lf_theme()->text_props;
            props.margin_bottom = 20;
            lf_push_style_props(props);
            lf_text("#");

            lf_set_ptr_x(lf_get_ptr_x() + state.winWidth / 4.0f - (lf_text_dimension("#").x + lf_theme()->text_props.margin_right + lf_theme()->text_props.margin_left));
            lf_text("Track");

            lf_set_ptr_x(state.winWidth - (lf_text_dimension("Duration").x) -  DIV_START_X * 2 - lf_theme()->text_props.margin_left);
            lf_text("Duration");
            lf_pop_style_props();

            lf_next_line();
        }

        
        // Begin a new div container for the files
        {
            lf_div_begin(LF_PTR, (vec2s){(float)state.winWidth - DIV_START_X * 2, (float)state.winHeight - DIV_START_Y * 2 - lf_get_ptr_y() - (BACK_BUTTON_HEIGHT + BACK_BUTTON_MARGIN_BOTTOM)});
        }

        if(lf_mouse_clicked_div(GLFW_MOUSE_BUTTON_LEFT))
            state.playlistFileOptionsIndex = -1;

        for(uint32_t i = 0; i < currentPlaylist.musicFiles.size(); i++) {
            SoundFile& file = currentPlaylist.musicFiles[i];
            {
                vec2s thumbnailContainerSize = (vec2s){48, 48};
                vec4s selectedColor = RGB_COLOR(255, 255, 255);
                float marginBottomThumbnail = 10.0f, 
                      marginTopThumbnail = 5.0f, 
                      marginLeftThumbnail = 10.0f;
                selectedColor = (vec4s){LF_ZTO_TO_RGBA(selectedColor.r, selectedColor.g, selectedColor.b, selectedColor.a)};

                LfAABB fileAABB = (LfAABB){
                    .pos = LF_PTR,
                    .size = (vec2s){(float)state.winWidth - DIV_START_X * 2, (float)thumbnailContainerSize.y + marginBottomThumbnail - marginTopThumbnail}
                };

                bool hovered_text_div = lf_hovered(fileAABB.pos, fileAABB.size);
                if(hovered_text_div) {
                    if(lf_mouse_button_went_down(GLFW_MOUSE_BUTTON_RIGHT)) {
                        state.playlistFileOptionsIndex = i;
                    }
                    lf_rect_render(fileAABB.pos, fileAABB.size, RGBA_COLOR(selectedColor.r, selectedColor.g, selectedColor.b, 60),
                            (vec4s){0, 0, 0, 0}, 0.0f, 2.0f);
                }
                if(currentPlaylist.playingFile == i) {
                    lf_rect_render(fileAABB.pos, fileAABB.size, RGBA_COLOR(selectedColor.r, selectedColor.g, selectedColor.b, 80),
                            (vec4s){0, 0, 0, 0}, 0.0f, 2.0f);
                } 

                // Index heading
                {
                    std::stringstream indexSS;
                    indexSS << i;
                    std::string indexStr = indexSS.str();
                    lf_text(indexStr.c_str());

                    // Pointer for Title heading
                    lf_set_ptr_x(lf_get_ptr_x() + state.winWidth / 4.0f - (lf_text_dimension(indexStr.c_str()).x + lf_theme()->text_props.margin_right + lf_theme()->text_props.margin_left));
                }

                // Title Heading
                {
                    std::filesystem::path fsPath(file.path);
                    std::string filename = removeFileExtension(fsPath.filename().string());

                    float aspect = (float)file.thumbnail.width / (float)file.thumbnail.height;
                    float thumbnailHeight = thumbnailContainerSize.y / aspect; 

                    lf_rect_render((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + marginTopThumbnail}, thumbnailContainerSize, 
                            RGBA_COLOR(40, 40, 40, 255), LF_NO_COLOR, 0.0f, 3.0f);

                    lf_image_render((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + (thumbnailContainerSize.y - thumbnailHeight) / 2.0f + marginTopThumbnail}, LF_WHITE, (LfTexture){.id = file.thumbnail.id, .width = (uint32_t)thumbnailContainerSize.x, .height = (uint32_t)thumbnailHeight}, LF_NO_COLOR, 0.0f, 0.0f);  

                    lf_set_ptr_x(lf_get_ptr_x() + thumbnailContainerSize.x + marginLeftThumbnail);
                    lf_set_line_height(thumbnailContainerSize.y + marginBottomThumbnail);

                    lf_text(filename.c_str());
                }

                if(state.playlistFileOptionsIndex == i) {
                    popupPos = (vec2s){lf_get_ptr_x() + 20, lf_get_ptr_y() + 20};
                    popupFilePath = file.path;
                    popupIndex = i;
                }

                if(lf_mouse_button_went_down(GLFW_MOUSE_BUTTON_LEFT) && hovered_text_div && state.playlistFileOptionsIndex != i) {
                    if(i != currentPlaylist.playingFile) {
                        playlistPlayFileWithIndex(i, state.currentPlaylist);
                    }
                    state.currentSoundFile = &file;
                    state.onTrackTab.trackThumbnail = file.thumbnail;
                    changeTabTo(GuiTab::OnTrack);
                }


                // Duration Heading
                {
                    lf_set_ptr_x(state.winWidth - (lf_text_dimension("Duration").x) -  DIV_START_X * 2 - lf_theme()->text_props.margin_left);
                    lf_text(formatDurationToMins(file.duration).c_str());
                    lf_next_line();    
                }
            }
        }
        lf_div_end();

        if(popupPos.x != -1 && popupPos.y != 1)
        {
            LfUIElementProps props = lf_theme()->button_props;
            props.color = RGB_COLOR(40, 40, 40);
            props.corner_radius = 5;
            props.border_width = 0;
            lf_push_style_props(props);
            lf_div_begin(popupPos, (vec2s){150, 50});
            lf_pop_style_props();

                {
                    LfUIElementProps props = lf_theme()->button_props;
                    props.corner_radius = 5;
                    props.color = LYSSA_RED;
                    props.border_width = 0;
                    lf_push_style_props(props);
                    if(lf_button("Remove") == LF_CLICKED) {
                        FileStatus removeStatus = removeFileFromPlaylist(popupFilePath, state.currentPlaylist);
                        if(removeStatus == FileStatus::Failed) {
                            std::cout << "[Error]: Failed to remove file '" << popupFilePath << "' from playlist '" << currentPlaylist.name << "'.\n";
                        }
                        if(currentPlaylist.playingFile == popupIndex) {
                            state.currentSound.stop();
                            state.currentSound.uninit();
                        }
                        state.playlistFileOptionsIndex = -1;
                    }
                    lf_pop_style_props();
                }
                lf_div_end();

            }
    }

    backButtonTo(GuiTab::Dashboard);

    lf_div_end();
}
void renderOnTrack() {
    OnTrackTab& tab = state.onTrackTab;

    lf_div_begin((vec2s){DIV_START_X, DIV_START_Y}, (vec2s){(float)state.winWidth, (float)state.winHeight});

    std::filesystem::path filepath = state.currentSoundFile->path;
    std::string filename = removeFileExtension(filepath.filename());
    lf_push_font(&state.musicTitleFont);
    float titleWidth = lf_text_dimension(filename.c_str()).x;
    lf_pop_font();
    float titlePosX = (state.winWidth - titleWidth) / 2.0f - DIV_START_X;

    {
        {
            const vec2s iconSize = (vec2s){state.soundIcon.width / 10.0f, state.soundIcon.height / 10.0f};

            LfUIElementProps props = lf_theme()->button_props;
            props.color = LF_NO_COLOR;
            props.border_color = LF_NO_COLOR;
            props.border_width = 0.0f;
            lf_push_style_props(props);
            if(lf_image_button((LfTexture){.id = state.soundIcon.id, .width = (uint32_t)iconSize.x, .height = (uint32_t)iconSize.y}) == LF_CLICKED) {
                tab.showVolumeSlider = !tab.showVolumeSlider; 
            }
            lf_pop_style_props();
        }
        state.onTrackTab.volumeSlider.width = 100; 
        if(tab.showVolumeSlider) {
            LfUIElementProps props = lf_theme()->slider_props;
            props.corner_radius = 1.5;
            props.color = RGBA_COLOR(255, 255, 255, 30);
            props.text_color = LF_WHITE;
            props.border_width = 0;
            props.margin_top = 30;
            lf_push_style_props(props);

            lf_rect_render((vec2s){lf_get_ptr_x() + props.margin_left, lf_get_ptr_y() + props.margin_top}, (vec2s){(float)tab.volumeSlider.handle_pos, (float)tab.volumeSlider.height}, 
                    props.text_color, LF_NO_COLOR, 0.0f, props.corner_radius);
            lf_slider_int(&state.onTrackTab.volumeSlider);
            lf_pop_style_props();
        }
    }
    // Sound Title
    {

        lf_push_font(&state.musicTitleFont);
        lf_set_ptr_x(titlePosX);
        lf_text(filename.c_str());
        lf_pop_font();
    }

    // Sound Thumbnail
    {
        const vec2s thumbnailContainerSize = (vec2s){350, 350};
        lf_set_ptr_x(((state.winWidth - thumbnailContainerSize.x) / 2.0f - DIV_START_X));
        lf_set_ptr_y(lf_get_ptr_y() + 100);

        float ptrX = lf_get_ptr_x();
        float ptrY = lf_get_ptr_y();
        lf_rect_render(LF_PTR, 
                thumbnailContainerSize, RGBA_COLOR(255, 255, 255, 30), LF_NO_COLOR, 0.0f, 8.0f);

        float aspect = (float)tab.trackThumbnail.width / (float)tab.trackThumbnail.height;

        float thumbnailHeight = thumbnailContainerSize.y / aspect; 

        lf_image_render((vec2s){lf_get_ptr_x(), lf_get_ptr_y() + (thumbnailContainerSize.y - thumbnailHeight) / 2.0f}, 
                LF_WHITE, (LfTexture){.id = tab.trackThumbnail.id, .width = (uint32_t)thumbnailContainerSize.x, .height = (uint32_t)thumbnailHeight}, LF_NO_COLOR, 0.0f, 0.0f);   

        lf_set_ptr_x(ptrX);
        lf_set_ptr_y(ptrY + thumbnailContainerSize.y);
    }


    // Progress Bar 
    {
        const vec2s progressBarSize = {400, 10}; 
   
        lf_set_ptr_x((state.winWidth - progressBarSize.x) / 2.0f - DIV_START_X);
        
        LfUIElementProps props = lf_theme()->slider_props;
        props.margin_top = 20;
        props.corner_radius = 1.5;
        props.color = RGBA_COLOR(255, 255, 255, 100);
        props.text_color = LF_WHITE;
        props.border_width = 0;
        lf_push_style_props(props);

        vec2s posPtr = (vec2s){lf_get_ptr_x() + props.margin_left, lf_get_ptr_y() + props.margin_top};

        LfClickableItemState slider = lf_slider_int(&tab.trackProgress);

        lf_rect_render(posPtr, (vec2s){(float)tab.trackProgress.handle_pos, (float)tab.trackProgress.height}, LF_WHITE, LF_NO_COLOR, 0.0f, props.corner_radius /= 2.0f);

        if(slider == LF_RELEASED || slider == LF_CLICKED) {
            state.currentSound.setPositionInSeconds(state.currentSoundPos);
        }

        lf_pop_style_props();
    }

    lf_next_line();

    {
        const float iconSizeSm = 48;
        const float iconSizeXsm = 36;

        LfUIElementProps props = lf_theme()->button_props;
        props.margin_left = 15;
        props.margin_right = 15;

        lf_set_ptr_x((state.winWidth - ((iconSizeSm + props.padding * 2.0f) + (iconSizeXsm * 2.0f) + (props.margin_left + props.margin_right) * 3.0f)) / 2.0f - DIV_START_X);

        props.color = LF_NO_COLOR;
        props.border_width = 0; 
        props.corner_radius = 0; 
        props.margin_top = iconSizeXsm / 2.0f;
        props.padding = 0;
       
        lf_push_style_props(props);
        if(lf_image_button((LfTexture){.id = state.skipDownTexture.id, .width = (uint32_t)iconSizeXsm, .height = (uint32_t)iconSizeXsm}) == LF_CLICKED) {
            skipSoundDown();
        }
        lf_pop_style_props();

        {
            props.color = LF_WHITE;
            props.corner_radius = 16;
            props.padding = 10;
            props.margin_top = 0;
            lf_push_style_props(props);
            if(lf_image_button((LfTexture){.id = state.currentSound.isPlaying ? state.pauseTexture.id : state.playTexture.id, .width = (uint32_t)iconSizeSm, .height = (uint32_t)iconSizeSm}) == LF_CLICKED) {
                if(state.currentSound.isPlaying)
                    state.currentSound.stop();
                else 
                    state.currentSound.play();
            }
            lf_pop_style_props();
        }

        props.color = LF_NO_COLOR;
        props.border_width = 0;
        props.corner_radius = 0;
        props.margin_top = iconSizeXsm / 2.0f;
        props.padding = 0;
       
        lf_push_style_props(props);
        if(lf_image_button((LfTexture){.id = state.skipUpTexture.id, .width = (uint32_t)iconSizeXsm, .height = (uint32_t)iconSizeXsm}) == LF_CLICKED) {
            skipSoundUp();
        }
        lf_pop_style_props();
    }

    backButtonTo(GuiTab::OnPlaylist);

    lf_div_end();
}
void renderPlaylistAddFromFile() {
    lf_div_begin((vec2s){DIV_START_X, DIV_START_Y}, (vec2s){(float)state.winWidth, (float)state.winHeight});
    
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
        LfUIElementProps props = lf_theme()->inputfield_props;
        props.padding = 15; 
        props.border_width = 1;
        props.color = (vec4s){0, 0, 0, 0};
        props.border_color = (vec4s){1, 1, 1, 1};
        props.corner_radius = 10;
        props.margin_top = 20;
        props.text_color = (vec4s){1, 1, 1, 1};
        lf_push_style_props(props);
        lf_input_text(&state.playlistAddFromFileTab.pathInput);
        lf_next_line();
        lf_pop_style_props();
    }
    // Add Button 
    {
        lf_next_line();
        LfUIElementProps props = lf_theme()->button_props;
        props.margin_top = 20;
        props.color = LYSSA_GREEN;
        props.corner_radius = 5;
        props.border_width = 0;
        lf_push_style_props(props);
        if(lf_button_fixed("Add", 150, 35) == LF_CLICKED) {
            state.playlistAddFromFileTab.addFileStatus = addFileToPlaylist(state.playlistAddFromFileTab.pathInput.buf, state.currentPlaylist);
            memset(state.playlistAddFromFileTab.pathInput.buf, 0, 512);
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
            LfUIElementProps props = lf_theme()->button_props;
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


    lf_div_end();
}
void renderPlaylistAddFromFolder() {
    lf_div_begin((vec2s){DIV_START_X, DIV_START_Y}, (vec2s){(float)state.winWidth, (float)state.winHeight});

    // Heading
    {
        lf_push_font(&state.h1Font);
        lf_text("Load Files from Folder");
        lf_pop_font();
    }
    // Input 
    {
        lf_next_line();
        LfUIElementProps props = lf_theme()->inputfield_props;
        props.padding = 15; 
        props.color = (vec4s){0, 0, 0, 0};
        props.border_color = (vec4s){1, 1, 1, 1};
        props.corner_radius = 10;
        props.border_width = 1;
        props.margin_top = 20;
        props.text_color = (vec4s){1, 1, 1, 1};
        lf_push_style_props(props);
        lf_input_text(&state.playlistAddFromFolderTab.pathInput);
        lf_next_line();
        lf_pop_style_props();
    }
    lf_next_line();
    // Load Button 
    {
        LfUIElementProps props = lf_theme()->button_props;
        props.color = LYSSA_GREEN;
        props.corner_radius = 5;
        props.margin_top = 15;
        props.border_width = 0;
        lf_push_style_props(props);
        if(lf_button_fixed("Load Files", 175, -1) == LF_CLICKED) {
            state.playlistAddFromFolderTab.loadedFolders.push_back((Folder){.path = std::string(state.playlistAddFromFolderTab.pathInput.buf), .files = loadFilesFromFolder(state.playlistAddFromFolderTab.pathInput.buf)}); 
            state.playlistAddFromFolderTab.folderIndex++;
            if(state.playlistAddFromFolderTab.fileContainer != NULL) {
                state.playlistAddFromFolderTab.fileContainer->scroll = 0;
            }
        }
        lf_pop_style_props();
    }
    lf_next_line();
    {
        // Tabs 
        for(uint32_t i = 0; i < state.playlistAddFromFolderTab.loadedFolders.size(); i++) {
            Folder& folder = state.playlistAddFromFolderTab.loadedFolders[i];
            std::filesystem::path folderPath = folder.path;
            std::string folderName = folderPath.filename();
            LfUIElementProps props = lf_theme()->button_props;
            props.border_width = 0;
            props.corner_radius = 3;
            props.margin_left = 15;
            props.margin_right = -15;
            props.margin_bottom = 10;
            props.color = state.playlistAddFromFolderTab.folderIndex == i ? RGB_COLOR(140, 140, 140) :  RGB_COLOR(200, 200, 200);
            lf_push_style_props(props);
            if(lf_button_fixed(folderName.c_str(), 160, -1) == LF_CLICKED) {
                if(state.playlistAddFromFolderTab.fileContainer != NULL) {
                    state.playlistAddFromFolderTab.folderIndex = i;
                    state.playlistAddFromFolderTab.fileContainer->scroll = 0;
                }
            }
            lf_pop_style_props();
        }

        lf_next_line();

        // File Container
        if(state.playlistAddFromFolderTab.folderIndex != -1)
        {
            Folder& selectedFolder = state.playlistAddFromFolderTab.loadedFolders[state.playlistAddFromFolderTab.folderIndex];

            // Props
            LfUIElementProps props = lf_theme()->div_props;
            props.color = RGBA_COLOR(40, 40, 40, 120);
            props.corner_radius = 10;
            lf_push_style_props(props);

            state.playlistAddFromFolderTab.fileContainer = lf_div_begin(LF_PTR, (vec2s){(float)state.winWidth - (DIV_START_X * 2), 
                                                                        (float)state.winHeight - DIV_START_Y * 2 - lf_get_ptr_y() - (BACK_BUTTON_HEIGHT + BACK_BUTTON_MARGIN_BOTTOM)});

            for(auto& file : selectedFolder.files) {
                LfAABB file_aabb = (LfAABB){
                    .pos = (vec2s){lf_get_current_div().aabb.pos.x + lf_theme()->text_props.margin_left, lf_get_ptr_y()},
                    .size =  (vec2s){lf_get_current_div().aabb.size.x - lf_theme()->text_props.margin_right * 2.0f, (float)lf_theme()->font.font_size}
                };

                bool hoveredFile = lf_hovered(file_aabb.pos, file_aabb.size);
                bool fileInPlaylist = isFileInPlaylist(file, state.currentPlaylist);

                // Indicate that the file is already in playlist
                if(fileInPlaylist && hoveredFile) {
                    vec4s greenTint = RGBA_COLOR(47, 168, 92, 125);
                    lf_rect_render(file_aabb.pos, file_aabb.size, greenTint, LF_NO_COLOR, 0.0f, 3.0f);
                }

                std::string filename = std::filesystem::path(file).filename();
                lf_text(filename.c_str());

                // Render add button
                if(!fileInPlaylist && hoveredFile) {
                    LfUIElementProps props = lf_theme()->button_props;
                    props.corner_radius = 3;
                    props.border_width = 0;
                    lf_push_style_props(props);
                    if(lf_image_button((LfTexture){.id = state.addTexture.id, .width = 15, .height = 15}) == LF_CLICKED) {
                        addFileToPlaylist(file, state.currentPlaylist);
                    }
                    lf_pop_style_props();
                }
                lf_next_line();
            }
            lf_div_end();
            lf_pop_style_props();
        }
    }

    backButtonTo(GuiTab::OnPlaylist);
    lf_div_end();

}

void renderFileOrFolderPopup() {
    // Beginning a new div
    const vec2s popupSize = (vec2s){400.0f, 100.0f};
    LfUIElementProps props = lf_theme()->div_props;
    props.color = RGB_COLOR(25, 25, 25);
    props.padding = 5;
    props.corner_radius = 10;
    lf_push_style_props(props);
    // Centering the div/popup
    lf_div_begin((vec2s){(state.winWidth - popupSize.x) / 2.0f, (state.winHeight - popupSize.y) / 2.0f}, popupSize);
    // Close Button
    {
        // Put the X Button in the top left of the div 

        // Styling
        LfUIElementProps props = lf_theme()->button_props;
        props.margin_left = 0;
        props.margin_right = 0;
        props.margin_top = 0;
        props.margin_bottom = 0;
        props.text_color = RGB_COLOR(255, 255, 255);
        props.color = RGBA_COLOR(0, 0, 0, 0);
        props.border_width = 0;

        lf_push_style_props(props);
        if(lf_button("X") == LF_CLICKED) {
            state.popups[(int32_t)PopupID::FileOrFolderPopup].render = false;
        }
        lf_pop_style_props();
        lf_next_line();
        lf_set_ptr_y(20);
    }
    // Popup Title
    {
        const char* text = "How do you want to add Music?";
        lf_push_font(&state.h6Font);
        float textWidth = lf_text_dimension(text).x;
        lf_set_ptr_x((lf_get_div_size().x - textWidth) / 2.0f);
        lf_text(text);
        lf_pop_font();
    }
    // Popup Buttons
    lf_next_line();
    {
        // Styling
        LfUIElementProps bprops = lf_theme()->button_props;
        bprops.border_width = 0;
        bprops.corner_radius = 5;
        bprops.color = LYSSA_BLUE;
        lf_push_style_props(bprops);

        // Make the buttons stretch the entire div
        float halfDivWidth = lf_get_div_size().x / 2.0f - bprops.padding * 2.0f - bprops.border_width * 2.0f  - (bprops.margin_left + bprops.margin_right);
        if(lf_button_fixed("From File", halfDivWidth, -1) == LF_CLICKED) {
            changeTabTo(GuiTab::PlaylistAddFromFile);
            state.popups[(int32_t)PopupID::FileOrFolderPopup].render = false;
        }
        if(lf_button_fixed("From Folder", halfDivWidth, -1) == LF_CLICKED) {
            changeTabTo(GuiTab::PlaylistAddFromFolder);
            state.popups[(int32_t)PopupID::FileOrFolderPopup].render = false;
        }
        lf_pop_style_props();
    }
    lf_div_end();
    lf_pop_style_props();
}

void backButtonTo(GuiTab tab) {
    lf_next_line();

    float ptr_y = lf_get_ptr_y();

    lf_set_ptr_y(state.winHeight - BACK_BUTTON_MARGIN_BOTTOM - BACK_BUTTON_HEIGHT * 2);
    LfUIElementProps props = lf_theme()->button_props;
    props.color = (vec4s){0, 0, 0, 0};
    props.border_width = 0;
    lf_push_style_props(props);

    if(lf_image_button((LfTexture){.id = state.backTexture.id, .width = BACK_BUTTON_WIDTH, .height = BACK_BUTTON_HEIGHT}) == LF_CLICKED) {
        changeTabTo(tab);
        if(state.currentPlaylist != -1)
            loadPlaylist(std::filesystem::directory_entry(state.playlists[state.currentPlaylist].path));
    }

    lf_pop_style_props();

    lf_set_ptr_y(ptr_y);
}
void changeTabTo(GuiTab tab) {
    state.currentTab = tab;

    for(uint32_t i = 0; i < lf_get_div_count(); i++) {
        lf_get_divs()[i].scroll = 0;
    }
}

FileStatus createPlaylist(const std::string& name) {
    std::string folderPath = LYSSA_DIR + "/" + name;
    if(!std::filesystem::exists(folderPath)) {
        if(!std::filesystem::create_directory(folderPath) )
            return FileStatus::Failed;
    } else {
        return FileStatus::AlreadyExists;
    }

    std::ofstream metadata(folderPath + "/.metadata");
    if(metadata.is_open()) {
        metadata << "name: " << name << "\n";
        metadata << "files: ";
    } else {
        return FileStatus::Failed;
    }
    metadata.close();

    loadPlaylist(std::filesystem::directory_entry(folderPath));
    return FileStatus::Success;
}
FileStatus addFileToPlaylist(const std::string& path, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];

    if(isFileInPlaylist(path, playlistIndex)) return FileStatus::AlreadyExists;

    std::ofstream metadata(playlist.path + "/.metadata", std::ios::app);

    if(!metadata.is_open()) return FileStatus::Failed;

    std::ifstream playlistFile(path);
    if(!playlistFile.good()) return FileStatus::Failed;

    metadata.seekp(0, std::ios::end);

    metadata << "\"" << path << "\" ";
    playlist.musicFiles.push_back((SoundFile){path, static_cast<int32_t>(getSoundDuration(path))});
    metadata.close();

    return FileStatus::Success;
}

FileStatus removeFileFromPlaylist(const std::string& path, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];

    if(!isFileInPlaylist(path, playlistIndex)) return FileStatus::Failed;

    std::ofstream metdata(playlist.path + "/.metadata", std::ios::trunc);

    if(!metdata.is_open()) return FileStatus::Failed;

    metdata << "name: " << playlist.name << "\n";
    metdata << "files: ";

    for(auto& file : playlist.musicFiles) {
        if(file.path == path) {
            playlist.musicFiles.erase(std::find(playlist.musicFiles.begin(), playlist.musicFiles.end(), file));
            break;
        }
    }

    for(auto& file : playlist.musicFiles) {
        metdata << "\"" << file.path << "\" ";
    }

    return FileStatus::Success;
}

bool isFileInPlaylist(const std::string& path, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    for(auto& file : playlist.musicFiles) {
        if(file.path == path) return true;
    }
    return false;
}
void loadPlaylist(const std::filesystem::directory_entry& folder) {
    if (folder.is_directory()) {
        Playlist* loadedPlaylist = nullptr;
        for(auto& playlist : state.playlists) {
            if(playlist.path == folder.path().string()) {
                loadedPlaylist = &playlist;
                break;
            }
        }

        std::ifstream metadata(folder.path().string() + "/.metadata");

        if(!metadata.is_open()) {
            std::cerr << "[Error] Failed to open the metadata of playlist on path '" << folder.path().string() << "'\n";
            return;
        }
        std::string name;
        std::vector<SoundFile> files;

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
            if (line.find("files:") != std::string::npos) {
                // If the line contains "files:", extract paths from the rest of the line
                std::istringstream iss(line.substr(line.find("files:") + std::string("files:").length()));
                std::string path;
           
                while (iss >> std::quoted(path)) {
                    files.push_back((SoundFile){
                            .path = path, 
                            .duration = static_cast<int32_t>(getSoundDuration(path)), 
                            .thumbnail = getSoundThubmnail(path)});
                }
            }
        }
        metadata.close();

        if(!loadedPlaylist) {
            Playlist playlist;
            playlist.path = folder.path().string();
            playlist.name = name;
            playlist.musicFiles = files;
            state.playlists.push_back(playlist);
        } else {
            loadedPlaylist->path = folder.path().string();
            loadedPlaylist->name = name;
            loadedPlaylist->musicFiles = files;
        }

    }
}
void loadPlaylists() {
    for (const auto& folder : std::filesystem::directory_iterator(LYSSA_DIR)) {
        loadPlaylist(folder);
    }
}

std::vector<std::string> loadFilesFromFolder(const std::filesystem::path& folderPath) {
    std::vector<std::string> files;
    for (const auto& entry : std::filesystem::directory_iterator(folderPath)) {
        if (std::filesystem::is_directory(entry.path()) && entry.path().filename().string()[0] != '.') {
            files = loadFilesFromFolder(entry.path());
        } else if (std::filesystem::is_regular_file(entry.path())) {
            files.push_back(entry.path().string());
        }
    } 
    return files;
}

void playlistPlayFileWithIndex(uint32_t i, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];
    playlist.playingFile = i;

    if(state.currentSound.isPlaying)
        state.currentSound.stop();

    if(state.currentSound.isInit)
        state.currentSound.uninit();

    state.currentSound.init(playlist.musicFiles[i].path);
    state.currentSound.play();

    state.currentSoundPos = 0.0;
    state.onTrackTab.trackProgress._init = false;
    state.onTrackTab.trackProgress.max = state.currentSound.lengthInSeconds;

}

void skipSoundUp() {
    Playlist& playlist = state.playlists[state.currentPlaylist];

    if(playlist.playingFile + 1 < playlist.musicFiles.size())
        playlist.playingFile++;
    else 
        playlist.playingFile = 0;

    state.currentSoundFile = &playlist.musicFiles[playlist.playingFile];
    state.onTrackTab.trackThumbnail = state.currentSoundFile->thumbnail;

    playlistPlayFileWithIndex(playlist.playingFile, state.currentPlaylist);

}

void skipSoundDown() {
    Playlist& playlist = state.playlists[state.currentPlaylist];

    if(playlist.playingFile - 1 >= 0)
        playlist.playingFile--;
    else 
        playlist.playingFile = playlist.musicFiles.size() - 1; 

    state.currentSoundFile = &playlist.musicFiles[playlist.playingFile];
    state.onTrackTab.trackThumbnail = state.currentSoundFile->thumbnail;

    playlistPlayFileWithIndex(playlist.playingFile, state.currentPlaylist);
}

double getSoundDuration(const std::string& soundPath) {
    Sound sound; 
    sound.init(soundPath);
    double duration = sound.lengthInSeconds;
    sound.uninit();
    return duration;
}

LfTexture getSoundThubmnail(const std::string& soundPath) {    
    MPEG::File file(soundPath.c_str());

    LfTexture tex = {0};
    // Get the ID3v2 tag
    ID3v2::Tag *tag = file.ID3v2Tag();
    if (!tag) {
        std::cerr << "[Error] No ID3v2 tag found for file '" << soundPath << "'.\n";
        return tex;
    }

    // Get the first APIC (Attached Picture) frame
    ID3v2::FrameList apicFrames = tag->frameListMap()["APIC"];
    if (apicFrames.isEmpty()) {
        std::cerr << "[Error]: No APIC frame found for file '" << soundPath << "'.\n";
        return tex;
    }

    // Extract the image data
    ID3v2::AttachedPictureFrame *apicFrame = dynamic_cast<ID3v2::AttachedPictureFrame *>(apicFrames.front());
    if (!apicFrame) {
        std::cerr << "[Error]: Failed to cast APIC frame for file '" << soundPath << "'.\n";
        return tex;
    }

    const ByteVector& imageData = apicFrame->picture();
    
    tex = lf_load_texture_from_memory(imageData.data(), (int)imageData.size(), true, LF_TEX_FILTER_LINEAR);

    return tex;
}
void updateSoundProgress() {
    if(!state.currentSound.isInit) {
        return;
    }

    if(state.currentSoundPos + 1 <= state.currentSound.lengthInSeconds && state.currentSound.isPlaying) {
        state.soundPosUpdateTime += state.deltaTime;
        if(state.soundPosUpdateTime >= state.soundPosUpdateTimer) {
            state.soundPosUpdateTime = 0.0f;
            state.currentSoundPos++;
            state.onTrackTab.trackProgress._init = false;
        }
    }

    if(state.currentSoundPos >= (uint32_t)state.currentSound.lengthInSeconds && !state.onTrackTab.trackProgress.held) {
        skipSoundUp();
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
    initWin(1280, 720); 
    initUI();

    if(!std::filesystem::exists(LYSSA_DIR)) { 
        std::filesystem::create_directory(LYSSA_DIR);
    }

    loadPlaylists();
    // Creating the popups
    state.popups.reserve((int32_t)PopupID::PopupCount);
    state.popups[(int32_t)PopupID::FileOrFolderPopup] = (Popup){.renderCb = renderFileOrFolderPopup, .render = false};

    while(!glfwWindowShouldClose(state.win)) { 
        // Updating the timestamp of the currently playing sound
        updateSoundProgress();
        handleTabKeyStrokes();

        // Delta-Time calculation
        float currentTime = glfwGetTime();
        state.deltaTime = currentTime - state.lastTime;
        state.lastTime = currentTime;

        // OpenGL color clearing 
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(LF_RGBA(0, 0, 0, 255));


        lf_begin();
        switch(state.currentTab) {
            case GuiTab::Dashboard:
                renderDashboard();
                break;
            case GuiTab::CreatePlaylist:
                renderCreatePlaylist();
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
            } else {
                lf_div_hide();
            }
        } 


        lf_end();

        glfwPollEvents();
        glfwSwapBuffers(state.win);

    }

    return 0;
} 
