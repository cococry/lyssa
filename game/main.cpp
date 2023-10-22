#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define MINIAUDIO_IMPLEMENTATION
#include "../vendor/miniaudio/miniaudio.h"
#include "../leif.h"

#define RGB_COLOR(r, g, b) (vec4s){LF_RGBA(r, g, b, 255.0f)}
#define RGBA_COLOR(r, g, b, a) (vec4s){LF_RGBA(r, g, b, a)}

struct SoundBuffer {
    std::string path;
    std::vector<std::string> files;
    int32_t playingFileIndex = -1;
};

struct Sound {
    ma_device device;
    ma_decoder decoder;
    bool isPlaying = false, isInit = false;
    double lengthInSeconds = 0;

    void init(const std::string& filepath);
    double getPositionInSeoconds();
    void uninit();
    void play();
    void stop();
};
struct GlobalState {
    GLFWwindow* win;
    uint32_t winWidth, winHeight;
    float deltaTime, lastTime;
    int32_t folderIndex = -1;
    std::vector<SoundBuffer> openFolders = {};
    LfInputField pathInput;
    
    ma_engine soundEngine;

    LfFont headingFont;

    Sound currentSound;
    LfSlider soundProgressBar;
    int32_t* soundProgressPosition;
    float soundProgressUpdateTimer = 0.0f;
    float soundProgressUpdateInterval = 1.0f;
};

static GlobalState state;

static void             winResizeCb(GLFWwindow* window, int32_t width, int32_t height);
static void             initWin(uint32_t width, uint32_t height);

static void             renderFolderInputMenu();
static void             renderFolderTabs();
static void             renderFilesInCurrentFolder();
static void             updateSoundProgress();

static void miniaudioDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ma_decoder_read_pcm_frames(&state.currentSound.decoder, pOutput, frameCount, NULL);
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

double Sound::getPositionInSeoconds() {
    ma_uint64 cursorInFrames;
    ma_decoder_get_cursor_in_pcm_frames(&decoder, &cursorInFrames);
    return (double)cursorInFrames / decoder.outputSampleRate;
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
    LfTheme theme = lf_default_theme("../game/fonts/arial.ttf", 24);
    theme.div_props.color = (vec4s){LF_RGBA(0, 0, 0, 0)};
    lf_init_glfw(width, height, "../game/fonts/arial.ttf", &theme, state.win);   
    lf_set_text_wrap(true);
    glfwSetFramebufferSizeCallback(state.win, winResizeCb);
    glViewport(0, 0, width, height);

    ma_result result = ma_engine_init(NULL, &state.soundEngine);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed to initialize miniaudio.\n";
    }

    state.headingFont = lf_load_font("../game/fonts/poppins.ttf", 28);


    state.soundProgressPosition = (int32_t*)malloc(sizeof(int32_t));
    *state.soundProgressPosition = 0;
    state.soundProgressBar = (LfSlider){.val = state.soundProgressPosition, .min = 0, .max = 100, .width = 600, .height = 20};
}
void renderFolderInputMenu() {
    {
        LfUIElementProps props = lf_theme()->text_props;
        props.padding = 10;
        lf_push_style_props(props);
        lf_text("Sound Folder:");
        lf_pop_style_props();
    }
    {
        LfUIElementProps props = lf_theme()->inputfield_props;
        props.border_width = 2;
        lf_push_style_props(props);
        lf_input_text(&state.pathInput);
        lf_pop_style_props();
    }
    {
        LfUIElementProps props = lf_theme()->button_props;
        props.border_width = 2;
        lf_push_style_props(props);
        if(lf_button_fixed("Submit", 150, -1) == LF_CLICKED) {            
            SoundBuffer buffer = {.path = std::string(state.pathInput.buf), .files = {}};
            for (const auto & entry : std::filesystem::directory_iterator(std::string(state.pathInput.buf))) {
                if (std::filesystem::is_regular_file(entry.path())) {
                    buffer.files.push_back(entry.path().filename().string());
                }
            }

            state.openFolders.push_back(buffer);
            state.folderIndex++;
        }
        lf_pop_style_props();
    }

}
void renderFolderTabs() {
    if(state.folderIndex != -1) {
        std::vector<const char*> openFoldersCstr = {};
        for (uint32_t i = 0; i < state.openFolders.size(); i++) {
            openFoldersCstr.push_back(state.openFolders[i].path.c_str());
        }
        lf_next_line();
        uint32_t clickedIndex = lf_menu_item_list(openFoldersCstr.data(), openFoldersCstr.size(), state.folderIndex, RGB_COLOR(200, 200, 200), [](uint32_t* index){}, false);
        if(clickedIndex != -1) {
            state.folderIndex = clickedIndex;
        }
    }
}
void renderFilesInCurrentFolder() {
    if(state.folderIndex != -1) {
        const uint32_t margin = 10;
        const uint32_t text_wrap_point = 500;
        const uint32_t defaulTabWidth = 200;
        lf_next_line();
        LfUIElementProps props = lf_theme()->text_props;
        props.margin_left = 10;
        props.margin_top = 15;
        lf_push_style_props(props);
        lf_push_font(&state.headingFont);
        lf_text("Files");
        lf_pop_style_props();
        lf_pop_font();
        lf_next_line();
        lf_rect_render((vec2s){margin, lf_get_ptr_y()}, (vec2s){defaulTabWidth, 1}, (vec4s){RGB_COLOR(255, 255, 255)});
        lf_set_ptr_y(lf_get_ptr_y() + 1 + margin);
        std::vector<LfTextProps> textProps = {};
        uint32_t tabWidth = defaulTabWidth;
        for(auto& path : state.openFolders[state.folderIndex].files) {
            LfTextProps props = lf_text_render((vec2s){lf_get_ptr_x(), lf_get_ptr_y()}, path.c_str(), lf_theme()->font, text_wrap_point, -1, -1, -1, -1, true, RGB_COLOR(255, 255, 255));
            textProps.push_back(props);
            if(props.width > tabWidth) {
                tabWidth = props.width;
            }
        }
        for(uint32_t i = 0; i < state.openFolders[state.folderIndex].files.size(); i++) { 
            auto& buffer = state.openFolders[state.folderIndex];
            auto& path = state.openFolders[state.folderIndex].files[i];
            if(lf_hovered((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y()}, (vec2s){(float)tabWidth + margin, (float)lf_theme()->font.font_size + margin}) && lf_mouse_button_went_down(GLFW_MOUSE_BUTTON_LEFT)) {
                buffer.playingFileIndex = i;
                if(state.currentSound.isPlaying) {
                    state.currentSound.stop();
                }
                if(state.currentSound.isInit) {
                    state.currentSound.uninit();
                }
                state.currentSound.init(buffer.path + "/" + buffer.files[buffer.playingFileIndex]);
                state.currentSound.play();
                state.soundProgressBar.max = state.currentSound.lengthInSeconds;
            }
            if(i == buffer.playingFileIndex) {
                lf_rect_render((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y()}, (vec2s){(float)tabWidth + margin, (float)lf_theme()->font.font_size + margin}, RGB_COLOR(125, 125, 125));
            }
            lf_text_render((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y() + margin}, path.c_str(), lf_theme()->font, text_wrap_point, -1, -1, -1, -1, false, RGB_COLOR(255, 255, 255));
            lf_set_ptr_y(lf_get_ptr_y() + lf_theme()->font.font_size + margin);
        }
        std::stringstream ss;
        ss << state.currentSound.lengthInSeconds;
        std::string str = ss.str();
        lf_text(str.c_str());
    }
}

void updateSoundProgress() {
    if(state.folderIndex != -1) {
        state.soundProgressUpdateTimer += state.deltaTime;
        if(state.soundProgressUpdateTimer >= state.soundProgressUpdateInterval) {
            *state.soundProgressPosition = state.currentSound.getPositionInSeoconds();
            *(int32_t*)state.soundProgressBar.val = *state.soundProgressPosition;
        }
    }
}
int main(int argc, char* argv[]) {
    // Initialization
    initWin(1280, 720); 
    char buf[512] = {0};
    state.pathInput = (LfInputField){.width = 400, .buf = buf};

    while(!glfwWindowShouldClose(state.win)) { 
        // Delta-Time calculation
        float currentTime = glfwGetTime();
        state.deltaTime = currentTime - state.lastTime;
        state.lastTime = currentTime;

        // OpenGL color clearing 
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(LF_RGBA(25, 25, 25, 255));

        lf_begin();
        lf_div_begin((vec2s){0.0f, 0.0f}, (vec2s){(float)state.winWidth, (float)state.winHeight});

        renderFolderInputMenu();
        renderFolderTabs();
        renderFilesInCurrentFolder();
        lf_progress_bar_int(&state.soundProgressBar);
        updateSoundProgress();
        lf_div_end();

        // Flush
        lf_end();
        glfwPollEvents();
        glfwSwapBuffers(state.win);
    }

    return 0;
} 
