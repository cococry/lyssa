#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <cglm/cglm.h>
#include <cglm/struct.h>
#include <wchar.h>

#define LF_RGBA(r, g, b, a) r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f
#define LF_ZTO_TO_RGBA(r, g, b, a) r * 255.0f, g * 255.0f, b * 255.0f, a * 255.0f

#define LF_RED (vec4s){LF_RGBA(255.0f, 0.0f, 0.0f, 255.0f)}
#define LF_GREEN (vec4s){LF_RGBA(0.0f, 255.0f, 0.0f, 255.0f)}
#define LF_BLUE (vec4s){LF_RGBA(0.0f, 0.0f, 255.0f, 255.0f)}
#define LF_WHITE (vec4s){LF_RGBA(255.0f, 255.0f, 255.0f, 255.0f)}
#define LF_BLACK (vec4s){LF_RGBA(0.0f, 0.0f, 0.0f, 255.0f)}
#define LF_NO_COLOR (vec4s){LF_RGBA(0.0f, 0.0f, 0.0f, 0.0f)}

#define LF_COLOR_BRIGHTNESS(color, brightness) (vec4s){LF_RGBA(color.r * (float)brightness, color.g * (float)brightness, color.b * (float)brightness, color.a)}
#define LF_SCROLL_AMOUNT 20
#define LF_MAX_DIVS 64

#define LF_PRIMARY_ITEM_COLOR (vec4s){LF_RGBA(133, 138, 148, 255)} 
#define LF_SECONDARY_ITEM_COLOR (vec4s){LF_RGBA(96, 100, 107, 255)}

// --- Events ---
typedef struct {
    int32_t keycode;
    bool happened, pressed;
} LfKeyEvent;

typedef struct {
    int32_t button_code;
    bool happened, pressed;
} LfMouseButtonEvent;

typedef struct {
    int32_t x, y;
    bool happened;
} LfCursorPosEvent;

typedef struct {
    int32_t xoffset, yoffset;
    bool happened;
} LfScrollEvent;

typedef struct {
    int32_t charcode;
    bool happened;
} LfCharEvent;

typedef struct {
    bool happened;
} LfGuiReEstablishEvent;

typedef struct {
    uint32_t id;
    uint32_t width, height;
} LfTexture;
typedef struct {
    void* cdata;
    void* font_info;
    uint32_t tex_width, tex_height;
    uint32_t line_gap_add, font_size;
    LfTexture bitmap;
} LfFont;

typedef enum {
    LF_TEX_FILTER_LINEAR = 0,
    LF_TEX_FILTER_NEAREST
} LfTextureFiltering;

typedef struct {
    uint32_t width, height;
    uint32_t char_count;
    bool reached_stop, reached_max_wraps;
    int32_t end_x, start_x, end_y;
} LfTextProps;

typedef struct {
    int32_t cursor_index, width, height, start_height;
    char* buf;
    void* val;
    char* placeholder;
    bool selected;
    bool expand_on_overflow;
    bool reached_stop;

    uint32_t max_chars;

    void (*char_callback)(char);
    
} LfInputField;

typedef struct {
    void* val;
    int32_t handle_pos;
    bool _init;
    float min, max;
    bool held, selcted;
    uint32_t width;
    uint32_t height;
    uint32_t handle_size;
    vec4s handle_color;
} LfSlider;

typedef enum {
    LF_RELEASED = -1,
    LF_IDLE = 0,
    LF_HOVERED = 1,
    LF_CLICKED = 2,
    LF_HELD = 3,
} LfClickableItemState;


typedef struct {
    vec4s color;
    vec4s text_color;
    vec4s border_color;
    float padding;
    float margin_left;
    float margin_right;
    float margin_top;
    float margin_bottom;
    float border_width;
    float corner_radius;
} LfUIElementProps;

typedef struct {
    vec2s pos, size;
} LfAABB;

typedef struct {
    LfUIElementProps button_props, div_props, text_props, image_props,
                     inputfield_props, checkbox_props, slider_props, scrollbar_props;
    LfFont font;
} LfTheme;

typedef struct {
    int32_t id;

    LfAABB aabb;
    LfClickableItemState interact_state;

    bool init, hidden;
    
    float scroll;

    vec2s total_area;
} LfDiv;

typedef void (*LfMenuItemCallback)(uint32_t*);

void lf_init_glfw(uint32_t display_width, uint32_t display_height, LfTheme* theme, void* glfw_window);

void lf_terminate();

LfTheme lf_default_theme();

void lf_resize_display(uint32_t display_width, uint32_t display_height);

LfFont lf_load_font(const char* filepath, uint32_t size);

LfTexture lf_load_texture(const char* filepath, bool flip, LfTextureFiltering filter);

LfTexture lf_load_texture_from_memory(const void* data, uint32_t size, bool flip, LfTextureFiltering filter);

void lf_free_texture(LfTexture tex);

void lf_free_font(LfFont* font);

void lf_add_key_callback(void* cb);

void lf_add_mouse_button_callback(void* cb);

void lf_add_scroll_callback(void* cb);

void lf_add_cursor_pos_callback(void* cb);

bool lf_key_went_down(uint32_t key);

bool lf_key_is_down(uint32_t key);

bool lf_key_is_released(uint32_t key);

bool lf_key_changed(uint32_t key);

bool lf_mouse_button_went_down(uint32_t button);

bool lf_mouse_button_is_down(uint32_t button);

bool lf_mouse_button_is_released(uint32_t button);

bool lf_mouse_button_changed(uint32_t button);

bool lf_mouse_button_went_down_on_div(uint32_t button);

bool lf_mouse_button_is_released_on_div(uint32_t button);

bool lf_mouse_button_changed_on_div(uint32_t button);

double lf_get_mouse_x();

double lf_get_mouse_y();

double lf_get_mouse_x_delta();

double lf_get_mouse_y_delta();

double lf_get_mouse_scroll_x();

double lf_get_mouse_scroll_y();

#define lf_div_begin(pos, size) _lf_div_begin_loc(pos, size, __FILE__, __LINE__)
LfDiv* _lf_div_begin_loc(vec2s pos, vec2s size, const char* file, int32_t line);

void lf_div_end();

#define lf_button(text) _lf_button_loc(text, __FILE__, __LINE__)
LfClickableItemState _lf_button_loc(const char* text, const char* file, int32_t line);

#define lf_image_button(img) _lf_image_button_loc(img, __FILE__, __LINE__)
LfClickableItemState _lf_image_button_loc(LfTexture img, const char* file, int32_t line);

#define lf_button_fixed(text, width, height) _lf_button_fixed_loc(text, width, height, __FILE__, __LINE__)
LfClickableItemState _lf_button_fixed_loc(const char* text, int32_t width, int32_t height, const char* file, int32_t line);

#define lf_slider_int(slider) _lf_slider_int_loc(slider, __FILE__, __LINE__)
LfClickableItemState _lf_slider_int_loc(LfSlider* slider, const char* file, int32_t line);

#define lf_progress_bar_val(width, height, min, max, val) _lf_progress_bar_val_loc(width, height, min, max, val, __FILE__, __LINE__)
LfClickableItemState _lf_progress_bar_val_loc(int32_t width, int32_t height, int32_t min, int32_t max, int32_t val, const char* file, int32_t line);

#define lf_progress_bar_int(slider) _lf_progress_bar_int_loc(slider , __FILE__, __LINE__)
LfClickableItemState _lf_progress_bar_int_loc(LfSlider* slider, const char* file, int32_t line);

#define lf_progress_stripe_int(slider) _lf_progresss_stripe_int_loc(slider , __FILE__, __LINE__)
LfClickableItemState _lf_progress_stripe_int_loc(LfSlider* slider, const char* file, int32_t line);

#define lf_checkbox(text, val, tick_color, tex_color) _lf_checkbox_loc(text, val, tick_color, tex_color, __FILE__, __LINE__)
LfClickableItemState _lf_checkbox_loc(const char* text, bool* val, vec4s tick_color, vec4s tex_color, const char* file, int32_t line);

void lf_next_line();

vec2s lf_text_dimension(const char* str);

vec2s lf_text_dimension_wide(const wchar_t* str);

float lf_get_text_end(const char* str, float start_x);

void lf_text(const char* text);

void lf_text_wide(const wchar_t* text);

vec2s lf_get_div_size();

LfDiv lf_get_current_div();

LfDiv lf_get_selected_div();

void lf_set_ptr_x(float x);

void lf_set_ptr_y(float y);

float lf_get_ptr_x();

float lf_get_ptr_y();

void lf_image(LfTexture tex);

LfTheme* lf_theme();

#define lf_begin() _lf_begin_loc(__FILE__, __LINE__)
void _lf_begin_loc(const char* file, int32_t line);

void lf_end();

#define lf_input_text(input) _lf_input_text_loc(input, __FILE__, __LINE__)
void _lf_input_text_loc(LfInputField* input, const char* file, int32_t line);

#define lf_input_int(input) _lf_input_int_loc(input, __FILE__, __LINE__)
void _lf_input_int_loc(LfInputField* input, const char* file, int32_t line);

#define lf_input_float(input) _lf_input_float_loc(input, __FILE__, __LINE__)
void _lf_input_float_loc(LfInputField* input, const char* file, int32_t line);

void lf_set_text_wrap(bool wrap);

void lf_push_font(LfFont* font);

void lf_pop_font();

void lf_rect(uint32_t width, uint32_t height, vec4s color, float corner_radius);

#define lf_menu_item_list(items, item_count, selected_index, per_cb, vertical) _lf_menu_item_list_loc(__FILE__, __LINE__, items, item_count, selected_index, per_cb, vertical)
int32_t _lf_menu_item_list_loc(const char* file, int32_t line, const char** items, uint32_t item_count, int32_t selected_index, LfMenuItemCallback per_cb, bool vertical);

#define lf_dropdown_menu(items, placeholder, item_count, width, height, selected_index, opened) _lf_dropdown_menu_loc(items, placeholder, item_count, width, height, selected_index, opened, __FILE__, __LINE__)
void _lf_dropdown_menu_loc(const char** items, const char* placeholder, uint32_t item_count, int32_t width, int32_t height, int32_t* selected_index, bool* opened, const char* file, int32_t line);

LfTextProps lf_text_render(vec2s pos, const char* str, LfFont font, int32_t wrap_point,
        int32_t stop_point_x, int32_t start_point_x, int32_t stop_point_y, int32_t start_point_y, int32_t max_wrap_count, bool no_render, vec4s color);

LfTextProps lf_text_render_wchar(vec2s pos, const wchar_t* str, LfFont font, int32_t wrap_point,
        int32_t stop_point_x, int32_t start_point_x, int32_t stop_point_y, int32_t start_point_y, int32_t max_wrap_count, bool no_render, vec4s color);

void lf_rect_render(vec2s pos, vec2s size, vec4s color, vec4s border_color, float border_width, float corner_radius);

void lf_image_render(vec2s pos, vec4s color, LfTexture tex, vec4s border_color, float border_width, float corner_radius);

bool lf_point_intersects_aabb(vec2s p, LfAABB aabb);

bool lf_aabb_intersects_aabb(LfAABB a, LfAABB b);

void lf_push_style_props(LfUIElementProps props);

void lf_pop_style_props();

void lf_set_image_color(vec4s color);

void lf_unset_image_color();

bool lf_hovered(vec2s pos, vec2s size);

void lf_flush();

void lf_renderer_begin();

LfCursorPosEvent lf_mouse_move_event();

LfMouseButtonEvent lf_mouse_button_event();

LfScrollEvent lf_mouse_scroll_event();

LfKeyEvent lf_key_event();

LfCharEvent lf_char_event();

LfGuiReEstablishEvent lf_gui_reastablish_event();

void lf_set_cull_start_x(float x);

void lf_set_cull_start_y(float y);

void lf_set_cull_end_x(float x);

void lf_set_cull_end_y(float y);  

void lf_unset_cull_start_x();

void lf_unset_cull_start_y();

void lf_unset_cull_end_x();

void lf_unset_cull_end_y();

void lf_div_hide();

void lf_div_hide_index(uint32_t i);

uint32_t lf_get_div_count();

LfDiv* lf_get_divs();

void lf_set_div_storage(bool storage);

void lf_set_div_cull(bool cull);

void lf_set_line_height(uint32_t line_height);

uint32_t lf_get_line_height();

void lf_set_line_should_overflow(bool overflow);

void lf_set_div_hoverable(bool hoverable);

void lf_push_element_id(int64_t id);

void lf_pop_element_id();
