#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <vector>
#include <cstdlib> 

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define MINIAUDIO_IMPLEMENTATION
#include "../vendor/miniaudio/miniaudio.h"
#include "../leif.h"

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
#define DIV_START_Y 50 

enum class GuiTab {
    Dashboard = 0, 
    CreatePlaylist,
    OnPlaylist,
    PlaylistAddFromFile,
    PlaylistAddFromFolder
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

struct Playlist {
    std::vector<std::string> musicFiles;
    std::string name, path;
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

struct Folder {
    std::string path;
    std::vector<std::string> files;
};

struct PlaylistAddFromFolderTab {
    LfInputField pathInput;

    uint32_t folderIndex = 0;
    std::vector<Folder> loadedFolders;
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

    LfTexture backTexture, musicNoteTexture;

    CreatePlaylistState createPlaylistTab;
    PlaylistAddFromFileTab playlistAddFromFileTab;
    PlaylistAddFromFolderTab playlistAddFromFolderTab;

    int32_t currentPlaylist = -1;
};

static GlobalState state;

static void                     winResizeCb(GLFWwindow* window, int32_t width, int32_t height);
static void                     initWin(uint32_t width, uint32_t height);
static void                     initUI();
static void                     handleKeystrokes();


static void                     renderDashboard();
static void                     renderCreatePlaylist();
static void                     renderOnPlaylist();
static void                     renderPlaylistAddFromFile();
static void                     renderPlaylistAddFromFolder();

static void                     renderFileOrFolderPopup();

static void                     backButtonTo(GuiTab tab);

static FileStatus               createPlaylist(const std::string& name);
static FileStatus               addFileToPlaylist(const std::string& path, uint32_t playlistIndex);
static void                     loadPlaylists();
static std::vector<std::string> loadFilesFromFolder(const std::filesystem::path& folderPath);

template<typename T>
static bool elementInVector(const std::vector<T>& v, const T& e) {
    for(auto& element : v) 
        if(element == e) 
            return true;
    return false;
}

static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
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
        std::cerr << "Failed to load Sound '" << filepath << "'.\n";
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
    ma_device_stop(&device);
    ma_device_uninit(&device);
    ma_decoder_uninit(&decoder);
    isInit = false;
}

void Sound::play() {
    ma_device_start(&device);
    isPlaying = true;
}

void Sound::stop() {
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
        std::cerr << "Failed to initialize GLFW.\n";
    }

    state.win = glfwCreateWindow(width, height, "Lyssa Music" ,NULL, NULL);
    if(!state.win) {
        std::cerr << "Failed to create GLFW window.\n";
    }
    glfwMakeContextCurrent(state.win);
    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize Glad.\n";
    }
    LfTheme theme = lf_default_theme("../game/fonts/inter.ttf", 24);
    theme.div_props.color = (vec4s){LF_RGBA(0, 0, 0, 0)};
    lf_init_glfw(width, height, "../game/fonts/inter.ttf", &theme, state.win);   
    lf_set_text_wrap(true);
    glfwSetFramebufferSizeCallback(state.win, winResizeCb);
    glViewport(0, 0, width, height);

    ma_result result = ma_engine_init(NULL, &state.soundEngine);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to initialize miniaudio.\n";
    } 

    lf_add_scroll_callback((void*)scrollCallback);
}

void initUI() {
    state.h1Font = lf_load_font("../game/fonts/inter-bold.ttf", 48);
    state.h2Font = lf_load_font("../game/fonts/inter-bold.ttf", 40);
    state.h3Font = lf_load_font("../game/fonts/inter-bold.ttf", 36);
    state.h4Font = lf_load_font("../game/fonts/inter-bold.ttf", 30);
    state.h5Font = lf_load_font("../game/fonts/inter-bold.ttf", 24);
    state.h6Font = lf_load_font("../game/fonts/inter-bold.ttf", 20);
    state.musicTitleFont = lf_load_font("../game/fonts/poppins.ttf", 74);
    state.poppinsBold = lf_load_font("../game/fonts/poppins.ttf", 30);

    state.backTexture = lf_tex_create("../game/textures/back.png", false, LF_TEX_FILTER_LINEAR);
    state.musicNoteTexture = lf_tex_create("../game/textures/music1.png", false, LF_TEX_FILTER_LINEAR);
}

void handleKeystrokes() {   
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
            lf_set_ptr_y(lf_get_ptr_y() - lf_text_dimension("Your Playlist").y - height / 2.0f);
            lf_pop_font();
            if(lf_button_fixed("Add Playlist", width, height) == LF_CLICKED) { 
                state.currentTab = GuiTab::CreatePlaylist;
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
                state.currentTab = GuiTab::CreatePlaylist;
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
               state.currentTab = GuiTab::OnPlaylist;
               state.currentPlaylist = i; 
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
    lf_div_begin((vec2s){DIV_START_X, DIV_START_Y}, (vec2s){(float)state.winWidth, (float)state.winHeight});
    lf_rect_render((vec2s){0, 0}, (vec2s){(float)state.winWidth, 160}, RGB_COLOR(12, 12, 12), RGBA_COLOR(0, 0, 0, 0), 0.0f, 0.0f);
    if(state.currentPlaylist == -1) return;

    auto& currentPlaylist = state.playlists[state.currentPlaylist];
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
    {
        lf_push_font(&state.h5Font);
        const char* text = "Add more Music";
        float textWidth = lf_text_dimension(text).x;
        LfUIElementProps props = lf_theme()->button_props;
        props.color = (vec4s){0, 0, 0, 0};
        props.text_color = (vec4s){1, 1, 1, 1};
        props.padding = 0;
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
    
    if(currentPlaylist.musicFiles.empty()) {
        // Text
        lf_set_ptr_y(100);
        {
            lf_push_font(&state.h5Font);
            const char* text = "There is no music in this playlist.";
            float textWidth = lf_text_dimension(text).x;
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
            lf_set_ptr_x((state.winWidth - 
                        (buttonWidth + props.padding * 2.0f) * 2.0f - (props.margin_right + props.margin_left) * 2.f) / 2.0f - DIV_START_X);
            lf_push_style_props(props);
            if(lf_button_fixed("Add from file", buttonWidth, 40) == LF_CLICKED) {
                state.currentTab = GuiTab::PlaylistAddFromFile;
            }
            if(lf_button_fixed("Add from Folder", buttonWidth, 40) == LF_CLICKED) {
                state.currentTab = GuiTab::PlaylistAddFromFolder;
            }
            lf_pop_style_props();
            lf_pop_font();
        }
    } else {
        lf_next_line();
        float ptr_y = lf_get_ptr_y();
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
        for(uint32_t i = 0; i < currentPlaylist.musicFiles.size(); i++) {
            const std::string& file = currentPlaylist.musicFiles[i];
            {
                std::filesystem::path fsPath(file);
                std::string filename = fsPath.filename().string();
                LfUIElementProps props = lf_theme()->text_props;
                props.margin_top = 20;
                props.margin_bottom = 20;
                lf_text(filename.c_str());
                lf_next_line();
            }
        }   
    }

    backButtonTo(GuiTab::Dashboard);

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
        props.margin_left = 15;
        props.margin_top = 15;
        props.border_width = 0;
        lf_push_style_props(props);
        if(lf_button_fixed("Load Files", 175, -1) == LF_CLICKED) {
            state.playlistAddFromFolderTab.loadedFolders.push_back((Folder){.path = std::string(state.playlistAddFromFolderTab.pathInput.buf), .files = loadFilesFromFolder(state.playlistAddFromFolderTab.pathInput.buf)}); 
        }
        lf_pop_style_props();
    }
    lf_next_line();
    // Folder Tabs
    {
        auto& selectedFolder = state.playlistAddFromFolderTab.loadedFolders[state.playlistAddFromFolderTab.folderIndex];
        for(auto& folder : state.playlistAddFromFolderTab.loadedFolders) {
            {
                std::filesystem::path folderPath = folder.path;
                std::string folderName = folderPath.filename();
                LfUIElementProps props = lf_theme()->button_props;
                props.border_width = 0;
                props.corner_radius = 3;
                props.margin_left = 35;
                props.margin_bottom = -3;
                props.color = RGB_COLOR(200, 200, 200);
                lf_push_style_props(props);
                lf_button_fixed(folderName.c_str(), 160, -1);
                lf_pop_style_props();
            }
            lf_next_line();
            {
                const uint32_t margin = 18;
                const uint32_t padding = 10;
                lf_rect_render((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y()}, (vec2s){(float)state.winWidth - ((DIV_START_X + margin) * 2), state.winHeight - lf_get_ptr_y() - 100}, RGB_COLOR(25, 25, 25), RGBA_COLOR(0, 0, 0, 0), 0.0f, 10.0f);
                for(auto& file : selectedFolder.files) {
                    std::filesystem::path filePath = file;
                    std::string fileName = filePath.filename();
                    LfUIElementProps props = lf_theme()->text_props;
                    props.margin_left = margin + padding;
                    props.margin_top = padding; 
                    props.margin_bottom = 12;
                    lf_push_style_props(props);
                    lf_set_cull_end_y(state.winHeight - 100 - lf_theme()->font.font_size);
                    lf_text(fileName.c_str());
                    lf_next_line();
                    lf_pop_style_props();
                    lf_set_cull_end_y(lf_get_div_size().y);
                }    
            }
        }
    }



    backButtonTo(GuiTab::OnPlaylist);

    lf_div_end();
}

void renderFileOrFolderPopup() {
    // Div
    LfUIElementProps props = lf_theme()->div_props;
    props.color = RGB_COLOR(25, 25, 25);
    props.padding = 20;
    props.corner_radius = 10;
    lf_push_style_props(props);
    lf_div_begin((vec2s){(state.winWidth - 400.0f) / 2.0f, (state.winHeight - 100.0f) / 2.0f}, (vec2s){400.0f, 100.0f});
    // Close Button
    {
        const char* text = "X";
        lf_set_ptr_y(0);
        lf_set_ptr_x(0);
        LfUIElementProps props = lf_theme()->button_props;
        props.margin_left = 0;
        props.margin_right = 0;
        props.margin_top = 0;
        props.margin_bottom = 0;
        props.text_color = RGB_COLOR(255, 255, 255);
        props.color = RGBA_COLOR(0, 0, 0, 0);
        props.border_width = 0;
        lf_push_style_props(props);
        if(lf_button(text) == LF_CLICKED) {
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
        LfUIElementProps props = lf_theme()->button_props;
        props.border_width = 0;
        props.corner_radius = 5;
        props.color = LYSSA_BLUE;
        lf_push_style_props(props);
        // Make the buttons stretch the entire div
        if(lf_button_fixed("From File", lf_get_div_size().x / 2.0f - props.padding * 2.0f - props.border_width * 2.0f  - (props.margin_left + props.margin_right), -1) == LF_CLICKED) {
            state.currentTab = GuiTab::PlaylistAddFromFile;
            state.popups[(int32_t)PopupID::FileOrFolderPopup].render = false;
        }
        if(lf_button_fixed("From Folder", lf_get_div_size().x / 2.0f - props.padding * 2.0f - props.border_width * 2.0f  - (props.margin_left + props.margin_right), -1) == LF_CLICKED) {
            state.currentTab = GuiTab::PlaylistAddFromFolder;
            state.popups[(int32_t)PopupID::FileOrFolderPopup].render = false;
        }
        lf_pop_style_props();
    }
    lf_div_end();
    lf_pop_style_props();
}

void backButtonTo(GuiTab tab) {
    const uint32_t marginBottom = 50, width = 20, height = 40;
    lf_next_line();

    float ptr_y = lf_get_ptr_y();

    lf_set_ptr_y(state.winHeight - marginBottom - height * 2);
    LfUIElementProps props = lf_theme()->button_props;
    props.color = (vec4s){0, 0, 0, 0};
    props.border_width = 0;
    lf_push_style_props(props);

    if(lf_image_button((LfTexture){.id = state.backTexture.id, .width = width, .height = height}) == LF_CLICKED) {
        state.currentTab = tab;
        loadPlaylists();
    }

    lf_pop_style_props();

    lf_set_ptr_y(ptr_y);
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

    return FileStatus::Success;
}
FileStatus addFileToPlaylist(const std::string& path, uint32_t playlistIndex) {
    Playlist& playlist = state.playlists[playlistIndex];

    if(std::find(playlist.musicFiles.begin(), playlist.musicFiles.end(), path) != playlist.musicFiles.end()) return FileStatus::AlreadyExists;

    std::ofstream metadata(playlist.path + "/.metadata", std::ios::app | std::ios::binary);

    if(!metadata.is_open()) return FileStatus::Failed;

    std::ifstream playlistFile(path);
    if(!playlistFile.good()) return FileStatus::Failed;

    metadata << path << " ";
    playlist.musicFiles.push_back(path);
    metadata.close();

    return FileStatus::Success;
}

void loadPlaylists() { 
     for (const auto& folder : std::filesystem::directory_iterator(LYSSA_DIR)) {
        if (folder.is_directory()) {
            bool alreadyLoaded = false;
            for(auto& playlist : state.playlists) {
                if(playlist.path == folder.path().string()) {
                    alreadyLoaded = true;
                    break;
                }
            }
            if(alreadyLoaded) continue;

            std::ifstream metadata(folder.path().string() + "/.metadata");

            if(!metadata.is_open()) {
                std::cerr << "Error opening the metadata of playlist on path: " << folder.path().string() << "\n";
                continue;
            }
            std::string name;
            std::vector<std::string> files;

            std::string line;
            while(std::getline(metadata, line)) {
                std::istringstream iss(line);
                std::string key;
                iss >> key;

                if(key == "name:") {
                      std::getline(iss, name);
                      name.erase(0, name.find_first_not_of(" \t"));
                      name.erase(name.find_last_not_of(" \t") + 1);
                } else if(key == "files:") {
                    std::string file;
                    while(iss >> file) {
                        files.push_back(file);
                        char comma;
                        iss >> comma;
                    }
                }
            }
            metadata.close();

            Playlist playlist;
            playlist.path = folder.path().string();
            playlist.name = name;
            playlist.musicFiles = files;

            state.playlists.push_back(playlist);
        }
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

int main(int argc, char* argv[]) {
    // Initialization 
    initWin(1280, 720); 
    initUI();

    char bufName[512] = {0};
    state.createPlaylistTab.nameInput = (LfInputField){
        .width = 600, 
        .buf = bufName, 
        .placeholder = (char*)"Name",
    };

    char bufPathFile[512] = {0};
    state.playlistAddFromFileTab.pathInput = (LfInputField){
        .width = 600, 
        .buf = bufPathFile, 
        .placeholder = (char*)"Path",
    };

    char bufPathFolder[512] = {0};
    state.playlistAddFromFolderTab.pathInput = (LfInputField){
        .width = 600, 
        .buf = bufPathFolder, 
        .placeholder = (char*)"Path",
    };

    if(!std::filesystem::exists(LYSSA_DIR)) { 
        std::filesystem::create_directory(LYSSA_DIR);
    }

    loadPlaylists();
    // Creating the popups
    state.popups.reserve((int32_t)PopupID::PopupCount);
    state.popups[(int32_t)PopupID::FileOrFolderPopup] = (Popup){.renderCb = renderFileOrFolderPopup, .render = false};

    while(!glfwWindowShouldClose(state.win)) { 
        // Delta-Time calculation
        float currentTime = glfwGetTime();
        state.deltaTime = currentTime - state.lastTime;
        state.lastTime = currentTime;

        // OpenGL color clearing 
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(LF_RGBA(0, 0, 0, 255));


        lf_begin();
        lf_div_begin((vec2s){0.0f, 0.0f}, (vec2s){(float)state.winWidth, (float)state.winHeight});

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
            case GuiTab::PlaylistAddFromFile:
                renderPlaylistAddFromFile();
                break;
            case GuiTab::PlaylistAddFromFolder:
                renderPlaylistAddFromFolder();
                break;
            default:
                lf_div_begin((vec2s){0, 0}, (vec2s){(float)state.winWidth, (float)state.winHeight});
                lf_text("Page not found");
                lf_div_end();
                break;
        }
        for(uint32_t i = 0; i < (uint32_t)PopupID::PopupCount; i++) {
            auto& popup = state.popups[i];
            if(popup.render) {
                popup.renderCb();
            }
        }
        lf_div_end();
        lf_end();
        
        glfwPollEvents();
        glfwSwapBuffers(state.win);

    }

    return 0;
} 
