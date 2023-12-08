#include "leif.h"
#include <cglm/mat4.h>
#include <cglm/types-struct.h>
#include <ctype.h>
#include <glad/glad.h>
#include <stb_image.h>
#include <stb_truetype.h>
#include <GLFW/glfw3.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#ifdef LF_GLFW 
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif 
#include <GLFW/glfw3.h>
#endif /* ifdef */

#define MAX(a, b) a > b ? a : b
#define MIN(a, b) a < b ? a : b

#define LF_TRACE(...) { printf("Leif: [TRACE]: "); printf(__VA_ARGS__); printf("\n"); } 
#ifdef LF_DEBUG
#define LF_DEBUG(...) { printf("Leif: [DEBUG]: "); printf(__VA_ARGS__); printf("\n"); } 
#else
#define LF_DEBUG(...) 
#endif // LF_DEBUG 
#define LF_INFO(...) { printf("Leif: [INFO]: "); printf(__VA_ARGS__); printf("\n"); } 
#define LF_WARN(...) { printf("Leif: [WARN]: "); printf(__VA_ARGS__); printf("\n"); } 
#define LF_ERROR(...) { printf("[LEIF ERROR]: "); printf(__VA_ARGS__); printf("\n"); } 

#ifdef _MSC_VER 
#define D_BREAK __debugbreak
#elif defined(__clang__) || defined(__GNUC__)
#define D_BREAK __builtin_trap
#else 
#define D_BREAK
#endif 

#ifdef _DEBUG 
#define LF_ASSERT(cond, ...) { if(cond) {} else { printf("[LEIF]: Assertion failed: '"); printf(__VA_ARGS__); printf("' in file '%s' on line %i.\n", __FILE__, __LINE__); D_BREAK(); }}
#else 
#define LF_ASSERT(cond, ...)
#endif // _DEBUG


#ifdef LF_GLFW
#define MAX_KEYS GLFW_KEY_LAST
#define MAX_MOUSE_BUTTONS GLFW_MOUSE_BUTTON_LAST
#define KEY_CALLBACK_t GLFWkeyfun
#define MOUSE_BUTTON_CALLBACK_t GLFWmousebuttonfun
#define SCROLL_CALLBACK_t GLFWscrollfun
#define CURSOR_CALLBACK_t GLFWcursorposfun
#else 

#define MAX_KEYS 0
#define MAX_MOUSE_BUTTONS 0
#define KEY_CALLBACK_t void*
#define MOUSE_BUTTON_CALLBACK_t void*
#define SCROLL_CALLBACK_t void*
#define CURSOR_CALLBACK_t void*
#endif
#define MAX_RENDER_BATCH 10000
#define MAX_TEX_COUNT_BATCH 32
#define MAX_KEY_CALLBACKS 4
#define MAX_MOUSE_BTTUON_CALLBACKS 4
#define MAX_SCROLL_CALLBACKS 4
#define MAX_CURSOR_POS_CALLBACKS 4

// -- Struct Defines ---
typedef struct {
    uint32_t id;
} LfShader;

typedef struct {
    vec2 pos; // 8 Bytes
    vec4 border_color; // 16 Bytes
    float border_width; // 4 Bytes 
    vec4 color; // 16 Bytes
    vec2 texcoord; // 8 Bytes
    float tex_index; // 4 Bytes
    vec2 scale; // 8 Bytes
    vec2 pos_px; // 8 Bytes
    float corner_radius; // 4 Bytes
    float is_gradient; // 4 Bytes
    vec2 min_coord, max_coord; // 16 Bytes
} Vertex; // 92 Bytes per vertex

typedef struct {
    uint32_t id;

    LfAABB aabb;
    LfClickableItemState interact_state;
    LfUIElementProps props;

    bool init;

    vec2s total_area;
} LfDiv;

typedef struct {
    bool keys[MAX_KEYS];
    bool keys_changed[MAX_KEYS];
} LfKeyboard;

typedef struct {
    bool buttons_current[MAX_MOUSE_BUTTONS];
    bool buttons_last[MAX_MOUSE_BUTTONS];

    double xpos, ypos, xpos_last, ypos_last, xpos_delta, ypos_delta;
    bool first_mouse_press; 
    double xscroll_delta, yscroll_delta;
} LfMouse;

// State of input 
typedef struct {
    LfKeyboard keyboard;
    LfMouse mouse;

    // List of callbacks (user defined)
    KEY_CALLBACK_t key_cbs[MAX_KEY_CALLBACKS];
    MOUSE_BUTTON_CALLBACK_t mouse_button_cbs[MAX_MOUSE_BTTUON_CALLBACKS];
    SCROLL_CALLBACK_t scroll_cbs[MAX_SCROLL_CALLBACKS];
    CURSOR_CALLBACK_t cursor_pos_cbs[MAX_CURSOR_POS_CALLBACKS];

    uint32_t key_cb_count, mouse_button_cb_count, scroll_cb_count, cursor_pos_cb_count;
} InputState;

// State of the batch renderer
typedef struct {
    LfShader shader;
    uint32_t vao, vbo, ibo;
    uint32_t vert_count;
    Vertex* verts;
    vec4s vert_pos[4];
    LfTexture textures[MAX_TEX_COUNT_BATCH];
    uint32_t tex_index, tex_count,index_count;
} RenderState;

typedef struct {
    bool init;
    bool text_wrap;
    
    // Window
    uint32_t dsp_w, dsp_h;
    void* window_handle;

    RenderState render;
    InputState input;
    LfTheme theme;

    LfDiv current_div, selected_div, selected_div_tmp, prev_div;
    int32_t current_line_height, prev_line_height;
    vec2s pos_ptr, prev_pos_ptr; 


    // Pushable variables
    LfFont* font_stack, *prev_font_stack;
    LfUIElementProps props_stack, div_props, prev_props_stack;
    bool props_on_stack;
    int32_t text_start_x_stack, text_stop_x_stack, 
        text_start_y_stack, text_stop_y_stack;

    // Event references 
    LfKeyEvent key_ev;
    LfMouseButtonEvent mb_ev;
    LfCursorPosEvent cp_ev;
    LfScrollEvent scr_ev;
    LfCharEvent ch_ev;
    LfGuiReEstablishEvent gui_re_ev;
    
    vec2s cull_start, cull_end;

    float div_scrolls[32];

    uint32_t div_count, div_index_ptr;
} LfState;

typedef enum {
    INPUT_INT = 0, 
    INPUT_FLOAT, 
    INPUT_TEXT
} InputFieldType;

// Static object to retrieve state data during runtime
static LfState state;

// --- Renderer ---
static uint32_t                 shader_create(GLenum type, const char* src);
static LfShader                 shader_prg_create(const char* vert_src, const char* frag_src);
static void                     shader_set_mat(LfShader prg, const char* name, mat4 mat); 
static void                     set_projection_matrix();

static void                     renderer_init();

static LfTexture                tex_create(const char* filepath, bool flip, LfTextureFiltering filter);

// --- UI ---
static LfClickableItemState     clickable_item(vec2s pos, vec2s size, LfUIElementProps props, vec4s color, float border_width, bool click_color, bool hover_color);
static LfTextProps              text_render_simple(vec2s pos, const char* text, LfFont font, vec4s font_color, bool no_render);
static void                     input_field(LfInputField* input, InputFieldType type);
LfFont                          load_font(const char* filepath, uint32_t pixelsize, uint32_t tex_width, uint32_t tex_height, uint32_t num_glyphs, uint32_t line_gap_add);
static void                     next_line_on_overflow(vec2s size);
static bool                     area_hovered(vec2s pos, vec2s size);

// Utility
static int32_t                  closes_num_in_arr(int arr[], int n, int target);
static int32_t                  get_max_char_height_font(LfFont font);
static double                   get_char_width(LfFont font, char c);
static float                    get_kerning(int32_t prev_character_codepoint, int current_character_codepoint);
static void                     remove_i_str(char *str, int32_t index);
static void                     insert_i_str(char *str, char ch, int32_t index);
static int32_t                  int_max(int32_t a, int32_t b);

// --- Input ---
#ifdef LF_GLFW
static void                     glfw_key_callback(GLFWwindow* window, int32_t key, int scancode, int action, int mods);
static void                     glfw_mouse_button_callback(GLFWwindow* window, int32_t button, int action, int mods); 
static void                     glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
static void                     glfw_cursor_callback(GLFWwindow* window, double xpos, double ypos);
static void                     glfw_char_callback(GLFWwindow* window, uint32_t charcode);
#endif

static void                     update_input();
static void                     clear_events();

// --- Static Functions --- 
uint32_t shader_create(GLenum type, const char* src) {
    // Create && compile the shader with opengl 
    uint32_t shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    // Check for compilation errors
    int32_t compiled; 
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if(!compiled) {
        LF_ERROR("Failed to compile %s shader.", type == GL_VERTEX_SHADER ? "vertex" : "fragment");
        char info[512];
        glGetShaderInfoLog(shader, 512, NULL, info);
        LF_INFO("%s", info);
        glDeleteShader(shader);
    }
    return shader;
}

LfShader shader_prg_create(const char* vert_src, const char* frag_src) {
    // Creating vertex & fragment shader with the shader API
    uint32_t vertex_shader = shader_create(GL_VERTEX_SHADER, vert_src);
    uint32_t fragment_shader = shader_create(GL_FRAGMENT_SHADER, frag_src);

    // Creating & linking the shader program with OpenGL
    LfShader prg;
    prg.id = glCreateProgram();
    glAttachShader(prg.id, vertex_shader);
    glAttachShader(prg.id, fragment_shader);
    glLinkProgram(prg.id);

    // Check for linking errors
    int32_t linked;
    glGetProgramiv(prg.id, GL_LINK_STATUS, &linked);

    if(!linked) {
        LF_ERROR("Failed to link shader program.");
        char info[512];
        glGetProgramInfoLog(prg.id, 512, NULL, info);
        LF_INFO("%s", info);
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        glDeleteProgram(prg.id);
        return prg;
    }
    
    // Delete the shaders after
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return prg;
}


char *readFile(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Could not open file %s\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(file_size * sizeof(char) + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Memory allocation failed");
        fclose(file);
        return NULL;
    }

    size_t result = fread(buffer, 1, file_size, file);
    if (result != file_size) {
        fprintf(stderr, "Reading file failed");
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[file_size] = '\0'; // Null-terminate the string
    fclose(file);
    return buffer;
}


void renderer_init() {
    // OpenGL Setup 
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    state.render.vert_count = 0;
    state.render.verts = (Vertex*)malloc(sizeof(Vertex) * MAX_RENDER_BATCH * 4);

    /* Creating vertex array & vertex buffer for the batch renderer */
    glCreateVertexArrays(1, &state.render.vao);
    glBindVertexArray(state.render.vao);
    
    glCreateBuffers(1, &state.render.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, state.render.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * MAX_RENDER_BATCH * 4, NULL, 
        GL_DYNAMIC_DRAW);

    uint32_t* indices = (uint32_t*)malloc(sizeof(uint32_t) * MAX_RENDER_BATCH * 6);

    uint32_t offset = 0;
    for (uint32_t i = 0; i < MAX_RENDER_BATCH * 6; i += 6) {
        indices[i + 0] = offset + 0;
        indices[i + 1] = offset + 1;
        indices[i + 2] = offset + 2;

        indices[i + 3] = offset + 2;
        indices[i + 4] = offset + 3;
        indices[i + 5] = offset + 0;
        offset += 4;
    }
    glCreateBuffers(1, &state.render.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.render.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_RENDER_BATCH * 6 * sizeof(uint32_t), indices, GL_STATIC_DRAW);

    free(indices); 
    // Setting the vertex layout
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), NULL);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(intptr_t)offsetof(Vertex, border_color));
    glEnableVertexAttribArray(1);
    
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(intptr_t)offsetof(Vertex, border_width));
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(intptr_t)offsetof(Vertex, color));
    glEnableVertexAttribArray(3);
    
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(intptr_t*)offsetof(Vertex, texcoord));
    glEnableVertexAttribArray(4);

    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(intptr_t*)offsetof(Vertex, tex_index));
    glEnableVertexAttribArray(5);

    glVertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(intptr_t*)offsetof(Vertex, scale));
    glEnableVertexAttribArray(6);

    glVertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(intptr_t*)offsetof(Vertex, pos_px));
    glEnableVertexAttribArray(7);

    glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(intptr_t*)offsetof(Vertex, corner_radius));
    glEnableVertexAttribArray(8);

    glVertexAttribPointer(9, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(intptr_t*)offsetof(Vertex, is_gradient));
    glEnableVertexAttribArray(9);

    glVertexAttribPointer(10, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(intptr_t*)offsetof(Vertex, min_coord));
    glEnableVertexAttribArray(10);

    glVertexAttribPointer(11, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(intptr_t*)offsetof(Vertex, max_coord));
    glEnableVertexAttribArray(11);

    // Creating the shader for the batch renderer
    const char* vert_src =
        "#version 460 core\n"
        "layout (location = 0) in vec2 a_pos;\n"
        "layout (location = 1) in vec4 a_border_color;\n"
        "layout (location = 2) in float a_border_width;\n"
        "layout (location = 3) in vec4 a_color;\n"
        "layout (location = 4) in vec2 a_texcoord;\n"
        "layout (location = 5) in float a_tex_index;\n"
        "layout (location = 6) in vec2 a_scale;\n"
        "layout (location = 7) in vec2 a_pos_px;\n"
        "layout (location = 8) in float a_corner_radius;\n"
        "layout (location = 9) in float a_is_gradient;\n"
        "layout (location = 10) in vec2 a_min_coord;\n"
        "layout (location = 11) in vec2 a_max_coord;\n"

        "uniform mat4 u_proj;\n"
        "out vec4 v_border_color;\n"
        "out float v_border_width;\n"
        "out vec4 v_color;\n"
        "out vec2 v_texcoord;\n"
        "out float v_tex_index;\n"
        "flat out vec2 v_scale;\n"
        "flat out vec2 v_pos_px;\n"
        "flat out float v_is_gradient;\n"
        "out float v_corner_radius;\n"
        "out vec2 v_min_coord;\n"
        "out vec2 v_max_coord;\n"

        "void main() {\n"
            "v_color = a_color;\n"
            "v_texcoord = a_texcoord;\n"
            "v_tex_index = a_tex_index;\n"
            "v_border_color = a_border_color;\n"
            "v_border_width = a_border_width;\n"
            "v_scale = a_scale;\n"
            "v_pos_px = a_pos_px;\n"
            "v_corner_radius = a_corner_radius;\n"
            "v_is_gradient = a_is_gradient;\n"
            "v_min_coord = a_min_coord;\n"
            "v_max_coord = a_max_coord;\n"
            "gl_Position = u_proj * vec4(a_pos.x, a_pos.y, 0.0f, 1.0);\n"
        "}\n";


    const char* frag_src = "#version 460 core\n"
        "out vec4 o_color;\n"
        "in vec4 v_color;\n"
        "in float v_tex_index;\n"
        "in vec4 v_border_color;\n"
        "in float v_border_width;\n"
        "in vec2 v_texcoord;\n"
        "flat in vec2 v_scale;\n"
        "flat in vec2 v_pos_px;\n"
        "flat in float v_is_gradient;\n"
        "in float v_corner_radius;\n"
        "uniform sampler2D u_textures[32];\n"
        "uniform vec2 u_screen_size;\n"
        "in vec2 v_min_coord;\n"
        "in vec2 v_max_coord;\n"

        "float rounded_box_sdf(vec2 center_pos, vec2 size, float radius) {\n"
        "    return length(max(abs(center_pos)-size+radius,0.0))-radius;\n"
        "}\n"

        "void main() {\n"
        "     if(u_screen_size.y - gl_FragCoord.y < v_min_coord.y && v_min_coord.y != -1) {\n"
        "         discard;\n"
        "     }\n"
        "     if(u_screen_size.y - gl_FragCoord.y > v_max_coord.y && v_max_coord.y != -1) {\n"
        "         discard;\n"
        "     }\n"
        "     if(u_screen_size.x - gl_FragCoord.y < v_min_coord.x && v_min_coord.x != -1) {\n"
        "         discard;\n"
        "     }\n"
        "     if(u_screen_size.x - gl_FragCoord.y > v_max_coord.x && v_max_coord.x != -1) {\n"
        "         discard;\n"
        "     }\n"
        "     if(v_is_gradient == 1) {\n"
        "         float normalized_y = gl_FragCoord.y / u_screen_size.y;\n"
        
        "         normalized_y *= u_screen_size.x / u_screen_size.y;\n"
        "         vec4 blended_color = mix(v_color, v_border_color, normalized_y);\n"
        "         o_color = blended_color;\n"
        "     } else {\n"
        "       vec2 size = v_scale;\n"
        "       vec4 opaque_color, display_color;\n"
        "       if(v_tex_index == -1) {\n"
        "         opaque_color = v_color;\n"
        "       } else {\n"
        "         opaque_color = texture(u_textures[int(v_tex_index)], v_texcoord) * v_color;\n"
        "       }\n"
        "       if(v_corner_radius != 0.0f) {"
        "         display_color = opaque_color;\n"

        "         vec2 location = vec2(v_pos_px.x, -v_pos_px.y);\n"
        "         location.y += u_screen_size.y - size.y;\n"

        "         float edge_softness = 1.0f;\n"

        "         float radius = v_corner_radius * 2.0f;\n"

        "         float distance = rounded_box_sdf(gl_FragCoord.xy - location - (size/2.0f), size / 2.0f, radius);\n"

        "         float smoothed_alpha = 1.0f-smoothstep(0.0f, edge_softness * 2.0f,distance);\n"


        "         vec3 fill_color;\n"
        "         if(v_border_width != 0.0f) {\n"
        "             vec2 location_border = vec2(location.x + v_border_width, location.y + v_border_width);\n"
        "             vec2 size_border = vec2(size.x - v_border_width * 2, size.y - v_border_width * 2);\n"
        "             float distance_border = rounded_box_sdf(gl_FragCoord.xy - location_border - (size_border / 2.0f), size_border / 2.0f, radius);\n"
        "             if(distance_border <= 0.0f) {\n"
        "                 fill_color = display_color.xyz;\n"
        "             } else {\n"
        "                 fill_color = v_border_color.xyz;\n"
        "             }\n"
        "         } else {\n"
        "             fill_color = display_color.xyz;\n"
        "         }\n"
        "         if(v_border_width != 0.0f)\n" 
        "           o_color =  mix(vec4(0.0f, 0.0f, 0.0f, 0.0f), vec4(fill_color, v_tex_index == -1 ? smoothed_alpha : display_color.a), smoothed_alpha);\n"
        "         else\n" 
        "           o_color = mix(vec4(0.0f, 0.0f, 0.0f, 0.0f), vec4(fill_color, display_color.a), smoothed_alpha);\n"
        "     } else {\n"
        "       vec4 fill_color = opaque_color;\n"
        "       if(v_border_width != 0.0f) {\n"
        "           vec2 location = vec2(v_pos_px.x, -v_pos_px.y);\n"
        "           location.y += u_screen_size.y - size.y;\n"
        "           vec2 location_border = vec2(location.x + v_border_width, location.y + v_border_width);\n"
        "           vec2 size_border = vec2(v_scale.x - v_border_width * 2, v_scale.y - v_border_width * 2);\n"
        "           float distance_border = rounded_box_sdf(gl_FragCoord.xy - location_border - (size_border / 2.0f), size_border / 2.0f, v_corner_radius);\n"
        "           if(distance_border > 0.0f) {\n"
        "               fill_color = v_border_color;\n"
                    "}\n"
        "       }\n"
        "       o_color = fill_color;\n"
        "     }\n"
        " }\n"
        "}\n";
    state.render.shader = shader_prg_create(vert_src, frag_src);

    // initializing vertex position data
    state.render.vert_pos[0] = (vec4s){-0.5f, -0.5f, 0.0f, 1.0f};
    state.render.vert_pos[1] = (vec4s){0.5f, -0.5f, 0.0f, 1.0f};
    state.render.vert_pos[2] = (vec4s){0.5f, 0.5f, 0.0f, 1.0f};
    state.render.vert_pos[3] = (vec4s){-0.5f, 0.5f, 0.0f, 1.0f};


    // Populating the textures array in the shader with texture ids
    int32_t tex_slots[MAX_TEX_COUNT_BATCH];
    for(uint32_t i = 0; i < MAX_TEX_COUNT_BATCH; i++) 
        tex_slots[i] = i;

    glUseProgram(state.render.shader.id);
    set_projection_matrix();
    glUniform1iv(glGetUniformLocation(state.render.shader.id, "u_textures"), MAX_TEX_COUNT_BATCH, tex_slots);
}
void shader_set_mat(LfShader prg, const char* name, mat4 mat) {
    glUniformMatrix4fv(glGetUniformLocation(prg.id, name), 1, GL_FALSE, mat[0]);
}

LfTexture lf_tex_create(const char* filepath, bool flip, LfTextureFiltering filter) {
    // Loading the texture into memory with stb_image
    LfTexture tex;
    int32_t width, height, channels;
    stbi_set_flip_vertically_on_load(flip);
    stbi_uc* data = stbi_load(filepath, &width, &height, &channels, 0);

    if(!data) {
        LF_ERROR("Failed to load texture file at '%s'.", filepath);
        return tex;
    }
    GLenum internal_format = (channels == 4) ? GL_RGBA8 : GL_RGB8;
    GLenum data_format = (channels == 4) ? GL_RGBA : GL_RGB;
    
    LF_ASSERT(internal_format & data_format, "Texture file at '%s' is using an unsupported format.", filepath);
    
    // Creating the textures in opengl with the loaded data
    glCreateTextures(GL_TEXTURE_2D, 1, &tex.id);
    glTextureStorage2D(tex.id, 1, internal_format, width, height);
    
    // Setting texture parameters
    switch(filter) {
        case LF_TEX_FILTER_LINEAR:
            glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            break;
        case LF_TEX_FILTER_NEAREST:
            glTextureParameteri(tex.id, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTextureParameteri(tex.id, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            break;
    }
    glTextureParameteri(tex.id, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(tex.id, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTextureSubImage2D(tex.id, 0, 0, 0, width, height, data_format, GL_UNSIGNED_BYTE, data);
    
    // Deallocating the data when finished
    stbi_image_free(data);

    tex.width = width;
    tex.height = height;

    return tex;
}

LfFont load_font(const char* filepath, uint32_t pixelsize, uint32_t tex_width, uint32_t tex_height, uint32_t num_glyphs, uint32_t line_gap_add) {
    LfFont font = {0};
    /* Opening the file, reading the content to a buffer and parsing the loaded data with stb_truetype */
    FILE* file = fopen(filepath, "rb");
    if (file == NULL) {
        LF_ERROR("Failed to open font file '%s'\n", filepath);
    }

    // Loading the content
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t* buffer = (uint8_t*)malloc(fileSize);
    fread(buffer, 1, fileSize, file);
    fclose(file); 
    font.font_info = malloc(sizeof(stbtt_fontinfo));

    // Initializing the font with stb_truetype
    stbtt_InitFont((stbtt_fontinfo*)font.font_info, buffer, stbtt_GetFontOffsetForIndex(buffer, 0));
    
    // Loading the font bitmap to memory by using stbtt_BakeFontBitmap
    uint8_t buf[1<<20];
    uint8_t* bitmap = (uint8_t*)malloc(tex_width * tex_height * sizeof(uint32_t));
    uint8_t* bitmap_4bpp = (uint8_t*)malloc(tex_width * tex_height * 4 * sizeof(uint32_t));
    fread(buf, 1, 1<<20, fopen(filepath, "rb"));
    font.cdata = malloc(sizeof(stbtt_bakedchar) * 256);
    font.tex_width = tex_width;
    font.tex_height = tex_height;
    font.line_gap_add = line_gap_add;
    font.font_size = pixelsize;
    stbtt_BakeFontBitmap(buf, 0, pixelsize, bitmap, tex_width, tex_height, 32, num_glyphs, (stbtt_bakedchar*)font.cdata);

    uint32_t bitmap_index = 0;
    for(uint32_t i = 0; i < (uint32_t)(tex_width * tex_height * 4); i++) {
        bitmap_4bpp[i] = bitmap[bitmap_index];
        if((i + 1) % 4 == 0) {
            bitmap_index++;
        }
    }

    // Creating an opengl texture (texture atlas) for the font
    glGenTextures(1, &font.bitmap.id);
    glBindTexture(GL_TEXTURE_2D, font.bitmap.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_width, tex_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap_4bpp);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Deallocating the bitmap data 
    free(bitmap);
    free(bitmap_4bpp);
    return font;
}

LfFont lf_load_font(const char* filepath, uint32_t size) {
    return load_font(filepath, size, 1024, 1024, 256, 0);
}

void lf_free_font(LfFont* font) {
    free(font->cdata);
    free(font->font_info);
}

bool lf_point_intersects_aabb(vec2s p, LfAABB aabb) {
    return p.x <= (aabb.size.x + (aabb.size.x / 2.0f)) && p.x >= (aabb.pos.x - (aabb.size.x / 2.0f)) && 
        p.y <= (aabb.pos.y + (aabb.size.y / 2.0f)) && p.y >= (aabb.pos.y - (aabb.size.y / 2.0f));
}

bool lf_aabb_intersects_aabb(LfAABB a, LfAABB b) {
    a.pos.x = a.pos.x + a.size.x / 2.0f;
    a.pos.y = a.pos.y + a.size.y / 2.0f;

    b.pos.x = b.pos.x + b.size.x / 2.0f;
    b.pos.y = b.pos.y + b.size.y / 2.0f;

    if(a.pos.x + a.size.x / 2.0f < b.pos.x - b.size.x / 2.0f
        || b.pos.x + b.size.x / 2.0f < a.pos.x - a.size.x / 2.0f) return false;

    if(a.pos.y + a.size.y / 2.0f < b.pos.y - b.size.y / 2.0f 
        || b.pos.y + b.size.y / 2.0f < a.pos.y - a.size.y / 2.0f) return false;
    return true;
}
LfClickableItemState clickable_item(vec2s pos, vec2s size, LfUIElementProps props, vec4s color, float border_width,  bool click_color, bool hover_color) {
    if(!state.current_div.init) {
        LF_ERROR("Trying to render without div context. Call lf_div_begin()");
        return LF_IDLE;
    }

    /* Rendering a rect with the given proportions with different color based on if it is hoverd, clicked or idle */
    bool is_hovered = lf_hovered(pos, size);
    if(is_hovered && lf_mouse_button_is_released(GLFW_MOUSE_BUTTON_LEFT)) {
        vec4s hover_color_rgb = (vec4s){LF_RGBA(color.r - 20, color.g - 20,
                                                color.b - 20, color.a - 20)};
        lf_rect_render(pos, size, hover_color ? hover_color_rgb : color, props.border_color, border_width, props.corner_radius);
        return LF_RELEASED;
    }
    if(is_hovered && lf_mouse_button_went_down(GLFW_MOUSE_BUTTON_LEFT)) {
        if(click_color) {
            // Click color is 40 lighter than the base color for every value
            vec4s click_color = (vec4s){LF_RGBA(color.r + 40, color.g + 40,
                                                color.b + 40, color.a + 40)};
            // Clamping the color
            if(click_color.r > 1.0)
                click_color.r = 1.0;
            if(click_color.g > 1.0)
                click_color.g = 1.0;
            if(click_color.b > 1.0)
                click_color.b = 1.0;
            if(click_color.a > 1.0)
                click_color.a = 1.0;
            lf_rect_render(pos, size, click_color, props.border_color, border_width, props.corner_radius);
        } else {
            vec4s hover_color_rgb = (vec4s){LF_RGBA(color.r - 20, color.g - 20,
                                                color.b - 20, color.a - 20)};
            lf_rect_render(pos, size, hover_color ? hover_color_rgb : color, props.border_color, border_width, props.corner_radius);
        }
        return LF_CLICKED;
    }
    if(is_hovered && lf_mouse_button_is_down(GLFW_MOUSE_BUTTON_LEFT)) {
            vec4s hover_color_rgb = (vec4s){LF_RGBA(color.r - 20, color.g - 20,
                                                color.b - 20, color.a - 20)};
        lf_rect_render(pos, size, hover_color ? hover_color_rgb : color, props.border_color, border_width, props.corner_radius);
        return LF_HELD;
    }
    if(is_hovered && (!lf_mouse_button_went_down(GLFW_MOUSE_BUTTON_LEFT) && !lf_mouse_button_is_down(GLFW_MOUSE_BUTTON_LEFT))) {
        if(hover_color) {
            // Hover color is 20 darker than the base color for every value
            vec4s hover_color = (vec4s){LF_RGBA(color.r - 20, color.g - 20,
                                                color.b - 20, color.a - 20)};
            // Clamping the color
            if(hover_color.r > 1.0)
                hover_color.r = 1.0;
            if(hover_color.g > 1.0)
                hover_color.g = 1.0;
            if(hover_color.b > 1.0)
                hover_color.b = 1.0;
            if(hover_color.a > 1.0)
                hover_color.a = 1.0;
            lf_rect_render(pos, size, hover_color, props.border_color, border_width, props.corner_radius);
        } else {
            lf_rect_render(pos, size, color, props.border_color, border_width, props.corner_radius);
        }
        return LF_HOVERED;
    }
    // Rendering with base color if idle
    lf_rect_render(pos, size, color, props.border_color, border_width, props.corner_radius);
    return LF_IDLE;
}
LfTextProps text_render_simple(vec2s pos, const char* text, LfFont font, vec4s font_color, bool no_render) {
    return lf_text_render(pos, text, font, -1, -1, -1, -1, -1, -1, no_render, font_color);
}

#ifdef LF_GLFW
void glfw_key_callback(GLFWwindow* window, int32_t key, int scancode, int action, int mods) {
    (void)window;
    (void)mods;
    (void)scancode;
    // Changing the the keys array to resamble the state of the keyboard 
    if(action != GLFW_RELEASE) {
        if(!state.input.keyboard.keys[key]) 
            state.input.keyboard.keys[key] = true;
    }  else {
        state.input.keyboard.keys[key] = false;
    }
    state.input.keyboard.keys_changed[key] = (action != GLFW_REPEAT);

    // Calling user defined callbacks
    for(uint32_t i = 0; i < state.input.key_cb_count; i++) {
        state.input.key_cbs[i](window, key, scancode, action, mods);
    }

    // Populating the key event
    state.key_ev.happened = true;
    state.key_ev.pressed = action != GLFW_RELEASE;
    state.key_ev.keycode = key;
}
void glfw_mouse_button_callback(GLFWwindow* window, int32_t button, int action, int mods) {
    (void)window;
    (void)mods;
    // Changing the buttons array to resamble the state of the mouse
    if(action != GLFW_RELEASE)  {
        if(!state.input.mouse.buttons_current[button])
            state.input.mouse.buttons_current[button] = true;
    } else {
        state.input.mouse.buttons_current[button] = false;
    }
    // Calling user defined callbacks
    for(uint32_t i = 0; i < state.input.mouse_button_cb_count; i++) {
        state.input.mouse_button_cbs[i](window, button, action, mods);
    }
    // Populating the mouse button event
    state.mb_ev.happened = true;
    state.mb_ev.pressed = action != GLFW_RELEASE;
    state.mb_ev.button_code = button;
}
void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    // Setting the scroll values
    state.input.mouse.xscroll_delta = xoffset;
    state.input.mouse.yscroll_delta = yoffset;

    // Calling user defined callbacks
    for(uint32_t i = 0; i< state.input.scroll_cb_count; i++) {
        state.input.scroll_cbs[i](window, xoffset, yoffset);
    }
    // Populating the scroll event
    state.scr_ev.happened = true;
    state.scr_ev.xoffset = xoffset;
    state.scr_ev.yoffset = yoffset;


    if(yoffset == -1) {
        if(state.selected_div.total_area.y > (state.selected_div.aabb.size.y + state.selected_div.aabb.pos.y)) {
            state.div_scrolls[state.selected_div.id] -= LF_SCROLL_AMOUNT;
        } 
    } else if (yoffset == 1) {
        if(state.div_scrolls[state.selected_div.id] < 0) {
            state.div_scrolls[state.selected_div.id] += LF_SCROLL_AMOUNT;
        }        
    }

}
void glfw_cursor_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    // Setting the position values 
    state.input.mouse.xpos = xpos;
    state.input.mouse.ypos = ypos;

    if(state.input.mouse.first_mouse_press) {
        state.input.mouse.xpos_last = xpos;
        state.input.mouse.ypos_last = ypos;
        state.input.mouse.first_mouse_press = false;
    }
    // Delta mouse positions 
    state.input.mouse.xpos_delta = state.input.mouse.xpos - state.input.mouse.xpos_last;
    state.input.mouse.ypos_delta = state.input.mouse.ypos - state.input.mouse.ypos_last;
    state.input.mouse.xpos_last = xpos;
    state.input.mouse.ypos_last = ypos;

    // Calling User defined callbacks
    for(uint32_t i = 0; i < state.input.cursor_pos_cb_count; i++) {
        state.input.cursor_pos_cbs[i](window, xpos, ypos);
    }

    // Populating the cursor event
    state.cp_ev.happened = true;
    state.cp_ev.x = xpos;
    state.cp_ev.y = ypos;
}
void glfw_char_callback(GLFWwindow* window, uint32_t charcode) {
    (void)window;
    state.ch_ev.charcode = charcode;
    state.ch_ev.happened = true;
}
#endif

void lf_rect_render(vec2s pos, vec2s size, vec4s color, vec4s border_color, float border_width, float corner_radius) {
    // Offsetting the postion, so that pos is the top left of the rendered object
    vec2s pos_initial = pos;
    pos = (vec2s){pos.x + size.x / 2.0f, pos.y + size.y / 2.0f};

    // Initializing texture coords data
    vec2s texcoords[4] = {
        (vec2s){1.0f, 1.0f},
        (vec2s){1.0f, 0.0f},
        (vec2s){0.0f, 0.0f},
        (vec2s){0.0f, 1.0f},
    };
    // Calculating the transform matrix
    mat4 translate; 
    mat4 scale;
    mat4 transform;
    vec3 pos_xyz = {(corner_radius != 0.0f ? (float)state.dsp_w / 2.0f : pos.x), (corner_radius != 0.0f ? (float)state.dsp_h / 2.0f : pos.y), 0.0f};
    vec3 size_xyz = {corner_radius != 0.0f ? state.dsp_w : size.x, corner_radius != 0.0f ? state.dsp_h : size.y, 0.0f};
    glm_translate_make(translate, pos_xyz);
    glm_scale_make(scale, size_xyz);
    glm_mat4_mul(translate,scale,transform);
    
    // Adding the vertices to the batch renderer
    for(uint32_t i = 0; i < 4; i++) {
        if(state.render.vert_count >= MAX_RENDER_BATCH) {
            lf_flush();
            lf_renderer_begin();
        }
        vec4 result;
        glm_mat4_mulv(transform, state.render.vert_pos[i].raw, result);
        state.render.verts[state.render.vert_count].pos[0] = result[0];
        state.render.verts[state.render.vert_count].pos[1] = result[1];
        vec4 border_color_arr;
        border_color_arr[0] = border_color.x;
        border_color_arr[1] = border_color.y;
        border_color_arr[2] = border_color.z;
        border_color_arr[3] = border_color.w;
        memcpy(state.render.verts[state.render.vert_count].border_color, border_color_arr, sizeof(vec4));

        state.render.verts[state.render.vert_count].border_width = border_width; 

        vec4 color_arr;
        color_arr[0] = color.r;
        color_arr[1] = color.g;
        color_arr[2] = color.b;
        color_arr[3] = color.a;
        memcpy(state.render.verts[state.render.vert_count].color, color_arr, sizeof(vec4));
        vec2 texcoord_arr;
        texcoord_arr[0] = texcoords[i].x;
        texcoord_arr[1] = texcoords[i].y;
        memcpy(state.render.verts[state.render.vert_count].texcoord, texcoord_arr, sizeof(vec2));
        state.render.verts[state.render.vert_count].tex_index = -1;

        vec2 scale_arr;
        scale_arr[0] = size.x;
        scale_arr[1] = size.y;
        memcpy(state.render.verts[state.render.vert_count].scale, scale_arr, sizeof(vec2));

        vec2 pos_px_arr;
        pos_px_arr[0] = (float)pos_initial.x;
        pos_px_arr[1] = (float)pos_initial.y;
        memcpy(state.render.verts[state.render.vert_count].pos_px, pos_px_arr, sizeof(vec2));

        state.render.verts[state.render.vert_count].corner_radius = corner_radius;
        state.render.verts[state.render.vert_count].is_gradient = 0;

        vec2 cull_start_arr;
        cull_start_arr[0] = state.cull_start.x;
        cull_start_arr[1] = state.cull_start.y;
        vec2 cull_end_arr;
        cull_end_arr[0] = state.cull_end.x;
        cull_end_arr[1] = state.cull_end.y;
        memcpy(state.render.verts[state.render.vert_count].min_coord, cull_start_arr, sizeof(vec2));
        memcpy(state.render.verts[state.render.vert_count].max_coord, cull_end_arr, sizeof(vec2));

        state.render.vert_count++;
    }
    state.render.index_count += 6;
}

void lf_rect_gradient_render(vec2s pos, vec2s size, vec4s top_color, vec4s bottom_color) {
 // Offsetting the postion, so that pos is the top left of the rendered object
    vec2s pos_initial = pos;
    pos = (vec2s){pos.x + size.x / 2.0f, pos.y + size.y / 2.0f};

    // Initializing texture coords data
    vec2s texcoords[4] = {
        (vec2s){1.0f, 1.0f},
        (vec2s){1.0f, 0.0f},
        (vec2s){0.0f, 0.0f},
        (vec2s){0.0f, 1.0f},
    };
    // Calculating the transform matrix
    mat4 translate; 
    mat4 scale;
    mat4 transform;
    vec3 pos_xyz = {pos.x, pos.y, 0.0f};
    vec3 size_xyz = {size.x, size.y, 0.0f};
    glm_translate_make(translate, pos_xyz);
    glm_scale_make(scale, size_xyz);
    glm_mat4_mul(translate,scale,transform);

    // Adding the vertices to the batch renderer
    for(uint32_t i = 0; i < 4; i++) {
        if(state.render.vert_count >= MAX_RENDER_BATCH) {
            lf_flush();
            lf_renderer_begin();
        }
        vec4 result;
        glm_mat4_mulv(transform, state.render.vert_pos[i].raw, result);
        state.render.verts[state.render.vert_count].pos[0] = result[0];
        state.render.verts[state.render.vert_count].pos[1] = result[1];
        vec4 border_color_arr;
        border_color_arr[0] = top_color.r;
        border_color_arr[1] = top_color.g;
        border_color_arr[2] = top_color.b;
        border_color_arr[3] = top_color.a;
        memcpy(state.render.verts[state.render.vert_count].border_color, border_color_arr, sizeof(vec4));

        state.render.verts[state.render.vert_count].border_width = 0.0f; 

        vec4 color_arr;
        color_arr[0] = bottom_color.r;
        color_arr[1] = bottom_color.g;
        color_arr[2] = bottom_color.b;
        color_arr[3] = bottom_color.a;
        memcpy(state.render.verts[state.render.vert_count].color, color_arr, sizeof(vec4));
        vec2 texcoord_arr;
        texcoord_arr[0] = texcoords[i].x;
        texcoord_arr[1] = texcoords[i].y;
        memcpy(state.render.verts[state.render.vert_count].texcoord, texcoord_arr, sizeof(vec2));
        state.render.verts[state.render.vert_count].tex_index = -1;

        vec2 scale_arr;
        scale_arr[0] = size.x;
        scale_arr[1] = size.y;
        memcpy(state.render.verts[state.render.vert_count].scale, scale_arr, sizeof(vec2));

        vec2 pos_px_arr;
        pos_px_arr[0] = (float)pos_initial.x;
        pos_px_arr[1] = (float)pos_initial.y;
        memcpy(state.render.verts[state.render.vert_count].pos_px, pos_px_arr, sizeof(vec2));

        state.render.verts[state.render.vert_count].corner_radius = 0.0f;
        state.render.verts[state.render.vert_count].is_gradient = 1;

        vec2 cull_start_arr;
        cull_start_arr[0] = state.cull_start.x;
        cull_start_arr[1] = state.cull_start.y;
        vec2 cull_end_arr;
        cull_end_arr[0] = state.cull_end.x;
        cull_end_arr[1] = state.cull_end.y;
        memcpy(state.render.verts[state.render.vert_count].min_coord, cull_start_arr, sizeof(vec2));
        memcpy(state.render.verts[state.render.vert_count].max_coord, cull_end_arr, sizeof(vec2));

        state.render.vert_count++;
    }
    state.render.index_count += 6;
}

void lf_image_render(vec2s pos, vec4s color, LfTexture tex, vec4s border_color, float border_width, float corner_radius) {
    // Offsetting the postion, so that pos is the top left of the rendered object
    vec2s pos_initial = pos;
    pos = (vec2s){pos.x + tex.width / 2.0f, pos.y + tex.height / 2.0f};

    // Initializing texture coords data
    vec2s texcoords[4] = {
        (vec2s){0.0f, 0.0f},
        (vec2s){1.0f, 0.0f},
        (vec2s){1.0f, 1.0f},
        (vec2s){0.0f, 1.0f},
    };
    // Retrieving the texture index of the rendered texture
    float tex_index = -1.0f;
    for(uint32_t i = 0; i < state.render.tex_count; i++) {
        if(tex.id == state.render.textures[i].id)  {
            tex_index = i;
            break;
        }
    }
    if(tex_index == -1.0f) {
        tex_index = (float)state.render.tex_index;
        state.render.textures[state.render.tex_count++] = tex;
        state.render.tex_index++;
    }
    // Calculating the transform
    mat4 translate = GLM_MAT4_IDENTITY_INIT; 
    mat4 scale = GLM_MAT4_IDENTITY_INIT;
    mat4 transform = GLM_MAT4_IDENTITY_INIT;
    vec3s pos_xyz = (vec3s){pos.x, pos.y, 0.0f};
    vec3 tex_size;
    tex_size[0] = tex.width;
    tex_size[1] = tex.height;
    tex_size[2] = 0;
    glm_translate_make(translate, pos_xyz.raw);
    glm_scale_make(scale, tex_size);
    glm_mat4_mul(translate,scale,transform);

    // Adding the vertices to the batch renderer
    for(uint32_t i = 0; i < 4; i++) {
        if(state.render.vert_count >= MAX_RENDER_BATCH) {
            lf_flush();
            lf_renderer_begin();
        }
        vec4 result;
        glm_mat4_mulv(transform, state.render.vert_pos[i].raw, result);
        memcpy(state.render.verts[state.render.vert_count].pos, result, sizeof(vec2));
        vec4 border_color_arr;
        border_color_arr[0] = border_color.x;
        border_color_arr[1] = border_color.y;
        border_color_arr[2] = border_color.z;
        border_color_arr[3] = border_color.w;
        memcpy(state.render.verts[state.render.vert_count].border_color, border_color_arr, sizeof(vec4));

        state.render.verts[state.render.vert_count].border_width = border_width; 

        vec4 color_arr;
        color_arr[0] = color.r;
        color_arr[1] = color.g;
        color_arr[2] = color.b;
        color_arr[3] = color.a;
        memcpy(state.render.verts[state.render.vert_count].color, color_arr, sizeof(vec4));

        vec2 texcoord_arr;
        texcoord_arr[0] = texcoords[i].x;
        texcoord_arr[1] = texcoords[i].y;
        memcpy(state.render.verts[state.render.vert_count].texcoord, texcoord_arr, sizeof(vec2));

        state.render.verts[state.render.vert_count].tex_index = tex_index;

        vec2 scale_arr;
        scale_arr[0] = (float)tex.width;
        scale_arr[1] = (float)tex.height;
        memcpy(state.render.verts[state.render.vert_count].scale, scale_arr, sizeof(vec2));

        vec2 pos_px_arr;
        pos_px_arr[0] = (float)pos_initial.x;
        pos_px_arr[1] = (float)pos_initial.y;
        memcpy(state.render.verts[state.render.vert_count].pos_px, pos_px_arr, sizeof(vec2));

        state.render.verts[state.render.vert_count].corner_radius = corner_radius;
        state.render.verts[state.render.vert_count].is_gradient = 0;

        vec2 cull_start_arr;
        cull_start_arr[0] = state.cull_start.x;
        cull_start_arr[1] = state.cull_start.y;
        vec2 cull_end_arr;
        cull_end_arr[0] = state.cull_end.x;
        cull_end_arr[1] = state.cull_end.y;
        memcpy(state.render.verts[state.render.vert_count].min_coord, cull_start_arr, sizeof(vec2));
        memcpy(state.render.verts[state.render.vert_count].max_coord, cull_end_arr, sizeof(vec2));

        state.render.vert_count++;
    } 
    state.render.index_count += 6;
}

void input_field(LfInputField* input, InputFieldType type) {
    if(!input->buf) return;
    // Getting the property data of the input field
    LfUIElementProps props = state.props_on_stack ? state.props_stack : state.theme.button_props;
    float padding = props.padding;
    float margin_left = props.margin_left;
    float margin_right = props.margin_right;
    float margin_top = props.margin_top;
    float margin_bottom = props.margin_bottom;
    float border_width = props.border_width;

    vec4s color = props.color;
    vec4s text_color = props.text_color;
    LfFont font = state.font_stack ? *state.font_stack : state.theme.font;

    // If the object cannot be fully rendered on the current line, advance one line down
    next_line_on_overflow(
        (vec2s){input->width + padding * 2.0f + margin_left + margin_right + border_width * 2.0f,
                    input->height + padding * 2.0f + margin_top + margin_bottom + border_width * 2.0f}
    );
    // Adding the margins to the position pointer
    state.pos_ptr.x += margin_left + border_width;
    state.pos_ptr.y += margin_top + border_width;
    if(input->selected) {
        // Handeling key input of the inputfiled
        if(lf_key_went_down(GLFW_KEY_BACKSPACE)) {
            if(input->cursor_index - 1 >= 0) {
                remove_i_str(input->buf, input->cursor_index - 1);
                input->cursor_index--;
            }
        }
        if(lf_key_went_down(GLFW_KEY_RIGHT)) {
            if(input->cursor_index <= (int32_t)strlen(input->buf) - 1) {
                input->cursor_index++;
            }
        }
        if(lf_key_went_down(GLFW_KEY_LEFT)) {
            if(input->cursor_index - 1 >= 0)
                input->cursor_index--;
        }
        if(lf_key_went_down(GLFW_KEY_ENTER) && input->expand_on_overflow) {
            insert_i_str(input->buf, '\n', input->cursor_index);
            input->cursor_index++;
        }
        if(lf_key_went_down(GLFW_KEY_TAB)) {
            for(uint32_t i = 0; i < 4; i++) {
                insert_i_str(input->buf, ' ', input->cursor_index);
                input->cursor_index++;
            }
        }
        if(state.ch_ev.happened) {
            switch(type) {
                case INPUT_FLOAT: {
                    char* end_ptr;
                    double val;
                    insert_i_str(input->buf, state.ch_ev.charcode, input->cursor_index);
                    val = strtod(input->buf,&end_ptr);
                    if(*end_ptr == '\0') {
                        input->cursor_index++;
                    } else {
                        remove_i_str(input->buf, input->cursor_index);
                    }
                    break;
                }
                case INPUT_INT: {
                    if(isdigit(state.ch_ev.charcode)) {
                        insert_i_str(input->buf, state.ch_ev.charcode, input->cursor_index);
                        input->cursor_index++;
                    }
                    break;
                }
                case INPUT_TEXT: {
                    insert_i_str(input->buf, state.ch_ev.charcode, input->cursor_index);
                    input->cursor_index++;
                    break;
                }
            }
        }
    }

    LfTextProps text_props_post_input = lf_text_render((vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + padding},
                                                    input->buf, font, input->expand_on_overflow ? input->width + padding : -1, -1, -1, -1, -1, -1, true, text_color);
    // Rendering the input field
    int32_t width = text_props_post_input.width > input->width ? text_props_post_input.width : input->width;

    if(!input->height) {
        input->height = get_max_char_height_font(font); 
    };
    if(text_props_post_input.height > input->height) {
        input->height = text_props_post_input.height;
    }
    LfClickableItemState item = clickable_item(state.pos_ptr,
                                               (vec2s){input->width + padding * 2.0f, input->height + padding * 2.0f},
                                                props, color, border_width, false,false);

    // Handeling selecting the input field
    if(item == LF_CLICKED && !input->selected) {
        input->selected = true;
        input->cursor_index = strlen(input->buf);
    }  else if(item == LF_IDLE && lf_mouse_button_went_down(GLFW_MOUSE_BUTTON_LEFT) && input->selected) {
        input->selected = false;
    }

    // Rendering the text in the input field
    LfTextProps text_props_rendered = lf_text_render((vec2s){(state.pos_ptr.x + padding),
        state.pos_ptr.y + padding}, input->buf,
                                                     font, input->expand_on_overflow ? input->width + padding : -1, -1, -1, -1, -1, -1, false, text_color);

    // Handleing the cursor indicator
    if(input->selected) {
        // Getting the part of the text that is before the cursor
        char cursor_slice[strlen(input->buf)];
        for(uint32_t i = 0; i <= (uint32_t)input->cursor_index; i++) {
            cursor_slice[i] = input->buf[i];
        }
        cursor_slice[input->cursor_index] = '\0';

        LfTextProps cursor_pos_props = lf_text_render((vec2s){(float)text_props_rendered.start_x,
            state.pos_ptr.y + padding}, cursor_slice,
                                                      font, input->expand_on_overflow ? input->width + padding : -1, -1, -1, -1, -1, -1, true, text_color);

        // Rendering the cursor indicator
        lf_rect_render((vec2s){(strlen(input->buf) != 0) ? cursor_pos_props.end_x : state.pos_ptr.x + padding,
                    state.pos_ptr.y + (input->expand_on_overflow ? (cursor_pos_props.height - get_max_char_height_font(font)) : 0) + padding},
                    (vec2s){1, (float)get_max_char_height_font(font)}, props.text_color, (vec4s){0, 0, 0, 0}, 0.0f, 0.0f);
    } else if(!input->selected && strlen(input->buf) == 0) {
        // Rendering the placeholder
        lf_text_render((vec2s){(state.pos_ptr.x + padding),
        state.pos_ptr.y + padding},
                    !input->placeholder ? "Type..." : input->placeholder, font, -1, -1, -1, -1, -1, -1, false, text_color);
    }
    // Advancing the position pointer by the size of the inputf ield
    state.pos_ptr.x += input->width + margin_right + padding * 2.0f + border_width;
    state.pos_ptr.y -= margin_top + border_width;

    // Setting the value pointer
    if(type == INPUT_FLOAT)
        *(float*)input->val = atof(input->buf);
    else if(type == INPUT_INT)
        *(int32_t*)input->val = atoi(input->buf);
    if(input->width > input->start_width) {
        input->width = width;
    } else {
        input->width = input->start_width;
    }
}


float get_kerning(int32_t prev_character_codepoint, int current_character_codepoint) {
    LfFont font = state.font_stack ? *state.font_stack : state.theme.font;
    float scale = stbtt_ScaleForPixelHeight((stbtt_fontinfo*)font.font_info, font.font_size);
    int32_t kern_advance = stbtt_GetCodepointKernAdvance((stbtt_fontinfo*)font.font_info, prev_character_codepoint, current_character_codepoint);
    float kerning = kern_advance * scale;
    return kerning;
}
static int32_t get_max_char_height_font(LfFont font) {
    float fontScale = stbtt_ScaleForPixelHeight((stbtt_fontinfo*)font.font_info, font.font_size);
    int32_t xmin, ymin, xmax, ymax;
    int32_t codepoint = 'y';
    stbtt_GetCodepointBitmapBox((stbtt_fontinfo*)font.font_info, codepoint, fontScale, fontScale, &xmin, &ymin, &xmax, &ymax);
    return ymax - ymin;
}

void remove_i_str(char *str, int32_t index) {
    int32_t len = strlen(str);
    if (index >= 0 && index < len) {
        for (int32_t i = index; i < len - 1; i++) {
            str[i] = str[i + 1];
        }
        str[len - 1] = '\0';
    }
}
void insert_i_str(char *str, char ch, int32_t index) {
     int len = strlen(str);

    if (index < 0 || index > len) {
        LF_ERROR("Invalid string index for inserting.");;
        return;
    }

    for (int i = len; i > index; i--) {
        str[i] = str[i - 1];
    }

    str[index] = ch;
}
int32_t int_max(int32_t a, int32_t b) {
    return (a > b) ? a : b;
}

bool lf_hovered(vec2s pos, vec2s size) {
    bool hovered = lf_get_mouse_x() <= (pos.x + size.x) && lf_get_mouse_x() >= (pos.x) && 
        lf_get_mouse_y() <= (pos.y + size.y) && lf_get_mouse_y() >= (pos.y) && state.selected_div.id == state.div_index_ptr - 1;
    return hovered;
}

void next_line_on_overflow(vec2s size) {
    // If the object does not fit in the area of the current div, advance to the next line
    if(state.pos_ptr.x - state.current_div.aabb.pos.x + size.x > state.current_div.aabb.size.x) {
        lf_next_line();
    }
    if(size.y > state.current_line_height) {
        state.current_line_height = size.y;
    }
}

bool area_hovered(vec2s pos, vec2s size) {
    bool hovered = lf_get_mouse_x() <= (pos.x + size.x) && lf_get_mouse_x() >= (pos.x) && 
        lf_get_mouse_y() <= (pos.y + size.y) && lf_get_mouse_y() >= (pos.y);
    return hovered;
}

int32_t closes_num_in_arr(int arr[], int n, int target) {
    int32_t closest = 0;
    int32_t min_diff = INT_MAX;
    for (int32_t i = 0; i < n; i++) {
        int32_t diff = abs(arr[i] - target);
        if (diff < min_diff) {
            min_diff = diff;
            closest = arr[i];
        }
    }
    return closest;
}
LfTextProps lf_text_render(vec2s pos, const char* str, LfFont font, int32_t wrap_point, int32_t stop_point_x, int32_t start_point_x, int32_t stop_point_y, int32_t start_point_y, int32_t max_wrap_count, bool no_render, 
                        vec4s color) {
    if(state.text_start_x_stack != -2) {
        start_point_x = state.text_start_x_stack;
    }
    if(state.text_stop_x_stack != -2) {
        stop_point_x = state.text_stop_x_stack;
    }
    if(state.text_start_y_stack != -2) {
        start_point_y = state.text_start_y_stack;
    }
    if(state.text_stop_y_stack != -2) {
        stop_point_y = state.text_stop_y_stack;
    }
    // Retrieving the texture index
    float tex_index = -1.0f;
    if(!no_render) {
        for(uint32_t i = 0; i < state.render.tex_count; i++) {
            if(state.render.textures[i].id == font.bitmap.id) {
                tex_index = (float)i;
                break;
            }
        }
        if(tex_index == -1.0f) {
            tex_index = (float)state.render.tex_index;
            LfTexture tex = font.bitmap;
            state.render.textures[state.render.tex_count++] = tex;
            state.render.tex_index++;
        }
    }
    // Local variables needed for rendering
    LfTextProps ret = {0};
    float x = pos.x;
    float y = pos.y;
    int32_t max_descended_char_height = get_max_char_height_font(font);

    bool reached_stop = false;

    float last_x = x;
    float last_char_width = 0, first_char_width = -1;
    int32_t first_char_index = 0;
    int32_t i = 0;

    int32_t height = get_max_char_height_font(font);
    float width = 0;
    int32_t char_count = 0;

    int32_t wrap_count = 0;
    
    while(*str) { 
        bool skip = false;
        // If the current character is a new line or the wrap point has been reached, advance to the next line
        if(*str == '\n' || (x >= wrap_point && wrap_point != -1)) {
            y += font.font_size;
            height += font.font_size;
            if(x - pos.x > width) {
                width = x - pos.x;
            }
            x = pos.x;
            last_x = x;
            if(*str == ' ') {
                skip = true; 
            }
            if(max_wrap_count != -1)
                wrap_count++; 
        }

        if(wrap_count >= max_wrap_count && max_wrap_count != -1) { 
            ret.reached_max_wraps = true;
            break;
        }
        if(!skip) {
            // Retrieving the vertex data of the current character & submitting it to the batch 
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad((stbtt_bakedchar*)font.cdata, font.tex_width, font.tex_height, *str-32, &x, &y, &q, 1);

            // Return if reached stop
            if((x - pos.x > stop_point_x && stop_point_x != -1) || (y > stop_point_y && stop_point_y != -1)) {
                reached_stop = true;
                if(x - pos.x > width) {
                    width = x - pos.x;
                }
                ret.width = width; 
                ret.height = height;
                ret.char_count = i - first_char_index; 
                ret.reached_stop = reached_stop;
                ret.end_x = x;
                return ret;
            }
            if(((start_point_x != -1 && x >= start_point_x) || start_point_x == -1) && ((start_point_y != -1 && y + pos.y >= start_point_y) || start_point_y == -1))  {
                if(first_char_width == -1) { 
                    first_char_width = x - last_x;
                    first_char_index = i; 
                    ret.start_x = q.x0;
                }
                // Adding the vertices to the batch if rendering the text
                if(!no_render) {
                    vec2s texcoords[4] = {
                        q.s0, q.t0, 
                        q.s1, q.t0, 
                        q.s1, q.t1, 
                        q.s0, q.t1
                    };
                    vec2s verts[4] = {
                        (vec2s){q.x0, q.y0 + max_descended_char_height}, 
                        (vec2s){q.x1, q.y0 + max_descended_char_height}, 
                        (vec2s){q.x1, q.y1 + max_descended_char_height},
                        (vec2s){q.x0, q.y1 + max_descended_char_height}
                    }; 
                    for(uint32_t i = 0; i < 4; i++) {
                        if(state.render.vert_count >= MAX_RENDER_BATCH) {
                            lf_flush();
                            lf_renderer_begin();
                        }
                        vec2 verts_arr;
                        verts_arr[0] = verts[i].x;
                        verts_arr[1] = verts[i].y;
                        memcpy(state.render.verts[state.render.vert_count].pos, verts_arr, sizeof(vec2));

                        vec4 border_color = {0, 0, 0, 0};
                        memcpy(state.render.verts[state.render.vert_count].border_color, border_color, sizeof(vec4));
                        state.render.verts[state.render.vert_count].border_width = 0;

                        vec4 color_arr;
                        color_arr[0] = color.r;
                        color_arr[1] = color.g;
                        color_arr[2] = color.b;
                        color_arr[3] = color.a;
                        memcpy(state.render.verts[state.render.vert_count].color, color_arr, sizeof(vec4));
                        vec2 texcoord_arr;
                        texcoord_arr[0] = texcoords[i].x;
                        texcoord_arr[1] = texcoords[i].y;
                        memcpy(state.render.verts[state.render.vert_count].texcoord, texcoord_arr, sizeof(vec2));
                        state.render.verts[state.render.vert_count].tex_index = tex_index;

                        vec2 scale_arr;
                        scale_arr[0] = 0;
                        scale_arr[1] = 0;
                        memcpy(state.render.verts[state.render.vert_count].scale, scale_arr, sizeof(vec2));

                        vec2 pos_px_arr;
                        pos_px_arr[0] = 0;
                        pos_px_arr[1] = 0;
                        memcpy(state.render.verts[state.render.vert_count].pos_px, pos_px_arr, sizeof(vec2));

                        state.render.verts[state.render.vert_count].corner_radius = 0;
                        state.render.verts[state.render.vert_count].is_gradient = 0;

                        vec2 cull_start_arr;
                        cull_start_arr[0] = state.cull_start.x;
                        cull_start_arr[1] = state.cull_start.y;
                        vec2 cull_end_arr;
                        cull_end_arr[0] = state.cull_end.x;
                        cull_end_arr[1] = state.cull_end.y;
                        memcpy(state.render.verts[state.render.vert_count].min_coord, cull_start_arr, sizeof(vec2));
                        memcpy(state.render.verts[state.render.vert_count].max_coord, cull_end_arr, sizeof(vec2));

                        state.render.vert_count++;
                    }
                    state.render.index_count += 6;

                }
                char_count++;
                last_char_width = x - last_x;
                last_x = x;
            }
        }
        str++;
        i++;
    }

    // Populating the return value
    if(x - pos.x > width) {
        width = x - pos.x;
    }
    ret.width = width;
    ret.height = height;
    ret.char_count = i - first_char_index; 
    ret.reached_stop = (x > stop_point_x);
    ret.end_x = x;
    ret.end_y = y;
    return ret;
}

void lf_renderer_begin() {
    state.render.vert_count = 0;
    state.render.index_count = 0;
    state.render.tex_index = 0;
}
void lf_flush() {
    if(state.render.vert_count <= 0) return;

    // Bind the vertex buffer & shader set the vertex data, bind the textures & draw
    glUseProgram(state.render.shader.id);
    glBindBuffer(GL_ARRAY_BUFFER, state.render.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * state.render.vert_count, 
                    state.render.verts);

    for(uint32_t i = 0; i < state.render.tex_count; i++) {
        glBindTextureUnit(i, state.render.textures[i].id);
    }

    vec2s renderSize = (vec2s){(float)state.dsp_w, (float)state.dsp_h};
    glUniform2fv(glGetUniformLocation(state.render.shader.id, "u_screen_size"), 1, (float*)renderSize.raw);
    glBindVertexArray(state.render.vao);
    glDrawElements(GL_TRIANGLES, state.render.index_count, GL_UNSIGNED_INT, NULL);
}

void update_input() {
    memcpy(state.input.mouse.buttons_last, state.input.mouse.buttons_current, sizeof(bool) * MAX_MOUSE_BUTTONS);
}

void clear_events() {
    state.key_ev.happened = false;
    state.mb_ev.happened = false;
    state.cp_ev.happened = false;
    state.scr_ev.happened = false;
    state.ch_ev.happened = false;
    state.gui_re_ev.happened = false;
    state.input.mouse.xpos_delta = 0;
    state.input.mouse.ypos_delta = 0;
}


// ------------------------------------
// ------- Public API Functions -------
// ------------------------------------
void lf_init_glfw(uint32_t display_width, uint32_t display_height, const char* font_path, LfTheme* theme, void* glfw_window) {
#ifndef LF_GLFW
    LF_ERROR("Trying to initialize Leif with GLFW without defining 'LF_GLFW'");
    return;
#else 
    if(!glfwInit()) {
        LF_ERROR("Trying to initialize Leif with GLFW without initializing GLFW first.");
        return;
    }
    memset(&state, 0, sizeof(state));

    // Default state
    state.init = true;
    state.dsp_w = display_width;
    state.dsp_h = display_height;
    state.window_handle = glfw_window;
    state.input.mouse.first_mouse_press = true;
    state.render.tex_count = 0;
    state.pos_ptr = (vec2s){0, 0};
    state.text_wrap = false;
    state.props_on_stack = false;
    if(theme != NULL) {
        state.theme = *theme;
        if(!state.theme.font.cdata) {
            LF_ERROR("No valid font specified for theme.\n");
        }
    } else {
        state.theme = lf_default_theme(font_path, 24);
    }

    // Setting glfw callbacks
    glfwSetKeyCallback((GLFWwindow*)state.window_handle, glfw_key_callback);
    glfwSetMouseButtonCallback((GLFWwindow*)state.window_handle, glfw_mouse_button_callback);
    glfwSetScrollCallback((GLFWwindow*)state.window_handle, glfw_scroll_callback);
    glfwSetCursorPosCallback((GLFWwindow*)state.window_handle, glfw_cursor_callback);
    glfwSetCharCallback((GLFWwindow*)state.window_handle, glfw_char_callback);
    renderer_init();

    lf_pop_text_start_x();
    lf_pop_text_stop_x();
    lf_pop_text_start_y();
    lf_pop_text_stop_y();
#endif
}

void lf_terminate() {
    lf_free_font(&state.theme.font);
}
LfTheme lf_default_theme(const char* font_path, uint32_t font_size) {
    // The default theme of Leif
    LfTheme theme = {0};
    theme.div_props = (LfUIElementProps){
        .color = (vec4s){LF_RGBA(45, 45, 45, 255)},
        .border_color = (vec4s){0, 0, 0, 0}, 
        .border_width = 0.0f, 
        .corner_radius = 0.0f, 
    };
    float global_padding = 10;
    float global_margin = 5;
    theme.text_props = (LfUIElementProps){
        .text_color = (vec4s){LF_RGBA(255, 255, 255, 255)}, 
        .border_color = (vec4s){0, 0, 0, 0},
        .padding = 0, 
        .margin_left = global_margin, 
        .margin_right = global_margin, 
        .margin_top = global_margin, 
        .margin_bottom = global_margin,
        .border_width = global_margin,
        .corner_radius = 0
    };
    theme.button_props = (LfUIElementProps){ 
        .color = (vec4s){LF_RGBA(133, 138, 148, 255)}, 
        .text_color = (vec4s){LF_RGBA(0, 0, 0, 255)}, 
        .border_color = (vec4s){LF_BLACK},
        .padding = global_padding,
        .margin_left = global_margin, 
        .margin_right = global_margin, 
        .margin_top = global_margin, 
        .margin_bottom = global_margin, 
        .border_width = 4, 
        .corner_radius = 0
    };
    theme.image_props = (LfUIElementProps){ 
        .color = (vec4s){LF_RGBA(255, 255, 255, 255)}, 
        .margin_left = global_margin, 
        .margin_right = global_margin, 
        .margin_top = global_margin, 
        .margin_bottom = global_margin,
        .corner_radius = 0
    };
    theme.inputfield_props = (LfUIElementProps){ 
        .color = (vec4s){LF_RGBA(133, 138, 148, 255)}, 
        .text_color = (vec4s){LF_RGBA(0, 0, 0, 255)}, 
        .border_color = (vec4s){LF_BLACK},
        .padding = global_padding, 
        .margin_left = global_margin, 
        .margin_right = global_margin, 
        .margin_top = global_margin, 
        .margin_bottom = global_margin, 
        .border_width = 4,
        .corner_radius = 0
    };
    theme.checkbox_props = (LfUIElementProps){ 
        .color = (vec4s){LF_RGBA(133, 138, 148, 255)}, 
        .text_color = (vec4s){LF_RGBA(0, 0, 0, 255)}, 
        .border_color = (vec4s){LF_BLACK},
        .padding = global_padding, 
        .margin_left = global_margin, 
        .margin_right = global_margin, 
        .margin_top = global_margin, 
        .margin_bottom = global_margin, 
        .border_width = 4,
        .corner_radius = 0
    };
    theme.slider_props = (LfUIElementProps){ 
        .color = (vec4s){LF_RGBA(133, 138, 148, 255)}, 
        .text_color = (vec4s){LF_RGBA(0, 0, 0, 255)}, 
        .border_color = (vec4s){LF_BLACK},
        .padding = global_padding,
        .margin_left = global_margin, 
        .margin_right = global_margin, 
        .margin_top = global_margin, 
        .margin_bottom = global_margin, 
        .border_width = 4,
        .corner_radius = 0
    };
    theme.scrollbar_props = (LfUIElementProps){ 
        .color = (vec4s){LF_RGBA(100, 100, 100, 255)}, 
        .border_color = (vec4s){LF_BLACK},
        .padding = 0,
        .margin_left = 0, 
        .margin_right = 10, 
        .margin_top = 10, 
        .margin_bottom = 0, 
        .border_width = 0,
        .corner_radius = 2
    };
    theme.font = load_font(font_path, font_size, 1024, 1024, 256, 5); 
    return theme;
}

void lf_resize_display(uint32_t display_width, uint32_t display_height) {
    // Setting the display height internally
    state.dsp_w = display_width;
    state.dsp_h = display_height;

    set_projection_matrix();

    state.current_div.aabb.size.x = state.dsp_w;
    state.current_div.aabb.size.y = state.dsp_h;

}

void set_projection_matrix() { 
    float left = 0.0f;
    float right = state.dsp_w;
    float bottom = state.dsp_h;
    float top = 0.0f;
    float near = 0.1f;
    float far = 100.0f;

    // Create the orthographic projection matrix
    mat4 orthoMatrix = GLM_MAT4_IDENTITY_INIT;
    orthoMatrix[0][0] = 2.0f / (right - left);
    orthoMatrix[1][1] = 2.0f / (top - bottom);
    orthoMatrix[2][2] = -1;
    orthoMatrix[3][0] = -(right + left) / (right - left);
    orthoMatrix[3][1] = -(top + bottom) / (top - bottom);

    shader_set_mat(state.render.shader, "u_proj", orthoMatrix);
}

void lf_add_key_callback(void* cb) {
    state.input.key_cbs[state.input.key_cb_count++] = (KEY_CALLBACK_t)cb;
}
void lf_add_mouse_button_callback(void* cb) {
    state.input.mouse_button_cbs[state.input.mouse_button_cb_count++] = (MOUSE_BUTTON_CALLBACK_t)cb;
}

void lf_add_scroll_callback(void* cb) {
    state.input.scroll_cbs[state.input.scroll_cb_count++] = (SCROLL_CALLBACK_t)cb;
}

void lf_add_cursor_pos_callback(void* cb) {
    state.input.cursor_pos_cbs[state.input.cursor_pos_cb_count++] = (CURSOR_CALLBACK_t)cb;
}

bool lf_key_went_down(uint32_t key) {
    return lf_key_changed(key) && state.input.keyboard.keys[key];
}

bool lf_key_is_down(uint32_t key) {
    return state.input.keyboard.keys[key];
}

bool lf_key_is_released(uint32_t key) {
    return lf_key_changed(key) && !state.input.keyboard.keys[key];
}

bool lf_key_changed(uint32_t key) {
    bool ret = state.input.keyboard.keys_changed[key];
    state.input.keyboard.keys_changed[key] = false;
    return ret;
}

bool lf_mouse_button_went_down(uint32_t button) {
    return lf_mouse_button_changed(button) && state.input.mouse.buttons_current[button];
}

bool lf_mouse_button_is_down(uint32_t button) {
    return state.input.mouse.buttons_current[button];
}

bool lf_mouse_button_is_released(uint32_t button) {
    return lf_mouse_button_changed(button) && !state.input.mouse.buttons_current[button];
}

bool lf_mouse_button_changed(uint32_t button) {
    return state.input.mouse.buttons_current[button] != state.input.mouse.buttons_last[button];
}

double lf_get_mouse_x() {
    return state.input.mouse.xpos;
}

double lf_get_mouse_y() {
    return state.input.mouse.ypos;
}

double lf_get_mouse_x_delta() {
    return state.input.mouse.xpos_delta;
}

double lf_get_mouse_y_delta() {
    return state.input.mouse.ypos_delta;
}

double lf_get_mouse_scroll_x() {
    return state.input.mouse.xscroll_delta;
}

double lf_get_mouse_scroll_y() {
    return state.input.mouse.yscroll_delta;
}

LfClickableItemState lf_button(const char* text) {
    // Retrieving the property data of the button
    LfUIElementProps props = state.props_on_stack ? state.props_stack : state.theme.button_props;
    float padding = props.padding;
    float margin_left = props.margin_left, margin_right = props.margin_right,
        margin_top = props.margin_top, margin_bottom = props.margin_bottom; 

    vec4s color = props.color;
    vec4s text_color = props.text_color;
    LfFont font = state.font_stack ? *state.font_stack : state.theme.font;

    // If the button does not fit onto the current div, advance to the next line
    LfTextProps text_props = text_render_simple(state.pos_ptr, text, font, text_color, true);
    next_line_on_overflow(
        (vec2s){text_props.width + padding * 2.0f + margin_right + margin_left, 
                    text_props.height + padding * 2.0f + margin_bottom + margin_top});

    // Advancing the position pointer by the margins
    state.pos_ptr.x += margin_left;
    state.pos_ptr.y += margin_top;

    // Rendering the button
    LfClickableItemState ret = clickable_item(state.pos_ptr, (vec2s){text_props.width + padding * 2, text_props.height + padding * 2}, 
                                              props, color, props.border_width, true, true);
    // Rendering the text of the button
    text_render_simple((vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + text_props.height / 2.0f}, text, font, text_color, false);

    // Advancing the position pointer by the width of the button
    state.pos_ptr.x += text_props.width + margin_right + padding * 2.0f;
    state.pos_ptr.y -= margin_top;

    return ret; 
}
LfClickableItemState lf_image_button(LfTexture img) {
    // Retrieving the property data of the button
    LfUIElementProps props = state.props_on_stack ? state.props_stack : state.theme.button_props;
    float padding = props.padding;
    float margin_left = props.margin_left, margin_right = props.margin_right,
        margin_top = props.margin_top, margin_bottom = props.margin_bottom; 

    vec4s color = props.color;
    vec4s text_color = state.theme.button_props.text_color;
    LfFont font = state.font_stack ? *state.font_stack : state.theme.font;

    // If the button does not fit onto the current div, advance to the next line
    next_line_on_overflow(
        (vec2s){img.width + padding * 2.0f + margin_right + margin_left, 
                    img.height + padding * 2.0f + margin_bottom + margin_top});

    // Advancing the position pointer by the margins
    state.pos_ptr.x += margin_left;
    state.pos_ptr.y += margin_top;

    // Rendering the button
    LfClickableItemState ret = clickable_item(state.pos_ptr, (vec2s){img.width + padding * 2, img.height + padding * 2}, 
                                              props, color, props.border_width, true, true);
    // Rendering the text of the button
    lf_image_render((vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + padding}, (vec4s){1.0f, 1.0f, 1.0f, 1.0f}, img, (vec4s){0.0f, 0.0f, 0.0f, 0.0f}, 0, props.corner_radius);

    // Advancing the position pointer by the width of the button
    state.pos_ptr.x += img.width + margin_right + padding * 2.0f;
    state.pos_ptr.y -= margin_top;

    return ret; 
}
LfClickableItemState lf_button_fixed(const char* text, int32_t width, int32_t height) {
    // Retrieving the property data of the button
    LfUIElementProps props = state.props_on_stack ? state.props_stack : state.theme.button_props;
    float padding = props.padding;
    float margin_left = props.margin_left, margin_right = props.margin_right,
        margin_top = props.margin_top, margin_bottom = props.margin_bottom;

    vec4s color = props.color;
    vec4s text_color = props.text_color;
    LfFont font = state.font_stack ? *state.font_stack : state.theme.font;

    // If the button does not fit onto the current div, advance to the next line
    LfTextProps text_props = text_render_simple(state.pos_ptr, text, font, text_color, true);
    next_line_on_overflow(
        (vec2s){((width == -1) ? text_props.width : width + padding * 2.0f) + margin_right + margin_left,
            ((height == -1) ? text_props.height : height) + padding * 2.0f + margin_bottom + margin_top}); 

    // Advancing the position pointer by the margins
    state.pos_ptr.x += margin_left;
    state.pos_ptr.y += margin_top; 

    // Rendering the button
    LfClickableItemState ret = clickable_item(state.pos_ptr, 
        (vec2s){width == -1 ? text_props.width + padding * 2.0f : width + padding * 2, ((height == -1) ? text_props.height : height) + padding * 2}, props, 
                                              color, props.border_width, false, true);

    // Rendering the text of the button
    text_render_simple((vec2s)
        {state.pos_ptr.x + padding + ((width != -1) ? width / 2.0f - text_props.width / 2.0f : 0),
        state.pos_ptr.y + padding + ((height != -1) ? height / 2.0f - text_props.height / 2.0f : 0)
        }, text, font, text_color, false);

    // Advancing the position pointer by the width of the button
    state.pos_ptr.x += ((width == -1) ? text_props.width : width) + margin_right + padding * 2.0f;
    state.pos_ptr.y -= margin_top;
    return ret;
}

LfClickableItemState lf_div_begin(vec2s pos, vec2s size) {
    state.prev_div = state.current_div;
    state.prev_pos_ptr = state.pos_ptr;
    state.prev_font_stack = state.font_stack;
    state.prev_props_stack = state.props_stack;
    state.prev_line_height = state.current_line_height;

    LfUIElementProps props = state.props_on_stack ? state.props_stack : state.theme.div_props;
    state.div_props = props;
    LfDiv div = state.selected_div;
    div.aabb.size = (vec2s){size.x, size.y};
    div.aabb.pos = pos;
    div.init = true;
    div.id = state.div_index_ptr;
    state.current_div.interact_state = LF_IDLE;
    state.pos_ptr = (vec2s){pos.x + props.padding, pos.y + props.padding};

    state.current_div = div;

    div.interact_state = clickable_item((vec2s){pos.x - props.padding, pos.y - props.padding}, 
                                                      (vec2s){size.x + props.padding * 2.0f, size.y + props.padding * 2.0f}, props, props.color, 0, false, false);
    // Culling & Scrolling

    state.cull_start = (vec2s){-1, pos.y + props.padding};
    state.cull_end = (vec2s){-1, pos.y + size.y};
    lf_set_ptr_y(state.div_scrolls[state.div_index_ptr++]);

    // Reseting the div parameters
    state.current_div = div;
    state.current_line_height = 0;
    state.font_stack = NULL;
    state.props_on_stack = false;

    if(area_hovered(pos, size)) {
        state.selected_div_tmp = div;
    }

    return state.current_div.interact_state;
}
void lf_div_end() {
    state.pos_ptr = state.prev_pos_ptr;
    state.current_div = state.prev_div;
    state.font_stack = state.prev_font_stack;
    state.props_stack = state.prev_props_stack;
    state.current_line_height = state.prev_line_height;
    // Scrollbar
    if(state.div_index_ptr - 1 == state.selected_div.id) {
        LfUIElementProps props = lf_theme()->scrollbar_props;
        state.selected_div.total_area = state.pos_ptr; 

        float total_area = state.selected_div.total_area.y - state.div_scrolls[state.selected_div.id];
        float visible_area = state.selected_div.aabb.size.y + state.selected_div.aabb.pos.y;

        if(total_area > visible_area) 
        {
            const float scrollbar_width = 10;
            const float min_scrollbar_width = 10;

            float area_mapped = visible_area/total_area; 
            float scroll_mapped = (-1 * state.div_scrolls[state.selected_div.id])/total_area;

            // Limiting the scrollbar height 
            float scrollbar_height = (state.selected_div.aabb.size.y*area_mapped - props.margin_top * 2) > min_scrollbar_width ? 
                (state.selected_div.aabb.size.y*area_mapped - props.margin_top * 2) : min_scrollbar_width;

            clickable_item(
                (vec2s){
                    state.selected_div.aabb.pos.x + state.selected_div.aabb.size.x - scrollbar_width - props.margin_right,
                    (state.selected_div.aabb.pos.y + props.margin_top + state.selected_div.aabb.size.y*scroll_mapped) <  visible_area - scrollbar_height ? 
                    (state.selected_div.aabb.pos.y + props.margin_top + state.selected_div.aabb.size.y*scroll_mapped) : visible_area - scrollbar_height}, 
                (vec2s){
                    scrollbar_width, 
                    scrollbar_height}, 
                props,
                props.color, props.border_width, false, true);
        }
    }
    // Un-initializing the div
    state.cull_start = (vec2s){-1, -1};
    state.cull_end = (vec2s){-1, -1};   
}

void lf_next_line() {
    // Advancing the position pointer by the current line height
    state.pos_ptr.y += state.current_line_height;
    state.pos_ptr.x = state.current_div.aabb.pos.x + state.div_props.padding;    
    state.current_line_height = 0;
    state.gui_re_ev.happened = true;
}
vec2s lf_text_dimension(const char* str) {
    LfFont font = state.font_stack ? *state.font_stack : state.theme.font;
    LfTextProps props = text_render_simple((vec2s){0.0f, 0.0f}, str, font, state.theme.text_props.text_color, true);

    return (vec2s){props.width, props.height};
}

float lf_get_text_end(const char* str, float start_x) {
    LfFont font = state.font_stack ? *state.font_stack : state.theme.font;
    LfTextProps props = text_render_simple((vec2s){start_x, 0.0f}, str, font, state.theme.text_props.text_color, true);
    return props.end_x;
}

void lf_text(const char* text) {
    if(!state.current_div.init) {
        LF_ERROR("Trying to render without div context. Call lf_div_begin()");
        return;
    }
    // Retrieving the property data of the text
    LfUIElementProps props = state.props_on_stack ? state.props_stack : state.theme.text_props;
    float padding = props.padding;
    float margin_left = props.margin_left, margin_right = props.margin_right, 
        margin_top = props.margin_top, margin_bottom = props.margin_bottom;
    vec4s text_color = props.text_color;
    vec4s color = props.color;
    LfFont font = state.font_stack ? *state.font_stack : state.theme.font;

    // Advancing to the next line if the the text does not fit on the current div
    LfTextProps text_props = lf_text_render(state.pos_ptr, text, font, 
                                        state.text_wrap ? (state.current_div.aabb.size.x + state.current_div.aabb.pos.x) - margin_right * 2.0 : -1,
                                         -1, -1, -1, 1, -1, true, text_color);
    next_line_on_overflow(
        (vec2s){text_props.width + padding * 2.0f + margin_left + margin_right,
                    text_props.height + padding * 2.0f + margin_top + margin_bottom});

    // Advancing the position pointer by the margins
    state.pos_ptr.x += margin_left;
    state.pos_ptr.y += margin_top;

    // Rendering a colored text box if a color is specified
    // Rendering the text
    lf_text_render((vec2s){state.pos_ptr.x + padding, state.pos_ptr.y + padding}, text, font, 
                state.text_wrap ? (state.current_div.aabb.size.x + state.current_div.aabb.pos.x) - margin_right * 2.0f : -1, -1, -1, -1, -1, -1, false, text_color);

    // Advancing the position pointer by the width of the text
    state.pos_ptr.x += text_props.width + margin_right + padding;
    state.pos_ptr.y -= margin_top;
}

vec2s lf_get_div_size() {
    return state.current_div.aabb.size;
}

void lf_set_ptr_x(float x) {
    state.pos_ptr.x = x + state.current_div.aabb.pos.x;
}

void lf_set_ptr_y(float y) {
    state.pos_ptr.y = y + state.current_div.aabb.pos.y;
}
float lf_get_ptr_x() {
    return state.pos_ptr.x;
}

float lf_get_ptr_y() {
    return state.pos_ptr.y;
}

void lf_image(LfTexture tex) {
    if(!state.current_div.init) {
        LF_ERROR("Trying to render without div context. Call lf_div_begin()");
        return;
    }
    // Retrieving the property data of the image
    LfUIElementProps props = state.props_on_stack ? state.props_stack : state.theme.image_props;
    float margin_left = props.margin_left, margin_right = props.margin_right, 
        margin_top = props.margin_top, margin_bottom = props.margin_bottom;
    vec4s color = props.color;

    // Advancing to the next line if the image does not fit on the current div
    next_line_on_overflow((vec2s){tex.width + margin_left + margin_right, tex.height + margin_top + margin_bottom});

    // Advancing the position pointer by the margins
    state.pos_ptr.x += margin_left; 
    state.pos_ptr.y += margin_top;

    // Rendering the image
    lf_image_render(state.pos_ptr, color, tex, (vec4s){0.0f, 0.0f, 0.0f, 0.0f}, 0, props.corner_radius);

    // Advancing the position pointer by the width of the image
    state.pos_ptr.x += tex.width + margin_right;
    state.pos_ptr.y -= margin_top;
}
LfTheme* lf_theme() {
    return &state.theme;
}

void lf_begin() {
    state.pos_ptr = (vec2s){0, 0};
    lf_renderer_begin();
}
void lf_end() {
    state.selected_div = state.selected_div_tmp;
    state.div_index_ptr = 0;
    update_input();
    clear_events();
    lf_flush();
}
void lf_input_text(LfInputField* input) {
    input_field(input, INPUT_TEXT);
}

void lf_input_int(LfInputField *input) {
    input_field(input, INPUT_INT);
}
void lf_input_float(LfInputField* input) {
    input_field(input, INPUT_FLOAT);
}

void lf_set_text_wrap(bool wrap) {
    state.text_wrap = wrap;
}

void lf_push_font(LfFont* font) {
    state.font_stack = font;
}

void lf_pop_font() {
    state.font_stack = NULL;
}
void lf_checkbox(const char* text, bool* val, uint32_t tex) {        
    // Retrieving the property values of the checkbox
    LfUIElementProps props = state.props_on_stack ? state.props_stack : state.theme.checkbox_props;
    float margin_left = props.margin_left;
    float margin_right = props.margin_right;
    float margin_top = props.margin_top;
    float margin_bottom = props.margin_bottom;

    vec4s color = props.color;
    float checkbox_size = state.theme.font.font_size;

    // Render the text of the checkbox
    lf_text(text);

    // Advance to next line if the object does not fit on the div
    next_line_on_overflow((vec2s){checkbox_size + margin_left + margin_right, checkbox_size + margin_top + margin_bottom});
 
    state.pos_ptr.x += margin_left; 
    state.pos_ptr.y += margin_top;
    
    // Render the box 
    LfClickableItemState checkbox = clickable_item(state.pos_ptr, (vec2s){checkbox_size, checkbox_size}, 
                                                   state.theme.checkbox_props, color, props.border_width, true, false);

    // Change the value if the checkbox is clicked
    if(checkbox == LF_CLICKED) {
        *val = !*val;
    }
    if(*val) {
        // Render the image
        lf_image_render(state.pos_ptr, (vec4s){LF_WHITE}, (LfTexture){.id = tex, .width = (uint32_t)checkbox_size, .height = (uint32_t)checkbox_size}, (vec4s){0.0f, 0.0f, 0.0f, 0.0f}, 0, props.corner_radius);
    }
    state.pos_ptr.x += checkbox_size + margin_right;
    state.pos_ptr.y -= margin_top;
}
void lf_rect(float width, float height, vec4s color, float corner_radius) {
    // Rendering the rect
    lf_rect_render(state.pos_ptr, (vec2s){width, height}, color, (vec4s){0.0f, 0.0f, 0.0f, 0.0f}, 0, corner_radius);

    state.pos_ptr.x += width;
}

int map_vals(int value, int from_min, int from_max, int to_min, int to_max) {
    return (value - from_min) * (to_max - to_min) / (from_max - from_min) + to_min;
}
LfClickableItemState lf_slider_int(LfSlider* slider) {
    // Getting property data
    LfUIElementProps props = state.props_on_stack ? state.props_stack : state.theme.slider_props;
    float margin_left = props.margin_left, margin_right = props.margin_right,
    margin_top = props.margin_top, margin_bottom =props.margin_bottom; 
    // constants
    const int32_t slider_width = slider->width; // px
    const int32_t slider_height = slider->height; // px
    const int32_t handle_size = 20; // px 
    // Get the height of the element
    const int32_t element_height = int_max(handle_size, 
                                    slider_height); 

    vec4s color = props.color;

    next_line_on_overflow(  
        (vec2s){slider_width + margin_right + margin_left, 
                handle_size + margin_bottom + margin_top});

    state.pos_ptr.x += margin_left;
    state.pos_ptr.y += margin_top + (handle_size / 2.0f);

    // Render the slider 
    LfClickableItemState slider_state = clickable_item(state.pos_ptr, (vec2s){(float)slider_width, (float)slider_height}, 
                                                       props, color, 0, 
                                                       false, false);
   
    // Render the handle
    if(!slider->_init) {
        slider->_init = true;
        slider->handle_pos = map_vals(*(int32_t*)slider->val, slider->min, slider->max,
                                          handle_size / 2.0f, slider->width - handle_size / 2.0f) - (handle_size) / 2.0f;
    }
    LfClickableItemState handle = clickable_item((vec2s){state.pos_ptr.x + slider->handle_pos, state.pos_ptr.y - (handle_size) / 2.0f + slider_height / 2.0f}, 
                                                 (vec2s){handle_size, handle_size}, props, color, props.border_width, false, false);
    
    // Check if the slider bar is pressed

    if(slider_state == LF_CLICKED) {
        slider->handle_pos = lf_get_mouse_x() - state.pos_ptr.x;
        *(int32_t*)slider->val = map_vals(state.pos_ptr.x + slider->handle_pos, state.pos_ptr.x,  state.pos_ptr.x + slider_width - (handle_size / 2.0f), 
                                          slider->min, slider->max);
    }
    if(handle == LF_CLICKED) {
        slider->held = true;
    }
    if(slider->held && lf_mouse_button_is_released(GLFW_MOUSE_BUTTON_LEFT)) {
        slider->held = false;
    }
    if(slider->held) {
        if(lf_get_mouse_x() >= state.pos_ptr.x && lf_get_mouse_x() <= state.pos_ptr.x + slider_width - handle_size) {
            slider->handle_pos = lf_get_mouse_x() - state.pos_ptr.x;
            *(int32_t*)slider->val = map_vals(state.pos_ptr.x + slider->handle_pos, state.pos_ptr.x,  state.pos_ptr.x + slider_width - handle_size, 
                                              slider->min, slider->max);
        } else if(lf_get_mouse_x() <= state.pos_ptr.x) {
            *(int32_t*)slider->val = slider->min;
            slider->handle_pos = 0;
        } else if(lf_get_mouse_x() >= state.pos_ptr.x + slider_width) {
            *(int32_t*)slider->val = slider->max;
            slider->handle_pos = slider_width - handle_size;
        }
    }
    state.pos_ptr.x += slider_width + margin_right;
    state.pos_ptr.y -= margin_top + (handle_size / 2.0f);
    return handle;
}

LfClickableItemState lf_progress_bar_int(LfSlider* slider) {
    // Getting property data
    LfUIElementProps props = state.props_on_stack ? state.props_stack : state.theme.slider_props;
    float margin_left = props.margin_left, margin_right = props.margin_right,
    margin_top = props.margin_top, margin_bottom =props.margin_bottom; 
    // constants
    const int32_t height = slider->height; // px

    vec4s color = props.color;

    next_line_on_overflow( 
        (vec2s){slider->width + margin_right + margin_left, 
                height + margin_bottom + margin_top });

    state.pos_ptr.x += margin_left;
    state.pos_ptr.y += margin_top;

    // Render the slider 
    LfClickableItemState bar = clickable_item(state.pos_ptr, (vec2s){(float)slider->width, (float)height},props, color, props.border_width, false, false);
    
    // Check if the slider bar is pressed
    slider->handle_pos = map_vals(*(int32_t*)slider->val, slider->min, slider->max,
                                  0, slider->width);
    LfClickableItemState handle = clickable_item(state.pos_ptr, (vec2s){(float)slider->handle_pos, (float)height}, props, props.text_color, 0, false, false);

    state.pos_ptr.x += slider->width + margin_right;
    state.pos_ptr.y -= margin_top;
    return bar;
}

LfClickableItemState lf_progress_stripe_int(LfSlider* slider) {
    // Getting property data
    LfUIElementProps props = state.props_on_stack ? state.props_stack : state.theme.slider_props;
    float margin_left = props.margin_left, margin_right = props.margin_right,
    margin_top = props.margin_top, margin_bottom =props.margin_bottom; 
    // constants

    vec4s color = props.color;

    next_line_on_overflow( 
        (vec2s){slider->width + margin_right + margin_left, 
                slider->height + margin_bottom + margin_top});

    state.pos_ptr.x += margin_left;
    state.pos_ptr.y += margin_top;

    // Render the slider 
    LfClickableItemState bar = clickable_item(state.pos_ptr, (vec2s){(float)slider->width, (float)slider->height},props, color, props.border_width, false, false);
    
    // Check if the slider bar is pressed
    slider->handle_pos = map_vals(*(int32_t*)slider->val, slider->min, slider->max,
                                  0, slider->width);
    LfClickableItemState handle = clickable_item(state.pos_ptr, (vec2s){(float)slider->handle_pos, (float)slider->height}, props, props.text_color, 0, false, false);
    lf_rect_render((vec2s){state.pos_ptr.x + slider->handle_pos, state.pos_ptr.y - (float)slider->height / 2.0f}, (vec2s){(float)slider->height * 2, (float)slider->height * 2}, props.text_color, (vec4s){0.0f, 0.0f, 0.0f, 0.0f}, 0, props.corner_radius);

    state.pos_ptr.x += slider->width + margin_right;
    state.pos_ptr.y -= margin_top;
    return bar;
}
int32_t lf_menu_item_list(const char** items, uint32_t item_count, int32_t selected_index, vec4s selected_color, LfMenuItemCallback per_cb, bool vertical) {
    LfUIElementProps props = state.props_on_stack ? state.props_stack : state.theme.button_props;
    float padding = props.padding;
    float margin_left = props.margin_left, margin_right = props.margin_right,
    margin_top = props.margin_top, margin_bottom = props.margin_bottom; 
    vec4s color = props.color;
    LfFont font = state.font_stack ? *state.font_stack : state.theme.font;
    
    LfTextProps text_props[item_count];
    uint32_t width = 0;
    for(uint32_t i  = 0; i < item_count; i++) {
        text_props[i] = text_render_simple((vec2s){state.pos_ptr.x, state.pos_ptr.y + margin_top},items[i], font, props.text_color, true);
        width += text_props[i].width + padding * 2.0f;
    }
    next_line_on_overflow(
        (vec2s){width + padding * 2.0f + margin_right + margin_left, 
                    font.font_size + padding * 2.0f + margin_bottom + margin_top});


    state.pos_ptr.y += margin_top; 
    state.pos_ptr.x += margin_left;

    uint32_t element_width = 0;
    uint32_t clicked_item = -1;
    for(uint32_t i = 0; i < item_count; i++) {
        LfUIElementProps props = state.theme.button_props;
        props.margin_left = 0;
        props.margin_right = 0;
        LfUIElementProps button_props = state.theme.button_props;
        lf_push_style_props(props);
        if(i == selected_index) {
            props.color = selected_color; 
        }
        lf_push_style_props(props);
        if(lf_button(items[i]) == LF_CLICKED) {
            clicked_item = i;  
        } 
        lf_pop_style_props();
        per_cb(&i);
    } 
    next_line_on_overflow((vec2s){element_width + margin_right, font.font_size + margin_top + margin_bottom});
 
    
    state.pos_ptr.y -= margin_top;
    return clicked_item;
}
void lf_push_style_props(LfUIElementProps props) {
    state.props_stack = props;
    state.props_on_stack = true;
}

void lf_pop_style_props() {
    state.props_on_stack = false;
}
void lf_push_text_start_x(int32_t start_x) {
    state.text_start_x_stack = start_x;
}

void lf_push_text_stop_x(int32_t stop_x) {
    state.text_stop_x_stack = stop_x;
}

void lf_push_text_start_y(int32_t start_y) {
    state.text_start_y_stack = start_y;
}

void lf_push_text_stop_y(int32_t stop_y) {
    state.text_stop_y_stack = stop_y;
}
void lf_pop_text_start_x() {
    state.text_start_x_stack = -2;
}

void lf_pop_text_stop_x() {
    state.text_stop_x_stack = -2;
}

void lf_pop_text_start_y() {
    state.text_start_y_stack = -2;
}

void lf_pop_text_stop_y() {
    state.text_stop_y_stack = -2;
}

LfCursorPosEvent lf_mouse_move_event() {
    return state.cp_ev;
}

LfMouseButtonEvent lf_mouse_button_event() {
    return state.mb_ev;
}

LfScrollEvent lf_mouse_scroll_event() {
    return state.scr_ev;
}

LfKeyEvent lf_key_event_event() {
    return state.key_ev;
}

LfCharEvent lf_char_event() { 
    return state.ch_ev;
}

LfGuiReEstablishEvent lf_gui_reastablish() {
    return state.gui_re_ev;
}

void lf_set_cull_end_x(float x) {
    state.cull_end.x = x; 
}

void lf_set_cull_end_y(float y) {
    state.cull_end.y = y; 
}
void lf_set_cull_start_x(float x) {
    state.cull_start.x = x;
}

void lf_set_cull_start_y(float y) {
    state.cull_start.y = y;
}

void lf_unset_cull_start_x() {
    state.cull_start.x = -1;
}

void lf_unset_cull_start_y() {
    state.cull_start.y = -1;
}

void lf_unset_cull_end_x() {
    state.cull_end.x = -1;
}

 void lf_unset_cull_end_y() {
    state.cull_end.y = -1;
}
