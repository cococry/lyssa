#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <sstream>
#include <vector>

#include "../leif.h"

#define RGB_COLOR(r, g, b) (vec4s){LF_RGBA(r, g, b, 255.0f)}
#define RGBA_COLOR(r, g, b, a) (vec4s){LF_RGBA(r, g, b, a)}

struct InputFieldState {
    LfInputField input;
    bool inputOpen = false;
    int32_t creationSuccess = -1;
    
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
};

static GlobalState state;

static void       winResizeCb(GLFWwindow* window, int32_t width, int32_t height);
static void       initWin(uint32_t width, uint32_t height);

static void       renderNewFileMenu();
static void       renderOpenFileMenu();

static void       renderBuffersTab();
static void       renderFileContentTab();

static bool       handleCreateFile(const std::string& filepath);
static bool       handleOpenFile(const std::string& filepath);

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
    LfTheme theme = lf_default_theme("../game/fonts/arial.ttf", 28);
    theme.div_props.color = (vec4s){LF_RGBA(0, 0, 0, 0)};
    lf_init_glfw(width, height, "../game/fonts/arial.ttf", &theme, state.win);   
    lf_set_text_wrap(true);
    glfwSetFramebufferSizeCallback(state.win, winResizeCb);
    glViewport(0, 0, width, height);
}

void renderNewFileMenu() { 
    if(lf_button("New") == LF_CLICKED) {
        state.createFileMenu.inputOpen = !state.createFileMenu.inputOpen;
    }
    if(state.createFileMenu.inputOpen) {
        lf_input_text(&state.createFileMenu.input);
        lf_set_item_color(RGB_COLOR(22, 181, 125)); 
        if(lf_button("Create") == LF_CLICKED) {
            std::ofstream file(std::string(state.createFileMenu.input.buf));
            
            state.createFileMenu.creationSuccess = handleCreateFile(state.createFileMenu.input.buf);
            state.createFileMenu.showCommentTextTimer = 0.0f;
        }
        lf_unset_item_color();
        if(state.createFileMenu.creationSuccess != -1 && state.createFileMenu.showCommentTextTimer < state.createFileMenu.showCommentTextTime) {
            state.createFileMenu.showCommentTextTimer += state.deltaTime;
            lf_next_line();
            if(!state.createFileMenu.creationSuccess) {
                lf_set_text_color(RGB_COLOR(166, 0, 0));
                lf_text("Failed to create file '%s'", state.createFileMenu.input.buf);
                lf_unset_text_color();
            }
        }
    }

}

void renderOpenFileMenu() {
   if(lf_button("Open") == LF_CLICKED) {
        state.openFileMenu.inputOpen = !state.openFileMenu.inputOpen;
    }
    if(state.openFileMenu.inputOpen) {
        lf_input_text(&state.openFileMenu.input);
        lf_set_item_color(RGB_COLOR(46, 46, 179)); 
        if(lf_button("Open") == LF_CLICKED) {
            std::ifstream file(std::string(state.openFileMenu.input.buf));
            
            state.openFileMenu.creationSuccess = handleOpenFile(state.openFileMenu.input.buf);
            state.openFileMenu.showCommentTextTimer = 0.0f;
        }
        lf_unset_item_color();
        if(state.openFileMenu.creationSuccess != -1 && state.openFileMenu.showCommentTextTimer < state.openFileMenu.showCommentTextTime) {
            state.openFileMenu.showCommentTextTimer += state.deltaTime;
            lf_next_line();
            if(!state.openFileMenu.creationSuccess) {
                lf_set_text_color(RGB_COLOR(166, 0, 0));
                lf_text("Failed to create file '%s'", state.openFileMenu.input.buf);
                lf_unset_text_color();
            }
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
        int32_t clickedItem = lf_menu_item_list(filenames.data(), filenames.size(), state.fileBufferIndex, RGB_COLOR(189, 189, 189), false);
        if(clickedItem != -1)
            state.fileBufferIndex = clickedItem;
    }
}

void renderFileContentTab() {
    if(state.fileBufferIndex != -1) {
        lf_next_line();
        FileBuffer& buffer = state.fileBuffers[state.fileBufferIndex];
        const char* fileContent = buffer.buf.c_str();
        lf_text("%s", fileContent);
    }
}
bool handleCreateFile(const std::string& filepath) {
    std::ofstream inputFile(filepath); 
    if (!inputFile.is_open()) {
        std::cerr << "Failed to create file '" << filepath  << "\n";
        return false;
    }
    state.fileBufferIndex = state.fileBuffers.size();
    state.fileBuffers.push_back((FileBuffer){.buf = "", .name = filepath});
    return true;
}
bool handleOpenFile(const std::string& filepath) {
    std::ifstream inputFile(filepath); 
    if (!inputFile.is_open()) {
        std::cerr << "Failed to read from file '" << filepath  << "\n";
        return false;
    }
    std::stringstream buf;
    buf << inputFile.rdbuf();
    inputFile.close(); 
    state.fileBufferIndex = state.fileBuffers.size();
    state.fileBuffers.push_back((FileBuffer){.buf = buf.str(), .name = filepath});
    return true;
}

int main(int argc, char* argv[]) {
    initWin(1920, 1080); 
    state.createFileMenu.init();
    state.openFileMenu.init();

    while(!glfwWindowShouldClose(state.win)) { 
        float currentTime = glfwGetTime();
        state.deltaTime = currentTime - state.lastTime;
        state.lastTime = currentTime;

        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(LF_RGBA(25, 25, 25, 255));
        
        lf_div_begin((vec2s){0.0f, 0.0f}, (vec2s){(float)state.winWidth, (float)state.winHeight});
        lf_rect((float)state.winWidth, 60, RGB_COLOR(31, 31, 31));
        renderNewFileMenu();
        renderOpenFileMenu();
        renderBuffersTab();
        renderFileContentTab();
        lf_div_end();

        // Flush
        lf_update();
        glfwPollEvents();
        glfwSwapBuffers(state.win);
    }

    return 0;
} 
