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
    int32_t selectedFileIndex = -1;

    uint32_t fileScrollOffset = 0;
    uint32_t fileDisplayMax = 3;
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
    LfSlider volumeSlider;
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
static void             renderSoundProgressBar();
static void             renderVolumeSlider();

static void             updateSoundProgress();

static void             handleKeystrokes();

static void             playFileWithIndex(uint32_t i);

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
    ma_device_stop(&device);

    if(ma_decoder_seek_to_pcm_frame(&decoder, targetFrame) != MA_SUCCESS) {
        std::cerr << "Sound position in seconds invalid.\n";
    }

    ma_device_start(&device);
}

static int map_vals(int value, int from_min, int from_max, int to_min, int to_max) {
    return (value - from_min) * (to_max - to_min) / (from_max - from_min) + to_min;
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
    state.volumeSlider = (LfSlider){.val = &state.currentSound.volume, .min = 0, .max = 100, .width = 200, .height = 10};
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
        if(lf_button_fixed("Load", 150, -1) == LF_CLICKED) {            
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
    if(state.folderIndex == -1) return;
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
    std::vector<LfTextProps> textProps = {};
    uint32_t tabWidth = defaulTabWidth;
    for(auto& path : state.openFolders[state.folderIndex].files) {
        LfTextProps props = lf_text_render((vec2s){lf_get_ptr_x(), lf_get_ptr_y()}, path.c_str(), lf_theme()->font, text_wrap_point, -1, -1, -1, -1, true, RGB_COLOR(255, 255, 255));
        textProps.push_back(props);
        if(props.width > tabWidth) {
            tabWidth = props.width;
        }
    }
    lf_rect_render((vec2s){margin, lf_get_ptr_y()}, (vec2s){(float)tabWidth, 1}, (vec4s){RGB_COLOR(255, 255, 255)});
    lf_set_ptr_y(lf_get_ptr_y() + 1 + margin);
    SoundBuffer& buffer = state.openFolders[state.folderIndex];
    uint32_t iteratorEnd = buffer.files.size();
    if(iteratorEnd > buffer.fileDisplayMax) {
        iteratorEnd = buffer.fileDisplayMax;
    }
    for(uint32_t i = buffer.fileScrollOffset; i < iteratorEnd + buffer.fileScrollOffset; i++) { 
        std::string path = state.openFolders[state.folderIndex].files[i];
        if(lf_hovered((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y()}, (vec2s){(float)tabWidth + margin, (float)lf_theme()->font.font_size + margin}) && lf_mouse_button_went_down(GLFW_MOUSE_BUTTON_LEFT)) {
            if(buffer.selectedFileIndex == i) {
                playFileWithIndex(i);
            }
            else
                buffer.selectedFileIndex = i;
        }
        if(i == buffer.playingFileIndex) {
            lf_rect_render((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y()}, (vec2s){(float)tabWidth + margin, (float)lf_theme()->font.font_size + margin}, RGBA_COLOR(125, 125, 125, 255));
        } else if(i == buffer.selectedFileIndex) {
            lf_rect_render((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y()}, (vec2s){(float)tabWidth + margin, (float)lf_theme()->font.font_size + margin}, RGBA_COLOR(125, 125, 125, 125));
        }
        lf_text_render((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y() + margin}, path.c_str(), lf_theme()->font, text_wrap_point, -1, -1, -1, -1, false, RGB_COLOR(255, 255, 255));
        lf_set_ptr_y(lf_get_ptr_y() + lf_theme()->font.font_size + margin);
    }
}
void renderSoundProgressBar() {
    if(state.currentSound.isInit) {
        float ptr_x = lf_get_ptr_x();
        float ptr_y = lf_get_ptr_y();

        uint32_t progressBarX = (state.winWidth - state.soundProgressBar.width) * 0.5f;
        lf_set_ptr_x(progressBarX);
        lf_set_ptr_y(state.winHeight - state.soundProgressBar.height - lf_theme()->font.font_size * 2.0f);
        LfClickableItemState progress_bar = lf_progress_bar_int(&state.soundProgressBar);

        if(progress_bar == LF_CLICKED) {
            int32_t val = map_vals(lf_get_mouse_x(), 
                    progressBarX, progressBarX + state.soundProgressBar.width,
                    state.soundProgressBar.min, state.soundProgressBar.max);
            if(val != *(int32_t*)state.soundProgressBar.val && val >= 0 && val <= state.currentSound.lengthInSeconds) {
                *(int32_t*)state.soundProgressBar.val = val;            
                state.currentSound.setPositionInSeconds(*(int32_t*)state.soundProgressBar.val);
            }
        }
        std::stringstream ss;
        ss << (uint32_t)state.currentSound.getPositionInSeconds();
        ss << " / ";
        ss << (uint32_t)state.currentSound.lengthInSeconds << "s";
        std::string str = ss.str();
        lf_next_line();
        float textWidth = lf_text_dimension(str.c_str()).x;
        lf_next_line();
        lf_set_ptr_x((state.winWidth - textWidth) * 0.5f);
        lf_text(str.c_str());

        lf_set_ptr_x(ptr_x);
        lf_set_ptr_y(ptr_y);
    }
}

void renderVolumeSlider() {
    if(state.folderIndex == -1) return;
    float textWidth = lf_text_dimension("Volume").x;
    lf_set_ptr_y(10);
    lf_set_ptr_x(state.winWidth - state.volumeSlider.width - textWidth - 40);
    lf_text("Volume");
    lf_set_ptr_y(0);
    lf_set_ptr_x(state.winWidth - state.volumeSlider.width - 30);
    lf_slider_int(&state.volumeSlider);
}

void updateSoundProgress() {
    if(!state.currentSound.isInit) return;
    state.soundProgressUpdateTimer += state.deltaTime;
    if(state.soundProgressUpdateTimer >= state.soundProgressUpdateInterval) {
        *state.soundProgressPosition = state.currentSound.getPositionInSeconds();
        *(int32_t*)state.soundProgressBar.val = *state.soundProgressPosition;
    }
}

void handleKeystrokes() {
    if(state.folderIndex == -1) return;
    SoundBuffer& buffer = state.openFolders[state.folderIndex];
    if(lf_key_went_down(GLFW_KEY_DOWN)) {
        if(buffer.selectedFileIndex + 1 < buffer.fileDisplayMax) {
            buffer.selectedFileIndex++;
        } else {
            if(buffer.selectedFileIndex + 1 < buffer.files.size()) {
                buffer.fileScrollOffset++;
                buffer.selectedFileIndex++;
            }
        }
    }
    if(lf_key_went_down(GLFW_KEY_UP)) {
        if(buffer.fileScrollOffset != 0 && buffer.selectedFileIndex == buffer.fileScrollOffset && buffer.selectedFileIndex - 1 >= 0) {
            buffer.fileScrollOffset--;
            buffer.selectedFileIndex--;
        } else {
            if(buffer.selectedFileIndex - 1 >= 0)
                buffer.selectedFileIndex--;
        }
    }
    if(lf_key_went_down(GLFW_KEY_ENTER)) {
        if(buffer.selectedFileIndex != -1)
            playFileWithIndex(buffer.selectedFileIndex);
    }
    if(state.currentSound.isInit) {
        if(lf_key_went_down(GLFW_KEY_SPACE)) {
            if(state.currentSound.isPlaying) {
                state.currentSound.stop();
            } else {
                state.currentSound.play();
            }
        }
        if(lf_key_went_down(GLFW_KEY_LEFT)) {
            if(*(int32_t*)state.soundProgressBar.val - 5 >= 0) {
                *(int32_t*)state.soundProgressBar.val -= 5;
                state.currentSound.setPositionInSeconds(*(int32_t*)state.soundProgressBar.val);
            } else {
                *(int32_t*)state.soundProgressBar.val = 0;
                state.currentSound.setPositionInSeconds(*(int32_t*)state.soundProgressBar.val);
            }
        }
        if(lf_key_went_down(GLFW_KEY_RIGHT)) {
            if(*(int32_t*)state.soundProgressBar.val + 5 <= state.currentSound.lengthInSeconds) {
                *(int32_t*)state.soundProgressBar.val += 5;
                state.currentSound.setPositionInSeconds(*(int32_t*)state.soundProgressBar.val);
            } else {
                *(int32_t*)state.soundProgressBar.val = state.currentSound.lengthInSeconds;
                state.currentSound.setPositionInSeconds(*(int32_t*)state.soundProgressBar.val);
            }
        }
    }
    if(lf_key_went_down(GLFW_KEY_TAB)) {
        if(state.folderIndex + 1 < state.openFolders.size()) {
            state.folderIndex++;
        } else {
            state.folderIndex = 0;
        }
    }
}
void playFileWithIndex(uint32_t i) {
    SoundBuffer& buffer = state.openFolders[state.folderIndex];
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
        renderVolumeSlider();
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, 200, 200);

        renderSoundProgressBar();
        glDisable(GL_SCISSOR_TEST);

        lf_flush();
        lf_renderer_begin();
        // Updating State
        handleKeystrokes();
        updateSoundProgress();



        // Flush
        lf_div_end();
        lf_end();
        
        glfwPollEvents();
        glfwSwapBuffers(state.win);

    }

    return 0;
} 
