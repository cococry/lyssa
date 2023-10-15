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

#include "../leif.h"

#define RGB_COLOR(r, g, b) (vec4s){LF_RGBA(r, g, b, 255.0f)}
#define RGBA_COLOR(r, g, b, a) (vec4s){LF_RGBA(r, g, b, a)}

enum class FileStatus {
    None = 0,
    FailedToOpen, 
    FailedToCreate,
    AlreadyExists, 
    OpenSuccess,
    CreationSucces
};

struct InputFieldState {
    LfInputField input;
    bool inputOpen = false;
    FileStatus fileStatus = FileStatus::None;

    const float showCommentTextTime = 2.5f;
    float showCommentTextTimer = 0.0f;
    const uint32_t height = 60;
    void init() {
        char* buf = (char*)malloc(512);
        memset(buf, 0, 512);

        input = (LfInputField){.width = 400, .buf = buf, .placeholder = (char*)"File..."};
    }
};

typedef struct {
    std::string buf;
    std::string name;
} FileBuffer;

struct GlobalState {
    GLFWwindow* win;
    uint32_t winWidth, winHeight;

    InputFieldState createFileMenu;
    InputFieldState openFileMenu;

    float deltaTime, lastTime;

    std::vector<FileBuffer> fileBuffers;
    std::vector<char*> bufferFileNames;
    int32_t fileBufferIndex = -1;

    int32_t textScrollOffset = 0;

    LfFont codeFont;
};

static GlobalState state;

static void             winResizeCb(GLFWwindow* window, int32_t width, int32_t height);
static void             initWin(uint32_t width, uint32_t height);

static void             renderNewFileMenu();
static void             renderOpenFileMenu();

static void             renderBuffersTab();
static void             renderFileContentTab();

static void             renderFileCommentsTab();

static FileStatus       handleCreateFile(const std::string& filepath);
static FileStatus       handleOpenFile(const std::string& filepath);

static void             handleDeleteBuffer(uint32_t bufferIndex);

static void             onMouseScroll(GLFWwindow* window, double xoffset, double yoffset);

void winResizeCb(GLFWwindow* window, int32_t width, int32_t height) {
    lf_resize_display(width, height);
    glViewport(0, 0, width, height);
    state.winWidth = width;
    state.winHeight = height;
}
void initWin(uint32_t width, uint32_t height) {
    state.winWidth = width;
    state.winHeight = height;
    if(!glfwInit()) {
        std::cout << "Failed to initialize GLFW.\n";
    }

    state.win = glfwCreateWindow(width, height, "ZynEd" ,NULL, NULL);
    if(!state.win) {
        std::cout << "Failed to create GLFW window.\n";
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

    lf_add_scroll_callback((void*)onMouseScroll);

    state.codeFont = lf_load_font("../game/fonts/Cascadia.ttf", 24);
}

void renderNewFileMenu() { 
    if(lf_button("New") == LF_CLICKED) {
        state.createFileMenu.inputOpen = !state.createFileMenu.inputOpen;
    }
    if(state.createFileMenu.inputOpen) {
        lf_input_text(&state.createFileMenu.input);
        lf_set_item_color(RGB_COLOR(22, 181, 125)); 
        if(lf_button_fixed("Create", 100, -1) == LF_CLICKED) {
            state.createFileMenu.fileStatus = handleCreateFile(state.createFileMenu.input.buf);
            state.createFileMenu.showCommentTextTimer = 0.0f;
        }
        lf_unset_item_color();
    }
}

void renderOpenFileMenu() {
    if(lf_button("Open") == LF_CLICKED) {
        state.openFileMenu.inputOpen = !state.openFileMenu.inputOpen;
    }
    if(state.openFileMenu.inputOpen) {
        lf_input_text(&state.openFileMenu.input);
        LfUIElementProps props = lf_theme()->button_props;
        if(lf_button("Open") == LF_CLICKED) {
            lf_set_item_color(RGB_COLOR(46, 46, 179)); 
            std::ifstream file(std::string(state.openFileMenu.input.buf));

            state.openFileMenu.fileStatus = handleOpenFile(state.openFileMenu.input.buf);
            state.openFileMenu.showCommentTextTimer = 0.0f;
            lf_unset_item_color();
            lf_pop_style_props();
        }
    }
}
void renderBuffersTab() {
    lf_next_line();
    if(state.fileBuffers.size() > 1) { 
        std::vector<const char*> filenames;
        for(auto& fileBuffer : state.fileBuffers) {
            filenames.push_back(fileBuffer.name.c_str());
        }
        int32_t clickedItem = lf_menu_item_list(filenames.data(), filenames.size(), state.fileBufferIndex, RGB_COLOR(189, 189, 189), [](uint32_t* item_index) {
                LfUIElementProps props = lf_theme()->button_props;
                props.margin_left = 0;
                props.margin_right = 0;
                lf_set_item_color(RGB_COLOR(0, 0, 0)); 
                lf_set_text_color(RGB_COLOR(255, 255, 255));
                lf_push_style_props(props); 
                if(lf_button("X") == LF_CLICKED) {
                handleDeleteBuffer(*item_index); 
                }
                lf_pop_style_props();
                lf_unset_item_color();
                lf_unset_text_color();
                }, false);
        if(clickedItem != -1)
            state.fileBufferIndex = clickedItem;
    }
}
void renderFileCommentsTab() {
    // Create Button
    if(state.createFileMenu.fileStatus != FileStatus::None && state.createFileMenu.showCommentTextTimer < state.createFileMenu.showCommentTextTime) {
        state.createFileMenu.showCommentTextTimer += state.deltaTime;
        lf_next_line();
        if(state.createFileMenu.fileStatus == FileStatus::FailedToCreate) {
            lf_set_text_color(RGB_COLOR(166, 0, 0));
            lf_text("Failed to create file.");
            lf_unset_text_color();
        } else if(state.createFileMenu.fileStatus == FileStatus::AlreadyExists) {
            lf_set_text_color(RGB_COLOR(166, 0, 0));
            lf_text("File already exists.");
            lf_unset_text_color();
        }
    }
    // Open Button
    if(state.openFileMenu.fileStatus != FileStatus:: None && state.openFileMenu.showCommentTextTimer < state.openFileMenu.showCommentTextTime) {
        state.openFileMenu.showCommentTextTimer += state.deltaTime;
        lf_next_line();
        if(state.openFileMenu.fileStatus == FileStatus::FailedToOpen) {
            lf_set_text_color(RGB_COLOR(166, 0, 0));
            lf_text("Failed to open file.");
            lf_unset_text_color();
        }
    }
}

void renderFileContentTab() {
    if(state.fileBufferIndex != -1) {
        lf_next_line();
        FileBuffer& buffer = state.fileBuffers[state.fileBufferIndex];
        const char* fileContent = buffer.buf.c_str();
        lf_push_font(&state.codeFont);
        lf_push_text_stop_y(state.winHeight);
        lf_push_text_start_y(lf_get_ptr_y());
        LfUIElementProps props = lf_theme()->text_props;
        props.margin_top = state.codeFont.font_size;
        props.margin_left = 10;
        lf_push_style_props(props);
        lf_set_ptr_y(lf_get_ptr_y() - state.textScrollOffset);
        lf_text(fileContent);
        lf_pop_style_props();
        lf_pop_text_stop_y();
        lf_pop_text_start_y();
        lf_pop_font();
    }
}
FileStatus handleCreateFile(const std::string& filepath) {
    if(std::filesystem::exists(filepath)) {
        std::cerr << "File '" << filepath << "' already exists.\n";
        return FileStatus::AlreadyExists;
    }

    std::ofstream inputFile(filepath); 
    if (!inputFile.is_open()) {
        std::cerr << "Failed to create file '" << filepath  << "\n";
        return FileStatus::FailedToCreate;
    }
    state.fileBufferIndex = state.fileBuffers.size();
    state.fileBuffers.push_back((FileBuffer){.buf = "", .name = filepath});
    return FileStatus::CreationSucces;
}
FileStatus handleOpenFile(const std::string& filepath) {
    std::ifstream inputFile(filepath); 
    if (!inputFile.is_open()) {
        std::cerr << "Failed to read from file '" << filepath  << "\n";
        return FileStatus::FailedToOpen;
    }
    std::stringstream buf;
    buf << inputFile.rdbuf();
    inputFile.close(); 
    state.fileBufferIndex = state.fileBuffers.size();
    state.fileBuffers.push_back((FileBuffer){.buf = buf.str(), .name = filepath});
    return FileStatus::OpenSuccess;
}
void handleDeleteBuffer(uint32_t bufferIndex) {
    auto it = state.fileBuffers.begin() + bufferIndex;
    state.fileBuffers.erase(it);
    state.fileBufferIndex--;
    if(state.fileBufferIndex < 0) state.fileBufferIndex = 0;
}

void onMouseScroll(GLFWwindow* window, double xoffset, double yoffset) {
    state.textScrollOffset += yoffset * state.codeFont.font_size;
}

int main(int argc, char* argv[]) {
    // Initialization
    initWin(1280, 720); 
    state.createFileMenu.init();
    state.openFileMenu.init();

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
        lf_rect((float)state.winWidth, 60, RGBA_COLOR(31, 31, 31, 255));
        renderNewFileMenu();
        renderOpenFileMenu();
        renderFileCommentsTab();
        renderBuffersTab();
        renderFileContentTab();
        lf_div_end();

        // Flush
        lf_end();
        glfwPollEvents();
        glfwSwapBuffers(state.win);
    }

    return 0;
} 
