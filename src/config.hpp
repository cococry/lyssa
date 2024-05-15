#pragma once 
    
extern "C" {
    #include <leif/leif.h>
}

// Lyssa Path 
#ifdef _WIN32
#define HOMEDIR "USERPROFILE"
#else
#define HOMEDIR "HOME"
#endif

#define LYSSA_DIR std::string(getenv(HOMEDIR)) + std::string("/.lyssa")

// Window 
#define WIN_START_W 1280 
#define WIN_START_H 720 
#define WIN_MIN_W 1050
#define WIN_MIN_H 450

#define WIN_VSYNC false

#define TARGET_FRAME_RATE 144

// Divs
#define DIV_START_X 20.0f
#define DIV_START_Y 20.0f


// Back button 
#define BACK_BUTTON_WIDTH 20
#define BACK_BUTTON_HEIGHT 40
#define BACK_BUTTON_MARGIN_BOTTOM 50

// General colors

#define NIGHT (LfColor){20, 23, 28, 255} 
#define GRAY (LfColor){60, 64, 72, 255}
#define DARK_SLATE_GRAY lf_color_from_hex(0x3A4E48)
#define BLUE_GRAY lf_color_from_hex(0x6A7B76)
#define CAMBRIDGE_BLUE lf_color_from_hex(0x8B9D83)
#define SILVER lf_color_from_hex(0xBEB0A7)
#define PERSIAN_GREEN lf_color_from_hex(0x339989)
#define PURPLE lf_color_from_hex(0x6a55e0)

#define LYSSA_GREEN (LfColor){13, 181, 108, 255}
#define LYSSA_BLUE  (LfColor){83, 150, 237, 255}
#define LYSSA_RED  (LfColor){163, 36, 50, 255}

// Colors 
#define LYSSA_BACKGROUND_COLOR NIGHT
#define LYSSA_PLAYLIST_COLOR PURPLE

// Scrolling 
#define DIV_SMOOTH_SCROLL true

// Playlist file thumbnails 
#define PLAYLIST_FILE_THUMBNAIL_CORNER_RADIUS 0.0f // Setting this to > 0 will worsen perfomance on low-end systems
#define PLAYLIST_ON_TRACK_CORNER_RADIUS PLAYLIST_FILE_THUMBNAIL_CORNER_RADIUS * 4.0f
#define PLAYLIST_FILE_THUMBNAIL_COLOR GRAY
#define PLAYLIST_FILE_THUMBNAIL_SIZE (vec2s){48, 48}

// Volume
#define VOLUME_TOGGLE_STEP 5
#define VOLUME_MAX 100.0f 
#define VOLUME_INIT 75.0f
#define VOLUME_SHOW_PERCENT true 

// Buffers
#define INPUT_BUFFER_SIZE 512

// Info cards 
#define MAX_INFO_CARDS 5

// Fullscreen Tab 
#define FULLSCREEN_TRACK_UI_TIME 3.0f // Time in seconds till the UI of fullscreend track disappears 

// Styles & Theming 
inline LfUIElementProps primary_button_style() {
    LfUIElementProps props = lf_get_theme().button_props;
    props.corner_radius = 6;
    props.border_width = 0;
    props.color = lf_color_brightness(GRAY, 1.8f); 
    return props;
}

inline LfUIElementProps secondary_button_style() {
    LfUIElementProps props = lf_get_theme().button_props;
    props.corner_radius = 9;
    props.border_width = 0;
    props.color = lf_color_brightness(GRAY, 1.8f); 
    return props;
}


inline LfUIElementProps call_to_action_button_style() {
    LfUIElementProps props = lf_get_theme().button_props;
    props.color = PERSIAN_GREEN;
    props.text_color = LF_BLACK;
    props.corner_radius = 6;
    props.border_width = 0;
    return props;
}

inline LfUIElementProps input_field_style() {
    LfUIElementProps props = lf_get_theme().inputfield_props;
    props.padding = 15; 
    props.border_width = 0;
    props.color = GRAY; 
    props.corner_radius = 4;
    props.text_color = LF_WHITE;
    return props;
}

inline LfTheme ui_theme() {
    LfTheme theme = lf_get_theme();
    theme.div_props.color = LF_NO_COLOR;
    theme.scrollbar_props.corner_radius = 1.5;
    theme.div_smooth_scroll = DIV_SMOOTH_SCROLL;
    return theme;
}

// Async loading
#define ASYNC_PLAYLIST_LOADING false 
#define MIN_FILES_FOR_ASYNC 10
