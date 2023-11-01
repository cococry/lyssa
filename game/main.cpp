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
    int32_t playingPlaylistFileIndex = -1;
    int32_t selectedFileIndex = -1;

    int32_t selectedPlaylistIndex = -1;

    uint32_t fileScrollOffset = 0;
    uint32_t playlistScrollOffset = 0;

    uint32_t fileDisplayMax = 8;

    void moveFileIndexUp(uint32_t n);
    void moveFileIndexDown(uint32_t n);
    void moveFileIndexToTop();

    std::vector<std::string> playlist;

    bool onPlaylistTab = false;
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

    void rewindSeconds(uint32_t seconds);
    void fastForewardSeconds(uint32_t seconds);
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

    LfTexture enterTexture, playTexture, pauseTexture, skipUpTexture, skipDownTexture,
              skipSoundUpTexture, skipSoundDownTexture;
};

static GlobalState state;

static void             winResizeCb(GLFWwindow* window, int32_t width, int32_t height);
static void             initWin(uint32_t width, uint32_t height);

static void             renderFolderInputMenu();
static void             renderFolderTabs();
static void             renderFilesInCurrentFolder();
static void             renderSoundControls();
static void             renderSoundProgressBar();
static void             renderVolumeSlider();

static void             updateSoundProgress();

static void             handleKeystrokes();

static void             playFileWithIndex(uint32_t i);

template<typename T>
static bool elementInVector(const std::vector<T>& v, const T& e) {
    for(auto& element : v) 
        if(element == e) 
            return true;
    return false;
}

static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if(state.folderIndex == -1) return;
    if(yoffset == -1) {
        state.openFolders[state.folderIndex].moveFileIndexDown(1);
    } else if(yoffset == 1) {
        state.openFolders[state.folderIndex].moveFileIndexUp(1);
    }
}

static void miniaudioDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ma_decoder_read_pcm_frames(&state.currentSound.decoder, pOutput, frameCount, NULL);

    // Adjust volume
    float* outputBuffer = (float*)pOutput;
    for (ma_uint32 i = 0; i < frameCount * pDevice->playback.channels; ++i) {
        outputBuffer[i] *= state.currentSound.volume / 100.0f;
    }
}

void SoundBuffer::moveFileIndexUp(uint32_t n) {
    if(!onPlaylistTab) {
        if((int32_t)(selectedFileIndex - n) >= 0) {
            if(selectedFileIndex == fileScrollOffset) {
                fileScrollOffset -= n;
            }
            selectedFileIndex -= n;
        }
    } else {
        if((int32_t)(selectedPlaylistIndex - n) >= 0) {
            if(selectedPlaylistIndex == playlistScrollOffset) {
                playlistScrollOffset -= n;
            }
            selectedPlaylistIndex -= n;
        }
    }

}
void SoundBuffer::moveFileIndexDown(uint32_t n) {
    if(!onPlaylistTab) {
        if(selectedFileIndex + n < files.size()) {
            selectedFileIndex += n;
            if(selectedFileIndex == fileDisplayMax + fileScrollOffset) {
                fileScrollOffset += n;
            }
        } else {
            moveFileIndexToTop();
        }
    } else {
        if(selectedPlaylistIndex + n < playlist.size()) {
            selectedPlaylistIndex += n;
            if(selectedPlaylistIndex == fileDisplayMax + playlistScrollOffset) {
                playlistScrollOffset += n;
            }
        } else {
            moveFileIndexToTop();
        }
    }
}
void SoundBuffer::moveFileIndexToTop() {
    if(!onPlaylistTab) {
        selectedFileIndex = 0;
        fileScrollOffset = 0;
    } else {
        selectedPlaylistIndex = 0;
        playlistScrollOffset = 0;
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

void Sound::rewindSeconds(uint32_t seconds) {
    if((int32_t)(*(int32_t*)state.soundProgressBar.val - seconds) >= 0) {
        *(int32_t*)state.soundProgressBar.val -= seconds;
        setPositionInSeconds(*(int32_t*)state.soundProgressBar.val);
    } else {
        *(int32_t*)state.soundProgressBar.val = 0;
        setPositionInSeconds(*(int32_t*)state.soundProgressBar.val);
    }
    if(!isPlaying) {
        stop();
    }
}
void Sound::fastForewardSeconds(uint32_t seconds) {
    if(*(int32_t*)state.soundProgressBar.val + seconds <= state.currentSound.lengthInSeconds) {
        *(int32_t*)state.soundProgressBar.val += seconds;
        setPositionInSeconds(*(int32_t*)state.soundProgressBar.val);
    } else {
        *(int32_t*)state.soundProgressBar.val = state.currentSound.lengthInSeconds;
        setPositionInSeconds(*(int32_t*)state.soundProgressBar.val);
    }
    if(!isPlaying) {
        stop();
    }
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
    state.soundProgressBar = (LfSlider){.val = state.soundProgressPosition, .min = 0, .max = 100, .width = 600, .height = 5};
    state.volumeSlider = (LfSlider){.val = &state.currentSound.volume, .min = 0, .max = 100, .width = 200, .height = 10};

    lf_add_scroll_callback((void*)scrollCallback);
}
void renderFolderInputMenu() {
    float width = state.pathInput.width + 
        lf_theme()->inputfield_props.padding * 2 + 10 + 10 + 17 + lf_theme()->button_props.padding * 3;
    float ptr_x = lf_get_ptr_x();
    lf_set_ptr_x((state.winWidth - width) * 0.5f);
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
        if(lf_image_button((LfTexture){.id = state.enterTexture.id, .width = 17, .height = 17}) == LF_CLICKED) {            
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
    lf_set_ptr_x(ptr_x);
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
    SoundBuffer& buffer = state.openFolders[state.folderIndex];

    const uint32_t margin = 10;
    const uint32_t textWrapPoint = 500;
    const uint32_t defaulTabWidth = 200;

    uint32_t tabWidth = defaulTabWidth; 
    std::vector<float> textWidths = {};
    for(auto& path : state.openFolders[state.folderIndex].files) {
        vec2s textDim = lf_text_dimension(path.c_str());
        textWidths.push_back(textDim.x);
        if(textDim.x > tabWidth) {
            tabWidth = textDim.x;
        }
    }
    {
        lf_next_line();
        LfUIElementProps props = lf_theme()->text_props;
        props.margin_left = 10;
        props.margin_top = 15;
        props.padding = 10; 
        vec2s textDim1 = lf_text_dimension("Files");
        vec2s textDim2 = lf_text_dimension("Playlist");
        lf_set_text_color((vec4s){1, 1, 1, 1});
        lf_push_style_props(props);
        lf_push_font(&state.headingFont);

        lf_set_item_color(buffer.onPlaylistTab ? (vec4s){0, 0, 0, 0} : (vec4s){1, 1, 1, 0.3});
        if(lf_button_fixed("Files", 150, -1) == LF_CLICKED) {
            buffer.onPlaylistTab = false;
        }
        lf_unset_item_color();

        lf_set_item_color(buffer.onPlaylistTab ? (vec4s){1, 1, 1, 0.3} : (vec4s){0, 0, 0, 0});
        if(lf_button_fixed("Playlist", 150, -1) == LF_CLICKED) {
            buffer.onPlaylistTab = true;
        }
        lf_unset_item_color();
        lf_unset_text_color();
        lf_pop_style_props();
        lf_pop_font();
    }
    lf_next_line();
    lf_rect_render((vec2s){margin, lf_get_ptr_y()}, (vec2s){(float)tabWidth, 1}, (vec4s){RGB_COLOR(255, 255, 255)}, (vec4s){0.0f, 0.0f, 0.0f, 0.0f}, 0.0f);
    lf_set_ptr_y(lf_get_ptr_y() + 1 + margin);
    uint32_t iteratorEnd;
    if(buffer.onPlaylistTab) 
        iteratorEnd = buffer.playlist.size();
    else 
        iteratorEnd = buffer.files.size();
    if(iteratorEnd > buffer.fileDisplayMax) {
        iteratorEnd = buffer.fileDisplayMax;
    }
    for(uint32_t i = ((buffer.onPlaylistTab) ? buffer.playlistScrollOffset : buffer.fileScrollOffset); i < iteratorEnd + 
            (buffer.onPlaylistTab ? buffer.playlistScrollOffset : buffer.fileScrollOffset); i++) { 
        float ptr_x = lf_get_ptr_x();
        float ptr_y = lf_get_ptr_y();
        std::string path;
        if(!buffer.onPlaylistTab)
            path = buffer.files[i];
        else
            path = buffer.playlist[i];
        if(lf_hovered((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y()}, (vec2s){(float)tabWidth + margin * 5, (float)lf_theme()->font.font_size + margin}) && lf_mouse_move_happend()) {
            if(!buffer.onPlaylistTab)
                buffer.selectedFileIndex = i;
            else 
                buffer.selectedPlaylistIndex = i;
        } else if(lf_hovered((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y()}, (vec2s){(float)tabWidth + margin, (float)lf_theme()->font.font_size + margin}) && lf_mouse_button_went_down(GLFW_MOUSE_BUTTON_LEFT)) {
            if(((buffer.onPlaylistTab) ? buffer.selectedPlaylistIndex : buffer.selectedFileIndex) == i) {
                if(buffer.onPlaylistTab)
                    buffer.playingFileIndex = -1;
                else 
                    buffer.playingPlaylistFileIndex = -1;
                playFileWithIndex(i);
            }
        }
        if((buffer.playingFileIndex == i && !buffer.onPlaylistTab) || (buffer.playingPlaylistFileIndex == i && buffer.onPlaylistTab)) {
            lf_rect_render((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y()}, (vec2s){(float)tabWidth + margin, (float)lf_theme()->font.font_size + margin}, RGBA_COLOR(125, 125, 125, 255), (vec4s){0.0f, 0.0f, 0.0f, 0.0f}, 0.0f);
        } else if((buffer.onPlaylistTab ? buffer.selectedPlaylistIndex : buffer.selectedFileIndex) == i) {
            lf_rect_render((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y()}, (vec2s){(float)tabWidth + margin, (float)lf_theme()->font.font_size + margin}, RGBA_COLOR(125, 125, 125, 125), (vec4s){0.0f, 0.0f, 0.0f, 0.0f}, 0.0f);
        }
        lf_text_render((vec2s){lf_get_ptr_x() + margin, lf_get_ptr_y() + margin}, path.c_str(), lf_theme()->font, textWrapPoint, -1, -1, -1, -1, false, RGB_COLOR(255, 255, 255));
        lf_set_ptr_x(tabWidth + margin * 2);
        if((buffer.onPlaylistTab ? buffer.selectedPlaylistIndex : buffer.selectedFileIndex) == i) {
            LfUIElementProps props = lf_theme()->button_props;
            props.margin_top = 0;
            lf_push_style_props(props);
            if(!buffer.onPlaylistTab) {
                if(!elementInVector(buffer.playlist, path)) {
                    if(lf_button_fixed("+", 8, 8) == LF_CLICKED) {
                            buffer.playlist.push_back(path);
                    }
                } else {
                    LfUIElementProps props = lf_theme()->text_props;
                    props.text_color = RGB_COLOR(23, 222, 26);
                    lf_push_style_props(props);
                    lf_text("Added");
                    lf_pop_style_props();
                }
            } else {
                if(lf_button_fixed("X", 8, 8) == LF_CLICKED) {
                    buffer.playlist.erase(buffer.playlist.begin() + i);
                    if(i == buffer.playingPlaylistFileIndex) {
                        state.currentSound.stop();
                        state.currentSound.uninit();
                        buffer.playingPlaylistFileIndex = -1;
                    }
                }
            }
            lf_pop_style_props();
        }
        lf_set_ptr_x(ptr_x);
        lf_set_ptr_y(lf_get_ptr_y() + lf_theme()->font.font_size + margin);
    }
}

void renderSoundControls() {
    if(!state.currentSound.isInit) return;

    const uint32_t button_size = 25;
    const uint32_t button_size_minor = 35;
    const uint32_t button_margin = 50;
    const uint32_t button_margin_bottom = 10;

    lf_set_ptr_x((state.winWidth - (button_size_minor + button_margin + 
                                    button_size + button_margin + 
                                    button_size + button_margin + 
                                    button_size + button_margin + 
                                    button_size_minor)) * 0.5f);
    lf_set_ptr_y(state.winHeight - state.soundProgressBar.height - (lf_theme()->font.font_size * 2.0f + button_size + button_margin_bottom));

    LfUIElementProps props = lf_theme()->button_props;
    props.padding = 0;
    props.margin_left = 0;
    props.margin_right = 0;
    props.border_width = 0;
    props.border_color = RGBA_COLOR(0, 0, 0, 0);
    lf_push_style_props(props);
    lf_set_item_color((vec4s){0, 0, 0, 0});

    if(lf_image_button((LfTexture){.id = state.skipSoundDownTexture.id, .width = button_size_minor, .height = button_size}) == LF_CLICKED) {
        SoundBuffer& buffer = state.openFolders[state.folderIndex];
        buffer.moveFileIndexUp(1);
        playFileWithIndex(buffer.onPlaylistTab ? buffer.selectedPlaylistIndex : buffer.selectedFileIndex);
    }
    lf_set_ptr_x(lf_get_ptr_x() + button_margin);

    if(lf_image_button((LfTexture){.id = state.skipDownTexture.id, .width = button_size, .height = button_size}) == LF_CLICKED) {
        state.currentSound.rewindSeconds(5);
    }

    lf_set_ptr_x(lf_get_ptr_x() + button_margin);

    if(lf_image_button((LfTexture){.id = (state.currentSound.isPlaying ? state.pauseTexture.id : state.playTexture.id), .width = button_size, .height = button_size}) == LF_CLICKED) {
        if(state.currentSound.isPlaying) 
            state.currentSound.stop();
        else 
            state.currentSound.play();
    }

    lf_set_ptr_x(lf_get_ptr_x() + button_margin);

    if(lf_image_button((LfTexture){.id = state.skipUpTexture.id, .width = button_size, .height = button_size}) == LF_CLICKED) {
        state.currentSound.fastForewardSeconds(5);
    } 
    lf_set_ptr_x(lf_get_ptr_x() + button_margin);

    if(lf_image_button((LfTexture){.id = state.skipSoundUpTexture.id, .width = button_size_minor, .height = button_size}) == LF_CLICKED) {
        SoundBuffer& buffer = state.openFolders[state.folderIndex];
        buffer.moveFileIndexDown(1);
        playFileWithIndex(buffer.onPlaylistTab ? buffer.selectedPlaylistIndex : buffer.selectedFileIndex);
    }

    lf_next_line();
    lf_unset_item_color();
    lf_pop_style_props();
}
void renderSoundProgressBar() {
    if(!state.currentSound.isInit) return;
    float ptr_x = lf_get_ptr_x();
    float ptr_y = lf_get_ptr_y();

    LfClickableItemState progressBar; 
    uint32_t progressBarX = (state.winWidth - state.soundProgressBar.width - lf_theme()->slider_props.border_width) * 0.5f;
    lf_set_ptr_x(progressBarX);
    lf_set_ptr_y(state.winHeight - state.soundProgressBar.height - lf_theme()->font.font_size * 2.0f);
    {
        LfUIElementProps props = lf_theme()->slider_props;
        props.margin_left = 0;
        props.margin_right = 0;
        props.border_width = 0;
        props.text_color = (vec4s){RGB_COLOR(0, 158, 100)};
        lf_set_item_color((vec4s){0.75, 0.75, 0.75, 1});
        lf_push_style_props(props);
        progressBar = lf_progress_stripe_int(&state.soundProgressBar);
        lf_pop_style_props();
        lf_unset_item_color();
    }
    if(progressBar == LF_CLICKED) {
        state.soundProgressBar.held = true;
        state.currentSound.stop();
    }
    if(lf_mouse_button_is_released(GLFW_MOUSE_BUTTON_LEFT) && state.soundProgressBar.held) {
        state.soundProgressBar.held = false;
        state.currentSound.play();
    }
    if(state.soundProgressBar.held) {
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

void renderVolumeSlider() {
    if(state.folderIndex == -1) return;
    lf_set_ptr_y(0);
    lf_set_ptr_x(state.winWidth - state.volumeSlider.width - 30);
    lf_slider_int(&state.volumeSlider);
}

void updateSoundProgress() {
    if(!state.currentSound.isInit || !state.currentSound.isPlaying) return;
    state.soundProgressUpdateTimer += state.deltaTime;
    if(state.soundProgressUpdateTimer >= state.soundProgressUpdateInterval) {
        *state.soundProgressPosition = state.currentSound.getPositionInSeconds();
        *(int32_t*)state.soundProgressBar.val += 1.0f;
    }
}

void handleKeystrokes() {
    if(state.folderIndex == -1) return;
    SoundBuffer& buffer = state.openFolders[state.folderIndex];
    if(lf_key_went_down(GLFW_KEY_DOWN)) {
        if(lf_key_is_down(GLFW_KEY_LEFT_SHIFT)) {
            if((int32_t)(state.currentSound.volume - 5) >= 0) {
                state.currentSound.volume -= 5;
                *(int32_t*)state.volumeSlider.val -= 5;
                state.volumeSlider._init = false;
            } else {
                state.currentSound.volume = 0;
                *(int32_t*)state.volumeSlider.val = 0;
                state.volumeSlider._init = false;
            }
        } else {
            buffer.moveFileIndexDown(1);
        }
    }
    if(lf_key_went_down(GLFW_KEY_UP)) {
        if(lf_key_is_down(GLFW_KEY_LEFT_SHIFT)) {
            if(state.currentSound.volume + 5 <= 100) {
                state.currentSound.volume += 5;
                *(int32_t*)state.volumeSlider.val += 5;
                state.volumeSlider._init = false;
            } else {
                state.currentSound.volume = 100;
                *(int32_t*)state.volumeSlider.val = 100;
                state.volumeSlider._init = false;
            }
        } else {
            buffer.moveFileIndexUp(1);
        }
    }
    if(lf_key_went_down(GLFW_KEY_ENTER)) {
        if(!buffer.onPlaylistTab) {
            if(buffer.selectedFileIndex != -1) 
                playFileWithIndex(buffer.selectedFileIndex);
        } else {
            if(buffer.selectedPlaylistIndex != -1) 
                playFileWithIndex(buffer.selectedPlaylistIndex);
        }
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
            state.currentSound.rewindSeconds(5);
        }
        if(lf_key_went_down(GLFW_KEY_RIGHT)) {
            state.currentSound.fastForewardSeconds(5);
        }
    }
    if(lf_key_went_down(GLFW_KEY_TAB)) {
        if(state.folderIndex + 1 < state.openFolders.size()) {
            state.folderIndex++;
        } else {
            state.folderIndex = 0;
        }
    } 
    if(lf_key_went_down(GLFW_KEY_N)) {
        if(buffer.onPlaylistTab)
            buffer.selectedPlaylistIndex = buffer.playingPlaylistFileIndex;
        else 
            buffer.selectedFileIndex = buffer.playingFileIndex;
        if(lf_key_is_down(GLFW_KEY_LEFT_SHIFT)) {
            buffer.moveFileIndexUp(1);
        } else {
            buffer.moveFileIndexDown(1);
        }
        playFileWithIndex(buffer.onPlaylistTab ? buffer.selectedPlaylistIndex  : buffer.selectedFileIndex);
    }
    if(lf_key_went_down(GLFW_KEY_A) && !elementInVector(buffer.playlist, buffer.files[buffer.selectedFileIndex]) && !buffer.onPlaylistTab) {
        buffer.playlist.push_back(buffer.files[buffer.selectedFileIndex]);
    }
    if(lf_key_went_down(GLFW_KEY_R) && buffer.onPlaylistTab && buffer.selectedPlaylistIndex != -1) {
        if(buffer.selectedPlaylistIndex == buffer.playingPlaylistFileIndex) {
            state.currentSound.stop();
        }
        buffer.playlist.erase(buffer.playlist.begin() + buffer.selectedPlaylistIndex);
    } 
}
void playFileWithIndex(uint32_t i) {
    SoundBuffer& buffer = state.openFolders[state.folderIndex];
    if(!buffer.onPlaylistTab)
        buffer.playingFileIndex = i;
    else 
        buffer.playingPlaylistFileIndex = i;
    if(state.currentSound.isPlaying) {
        state.currentSound.stop();
    }
    if(state.currentSound.isInit) {
        state.currentSound.uninit();
    }
    if(!buffer.onPlaylistTab)
        state.currentSound.init(buffer.path + "/" + buffer.files[buffer.playingFileIndex]);
    else 
        state.currentSound.init(buffer.path + "/" + buffer.playlist[buffer.playingPlaylistFileIndex]);
    state.currentSound.play();
    state.soundProgressBar.max = state.currentSound.lengthInSeconds;

}

int main(int argc, char* argv[]) {
    // Initialization
    initWin(1280, 720); 
    char buf[512] = {0};
    state.pathInput = (LfInputField){.width = 650, .buf = buf, .placeholder = (char*)"Sound Folder..."};

    // Loading Textures
    state.enterTexture = lf_tex_create("../game/textures/enter.png", false, LF_TEX_FILTER_LINEAR);
    state.playTexture = lf_tex_create("../game/textures/play.png", false, LF_TEX_FILTER_LINEAR);
    state.pauseTexture = lf_tex_create("../game/textures/pause.png", false, LF_TEX_FILTER_LINEAR);
    state.skipUpTexture = lf_tex_create("../game/textures/skip_up.png", false, LF_TEX_FILTER_LINEAR);
    state.skipDownTexture = lf_tex_create("../game/textures/skip_down.png", false, LF_TEX_FILTER_LINEAR);
    state.skipSoundUpTexture = lf_tex_create("../game/textures/skip_song_up.png", false, LF_TEX_FILTER_LINEAR);
    state.skipSoundDownTexture = lf_tex_create("../game/textures/skip_song_down.png", false, LF_TEX_FILTER_LINEAR);

    LfTexture tex = lf_tex_create("../game/textures/usa.jpg", false, LF_TEX_FILTER_LINEAR);

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
        renderFolderInputMenu();

        renderFolderTabs();

        renderFilesInCurrentFolder();

        renderVolumeSlider();
        
        renderSoundControls();
        renderSoundProgressBar();

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
