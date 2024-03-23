#include "popups.hpp"
#include "config.hpp"
#include "global.hpp"
extern "C" {
    #include <leif.h>
}
#include "playlists.hpp"
#include "soundTagParser.hpp"

#include <cstring>
#include <fstream>

void EditPlaylistPopup::render() {
    static char nameBuf[INPUT_BUFFER_SIZE] = {0};
    static char descBuf[INPUT_BUFFER_SIZE] = {0};
    static bool initBufs = false;
    if(!initBufs) {
        memcpy(nameBuf, state.playlists[state.currentPlaylist].name.c_str(), INPUT_BUFFER_SIZE);
        memcpy(descBuf, state.playlists[state.currentPlaylist].desc.c_str(), INPUT_BUFFER_SIZE);
        initBufs = true;
    }

    // Beginning a new div
    const vec2s popupSize = (vec2s){500.0f, 350.0f};
    LfUIElementProps div_props = lf_get_theme().div_props;
    div_props.color = lf_color_brightness(GRAY, 0.7); 
    div_props.border_width = 0;
    div_props.padding = 5;
    div_props.corner_radius = 10;
    lf_push_style_props(div_props);
    // Centering the div/popup
    lf_div_begin(((vec2s){(state.win->getWidth() - popupSize.x) / 2.0f, (state.win->getHeight() - popupSize.y) / 2.0f}), popupSize, false);
    if(!lf_div_grabbed()) {
        lf_div_grab(lf_get_current_div());
    }
    // Close Button
    {
        // Put the X Button in the top left of the div 

        // Styling
        LfUIElementProps props = lf_get_theme().button_props;
        props.margin_left = 0;
        props.margin_right = 0;
        props.margin_top = 0;
        props.margin_bottom = 0;
        props.border_width = 0;
        props.text_color = LF_WHITE;
        props.color = LF_NO_COLOR;

        lf_push_style_props(props);
        if(lf_button("X") == LF_CLICKED) {
            this->shouldRender = false;
            lf_div_ungrab();

            memset(nameBuf, 0, INPUT_BUFFER_SIZE);
            memset(descBuf, 0, INPUT_BUFFER_SIZE);
            initBufs = false;
        }
        lf_pop_style_props();
    }

    lf_next_line();
    {
        LfUIElementProps props = lf_get_theme().text_props;
        props.margin_left = 20;
        props.margin_bottom = 5;
        lf_push_style_props(props);
        lf_text("Name");
        lf_pop_style_props();
    }
    lf_next_line();
    { 
        state.createPlaylistTab.nameInput.input.width = 457;
        LfUIElementProps props = input_field_style();
        props.color = lf_color_brightness(GRAY, 0.4);
        lf_push_style_props(props);
        lf_input_text_inl_ex(nameBuf, INPUT_BUFFER_SIZE, (int32_t)(popupSize.x - 50), "");
        lf_pop_style_props();
        lf_next_line();
    }

    lf_next_line();
    {
        LfUIElementProps props = lf_get_theme().text_props;
        props.margin_left = 20;
        props.margin_bottom = 5;
        props.margin_top = 15;
        lf_push_style_props(props);
        lf_text("Description");
        lf_pop_style_props();
    }
    lf_next_line();
    { 
        state.createPlaylistTab.descInput.input.width = 457;
        LfUIElementProps props = input_field_style(); 
        props.color = lf_color_brightness(GRAY, 0.4);
        lf_push_style_props(props);
        lf_input_text_inl_ex(descBuf, INPUT_BUFFER_SIZE, (int32_t)(popupSize.x - 50), "");
        lf_pop_style_props();
        lf_next_line();
    }
    lf_next_line();
    {
        LfUIElementProps props = primary_button_style();
        props.margin_top = 15;
        lf_push_style_props(props);
        if(lf_button_fixed("Done", 150, -1) == LF_CLICKED) {
            Playlist::rename(std::string(nameBuf), state.currentPlaylist);
            Playlist::changeDesc(std::string(descBuf), state.currentPlaylist);
            this->shouldRender = false;

            memset(nameBuf, 0, INPUT_BUFFER_SIZE);
            memset(descBuf, 0, INPUT_BUFFER_SIZE);
            lf_div_ungrab();
        }
    }

    lf_div_end();
    lf_pop_style_props();

}

void PlaylistFileDialoguePopup::render() {
    if(!this->shouldRender) return;
    const vec2s popupSize =(vec2s){200, 200};

    LfUIElementProps props = lf_get_theme().div_props;
    props.color = lf_color_brightness(GRAY, 0.35);
    props.corner_radius = 4;
    lf_push_style_props(props);
    lf_div_begin(this->pos, popupSize, false);
    if(!lf_div_grabbed()) {
        lf_div_grab(lf_get_current_div());
    }
    lf_pop_style_props();

    static bool showPlaylistPopup = false;
    static bool onPlaylistPopup = false;

    const uint32_t options_count = 4;
    static const char* options[options_count] = {
        "Add to playlist",
        "Remove",
        "Add to favourites",
        "Open URL..."
    };

    int32_t clickedIndex = -1;
    for(uint32_t i = 0; i < options_count; i++) {
        // Option
        props = lf_get_theme().text_props;
        props.hover_text_color = lf_color_brightness(GRAY, 2);
        lf_push_style_props(props);
        if(lf_button(options[i]) == LF_CLICKED) {
            clickedIndex = i;
        }
        lf_pop_style_props();

        // Seperator
        props = lf_get_theme().button_props;
        props.color = lf_color_brightness(GRAY, 0.7);
        lf_push_style_props(props);
        lf_seperator();
        lf_pop_style_props();

        lf_next_line();
    }

    switch(clickedIndex) {
        case 0:
            {
                showPlaylistPopup = !showPlaylistPopup;
                break;
            }
        case 1: /* Remove */
            {
                if(state.currentSoundFile != nullptr) {
                    if(state.currentSoundFile->path == this->path) {
                        state.soundHandler.stop();
                        state.soundHandler.uninit();
                        state.currentSoundFile = nullptr;
                    }
                }
                Playlist::removeFile(this->path, state.currentPlaylist);
                this->shouldRender = false;
                break;
            }
        case 2: /* Add to favourites */
            {
                break;
            }
        case 3:
            {
                std::string url = SoundTagParser::getSoundComment(this->path.string());
                if(url != "") {
                    std::string cmd = "xdg-open " + url + "& ";
                    system(cmd.c_str());
                }
                break;
            }
        default:
            break;
    }

    if(lf_get_current_div().id != lf_get_selected_div().id && !onPlaylistPopup && lf_mouse_button_is_released(GLFW_MOUSE_BUTTON_LEFT)) {
        this->shouldRender = false;
        showPlaylistPopup = false;
        lf_div_ungrab();
    }
    lf_div_end();

    if(showPlaylistPopup) {
        LfUIElementProps div_props = lf_get_theme().div_props;
        div_props.color = lf_color_brightness(GRAY, 0.35);
        div_props.corner_radius = 4;
        div_props.padding = 10;
        lf_push_style_props(div_props);
        lf_div_begin(((vec2s){this->pos.x + popupSize.x + div_props.padding, this->pos.y}), popupSize, true);
        onPlaylistPopup = lf_get_current_div().id == lf_get_selected_div().id;
        lf_pop_style_props();

        lf_set_cull_end_x(this->pos.x + popupSize.x * 2 + div_props.padding * 2);

        uint32_t renderedItems = 0;
        for(uint32_t i = 0; i < state.playlists.size(); i++) {
            Playlist& playlist = state.playlists[i];
            std::vector<std::string> playlistFiles = PlaylistMetadata::getFilepaths(std::filesystem::directory_entry(playlist.path));

            if(i == state.currentPlaylist || std::find(playlistFiles.begin(), playlistFiles.end(), this->path.string()) != playlistFiles.end()) continue;
            // Playlist
            props = lf_get_theme().text_props;
            props.hover_text_color = lf_color_brightness(GRAY, 2);
            lf_push_style_props(props);
            if(lf_button(playlist.name.c_str()) == LF_CLICKED) {
                if(playlist.loaded) {
                    Playlist::addFile(this->path, i);
                    playlist.loaded = false;
                } else {
                    std::ofstream metadata(playlist.path.string() + "/.metadata", std::ios::app);
                    metadata.seekp(0, std::ios::end);

                    metadata << "\"" << this->path << "\" ";
                    metadata.close();
                    playlist.loaded = false;
                }
                this->shouldRender = false;
                showPlaylistPopup = false;
            }
            lf_pop_style_props();

            // Seperator
            props = lf_get_theme().button_props;
            props.color = lf_color_brightness(GRAY, 0.7);
            lf_push_style_props(props);
            lf_seperator();
            lf_pop_style_props();

            lf_next_line();
            renderedItems++;
        }
        if(renderedItems == 0) {
            lf_text("No other playlists");
        }
        lf_unset_cull_end_x();
        lf_div_end();
    }
    
} 

void TwoChoicePopup::render() {
    if(!this->shouldRender) return;
    // Beginning a new div
    const vec2s popupSize = (vec2s){(float)this->width, 100.0f};
    LfUIElementProps props = lf_get_theme().div_props;
    props.color = lf_color_brightness(GRAY, 0.45);
    props.padding = 0;
    props.corner_radius = 10;
    lf_push_style_props(props);
    // Centering the div/popup
    lf_div_begin(((vec2s){(state.win->getWidth() - popupSize.x) / 2.0f, (state.win->getHeight() - popupSize.y) / 2.0f}), popupSize, false); 
    if(!lf_div_grabbed()) {
        lf_div_grab(lf_get_current_div());
    }

    // Close Button
    {
        // Styling
        LfUIElementProps props = lf_get_theme().button_props;
        props.margin_left = 0;
        props.margin_right = 0;
        props.margin_top = 0;
        props.margin_bottom = 0;
        props.text_color = (LfColor){255, 255, 255, 255};
        props.color = LF_NO_COLOR;
        props.border_width = 0;

        lf_push_style_props(props);
        if(lf_button("X") == LF_CLICKED) {
            this->shouldRender = false;
            lf_div_ungrab();
        }
        lf_pop_style_props();
        lf_next_line();
        lf_set_ptr_y(20);
    }
    // Popup Title
    {
        const char* text = this->title.c_str();
        lf_push_font(&state.h6Font);
        float textWidth = lf_text_dimension(text).x;
        lf_set_ptr_x((lf_get_current_div().aabb.size.x - textWidth) / 2.0f);
        lf_text(text);
        lf_pop_font();
    }
    // Popup Buttons
    lf_next_line();
    {
        // Styling
        LfUIElementProps bprops = primary_button_style();
        lf_push_style_props(bprops);

        // Make the buttons stretch the entire div
        float halfDivWidth = lf_get_current_div().aabb.size.x / 2.0f - 
            bprops.padding * 2.0f - bprops.border_width * 2.0f  - (bprops.margin_left + bprops.margin_right);
        if(lf_button_fixed(this->aStr.c_str(), halfDivWidth, -1) == LF_CLICKED) {
            this->aCb();
        }
        if(lf_button_fixed(this->bStr.c_str(), halfDivWidth, -1) == LF_CLICKED) {
            this->bCb();
        }
        lf_pop_style_props();
    }
    lf_div_end();
    lf_pop_style_props();

}
