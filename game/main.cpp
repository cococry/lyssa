#include <fstream>
#include <iostream>
#include <filesystem>
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

enum class GuiTab {
    Dashboard = 0, 
    CreatePlaylist
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
};

struct CreatePlaylistState {
    LfInputField nameInput, descInput;  

    FileStatus createFileStatus;
    float createFileMessageShowTime = 3.0f; 
    float createFileMessageTimer = 0.0f;
};

struct GlobalState {
    GLFWwindow* win;
    uint32_t winWidth, winHeight;
    float deltaTime, lastTime;

    ma_engine soundEngine;
    Sound currentSound;

    LfFont h1Font;
    LfFont h2Font;
    LfFont h3Font;
    LfFont h4Font;
    LfFont h5Font;
    LfFont h6Font;

    GuiTab currentTab;

    std::vector<Playlist> playlists;

    LfTexture backTexture;

    CreatePlaylistState createPlaylistTab;
};

static GlobalState state;

static void             winResizeCb(GLFWwindow* window, int32_t width, int32_t height);
static void             initWin(uint32_t width, uint32_t height);
static void             initUI();
static void             handleKeystrokes();

static void             loadPlaylists();

static void             renderDashboard();
static void             renderCreatePlaylist();

static FileStatus        createPlaylist(const std::string& name, const std::string desc);

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

    state.win = glfwCreateWindow(width, height, "ZynEd" ,NULL, NULL);
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
    state.h4Font = lf_load_font("../game/fonts/inter-bold.ttf", 32);
    state.h5Font = lf_load_font("../game/fonts/inter-bold.ttf", 30);
    state.h6Font = lf_load_font("../game/fonts/inter-bold.ttf", 28);

    state.backTexture = lf_tex_create("../game/textures/back.png", false, LF_TEX_FILTER_LINEAR);

}

void handleKeystrokes() {   
}

void loadPlaylists() { 
    if(!std::filesystem::exists(LYSSA_DIR)) { 
        std::filesystem::create_directory(LYSSA_DIR);
    }

     for (const auto& entry : std::filesystem::directory_iterator(LYSSA_DIR)) {
        if (entry.is_directory()) {
            std::vector<std::string> files;
            for (const auto& file : std::filesystem::directory_iterator(entry.path())) {
                if (file.is_regular_file()) {
                    files.push_back(file.path().string());
                }
            }
            state.playlists.push_back((Playlist){.musicFiles = files});
        }
    }
}

void renderDashboard() {

    lf_div_begin((vec2s){20, 50}, (vec2s){(float)state.winWidth, (float)state.winHeight});

    lf_push_font(&state.h1Font);
    lf_text("Your Playlists");
    lf_pop_font();

    lf_next_line();
    if(state.playlists.empty()) {
        {
            const char* text = "You don't have any playlists.";
            float textWidth = lf_text_dimension(text).x;
            lf_set_ptr_x((state.winWidth - textWidth) / 2.0f - 20);
            LfUIElementProps props = lf_theme()->text_props;
            props.margin_top = 40;
            props.margin_left = 0;
            lf_push_style_props(props);
            lf_text("You dont have any playlists.");
            lf_pop_style_props();
        }
        lf_next_line();
        {
            lf_set_ptr_x((state.winWidth - (200 + (lf_theme()->button_props.padding * 2))) / 2.0f - 20);
            LfUIElementProps props = lf_theme()->button_props;
            props.color = LYSSA_GREEN;
            props.border_width = 0;
            props.corner_radius = 15;
            props.margin_top = 20;
            props.margin_left = 0;
            lf_push_style_props(props);
            if(lf_button_fixed("Create Playlist", 200, 50) == LF_CLICKED)  {
                state.currentTab = GuiTab::CreatePlaylist;
            }
            lf_pop_style_props();
        }
    }
    lf_div_end();
}

void renderCreatePlaylist() {
    lf_div_begin((vec2s){20, 50}, (vec2s){(float)state.winWidth, (float)state.winHeight});
    
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
        lf_input_text(&state.createPlaylistTab.descInput);
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
           state.createPlaylistTab.createFileStatus = createPlaylist(std::string(state.createPlaylistTab.nameInput.buf), std::string(state.createPlaylistTab.descInput.buf));
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
    // Back Button
    {
        lf_next_line();
        float ptr_y = lf_get_ptr_y();
        lf_set_ptr_y(state.winHeight - 50 - 40 * 2);
        LfUIElementProps props = lf_theme()->button_props;
        props.color = (vec4s){0, 0, 0, 0};
        props.border_width = 0;
        lf_push_style_props(props);
        if(lf_image_button((LfTexture){.id = state.backTexture.id, .width = 20, .height = 40}) == LF_CLICKED) {
            state.currentTab = GuiTab::Dashboard;
        }
        lf_pop_style_props();
        lf_set_ptr_y(ptr_y);
    }
    lf_div_end();
}

FileStatus createPlaylist(const std::string& name, const std::string desc) {
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
        metadata << "desc: " << desc << "\n";
    } else {
        return FileStatus::Failed;
    }
    metadata.close();

    return FileStatus::Success;
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

    char bufDesc[512] = {0};
    state.createPlaylistTab.descInput = (LfInputField){
        .width = 600, 
        .height = 150,
        .buf = bufDesc, 
        .placeholder = (char*)"Description", 
        .expand_on_overflow = true, 
    };


    loadPlaylists();

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


        lf_text("Hello\nBye");
        std::cout << state.createPlaylistTab.descInput.buf << "\n";
        switch(state.currentTab) {
            case GuiTab::Dashboard:
                renderDashboard();
                break;
            case GuiTab::CreatePlaylist:
                renderCreatePlaylist();
                break;
        }
        lf_div_end();
        lf_end();
        
        glfwPollEvents();
        glfwSwapBuffers(state.win);

    }

    return 0;
} 
