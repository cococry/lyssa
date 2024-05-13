#include "popups.hpp"
#include "config.hpp"
#include "global.hpp"

#include <filesystem>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "playlists.hpp"
#include "soundTagParser.hpp"

#include <cstring>
#include <fstream>
#include <algorithm>
#include <iostream>

void Popup::update() {
  if(lf_key_went_down(GLFW_KEY_ESCAPE)) {
    this->shouldRender = false;
    lf_div_ungrab();
  }
}
void EditPlaylistPopup::render() {
  static char nameBuf[INPUT_BUFFER_SIZE] = {0};
  static char descBuf[INPUT_BUFFER_SIZE] = {0};
  static bool initBufs = false;

  Playlist& currentPlaylist = state.playlists[state.currentPlaylist];

  if(!initBufs) {
    memcpy(nameBuf, currentPlaylist.name.c_str(), INPUT_BUFFER_SIZE);
    memcpy(descBuf, currentPlaylist.desc.c_str(), INPUT_BUFFER_SIZE);
    initBufs = true;
  }

  // Beginning a new div
  const vec2s popupSize = (vec2s){500.0f, 500.0f};
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
      std::vector<std::string> playlistFilepaths =  PlaylistMetadata::getFilepaths(std::filesystem::directory_entry(currentPlaylist.path));

      Playlist::rename(std::string(nameBuf), state.currentPlaylist);
      Playlist::changeDesc(std::string(descBuf), state.currentPlaylist);

      bool playlistEmpty = currentPlaylist.musicFiles.empty();
      if(playlistEmpty) {
        for(auto& filepath : playlistFilepaths) {
          currentPlaylist.musicFiles.emplace_back((SoundFile){.path = filepath});
        }
      }
      Playlist::save(state.currentPlaylist);
      if(playlistEmpty) {
        currentPlaylist.musicFiles.clear();
        currentPlaylist.musicFiles.shrink_to_fit();
      }
      this->shouldRender = false;

      memset(nameBuf, 0, INPUT_BUFFER_SIZE);
      memset(descBuf, 0, INPUT_BUFFER_SIZE);
      initBufs = false;
      lf_div_ungrab();
    }
  }

  lf_div_end();

}

void PlaylistFileDialoguePopup::render() {
  if(!this->shouldRender) return;
  const vec2s popupSize =(vec2s){300, 230};
  this->pos.x = MIN(this->pos.x, state.win->getWidth() - popupSize.x - 15.0f);
  this->pos.y = MIN(this->pos.y, state.win->getHeight() - popupSize.y - 15.0f);
  static bool onPlaylistAddTab = false;

  LfUIElementProps props = lf_get_theme().div_props;
  props.color = lf_color_brightness(GRAY, 0.5);
  props.corner_radius = 5;
  lf_push_style_props(props);

  static float div_scroll = 0.0f;
  static float div_scroll_vel = 0.0f;
  lf_div_begin_ex(this->pos, popupSize, true, &div_scroll, &div_scroll_vel);
  if(!lf_div_grabbed()) {
    lf_div_grab(lf_get_current_div());
  }
  lf_pop_style_props();


  if(!onPlaylistAddTab) {
    const uint32_t options_count = 5;
    static const char* options[options_count];
    options[0] = "Add to playlist...";
    options[1] = "Remove";
    options[2] = Playlist::metadataContainsFile(this->path.string(), 0) ? "Remove from favourites" : "Add to favourites";
    if(state.currentTab == GuiTab::Dashboard && state.dashboardTab == DashboardTab::Favourites) {
      options[2] = "";
    }
    options[3] = "Open URL...";
    options[4] = state.currentPlaylist != 0 ? "Set as thumbnail" : "";

    static uint32_t optionIcons[options_count];
    optionIcons[0] = state.icons["add_symbol"].id;
    optionIcons[1] = state.icons["delete"].id;
    optionIcons[2] = state.icons["favourite"].id;
    optionIcons[3] = state.icons["more"].id;
    optionIcons[4] = state.icons["thumbnail"].id;

    int32_t clickedIndex = -1;
    for(uint32_t i = 0; i < options_count; i++) {
      if(strlen(options[i]) == 0) continue;
      uint32_t texWidth = (i == 4) ? 22 : 20;
      lf_image((LfTexture){.id = optionIcons[i], .width = texWidth, .height = 20});
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
          onPlaylistAddTab = true;
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
          lf_div_ungrab();
          state.infoCards.addCard("Removed from playlist.");
          break;
        }
      case 2: /* Add to favourites */
        {
          if(Playlist::metadataContainsFile(this->path.string(), 0)) {
            Playlist& favourites = state.playlists[0];
            if(!favourites.loaded) {
              std::vector<std::string> paths = PlaylistMetadata::getFilepaths(
                  std::filesystem::directory_entry(state.playlists[0].path));
              for(auto& path : paths) {
                favourites.musicFiles.push_back((SoundFile){.path = path});
              }
              Playlist::removeFile(this->path.string(), 0);
              favourites.musicFiles.clear();
            } else {
              Playlist::removeFile(this->path.string(), 0);
            }
            this->shouldRender = false;
            lf_div_ungrab();
            state.infoCards.addCard("Removed from favourites.");
          } else {
            Playlist& favourites = state.playlists[0]; // 0th playlist is favourites
            if(favourites.loaded) {
              Playlist::addFile(this->path, 0);
              favourites.loaded = false;
            } else {
              std::ofstream metadata(favourites.path.string() + "/.metadata", std::ios::app);
              metadata.seekp(0, std::ios::end);

              metadata << "\"" << this->path.string() << "\" ";
              metadata.close();
            }
            this->shouldRender = false;
            lf_div_ungrab();
            state.infoCards.addCard("Added to favourites.");
          }
          break;
        }
      case 3: /* Open URL */
        {
          std::string url = SoundTagParser::getSoundComment(this->path.string());
          if(url != "") {
            std::string cmd = "xdg-open " + url + "& ";
            system(cmd.c_str());
          }
          this->shouldRender = false;
          lf_div_ungrab();
          state.infoCards.addCard("Opening URL...");
          break;
        }
      case 4: /* Set thumbnail */
        {
          Playlist& playlist = state.playlists[state.currentPlaylist];
          TextureData fullscaleThumb = SoundTagParser::getSoundThubmnailData(this->path.string(), (vec2s){-1, -1});

          if (!stbi_write_jpg(std::string(playlist.path.string() + "/thumbnail.jpg.jpg").c_str(), fullscaleThumb.width, fullscaleThumb.height, 
                fullscaleThumb.channels, fullscaleThumb.data, 100)) {
            LOG_ERROR("Failed to write thumbnail file.");
            break;
          }
          lf_create_texture_from_image_data(LF_TEX_FILTER_LINEAR, &playlist.thumbnail.id, 
              fullscaleThumb.width, fullscaleThumb.height, fullscaleThumb.channels, fullscaleThumb.data);

          playlist.thumbnail.width = fullscaleThumb.width;
          playlist.thumbnail.height = fullscaleThumb.height;

          playlist.thumbnailPath = std::filesystem::path(playlist.path.string() + "/thumbnail.jpg.jpg");
          Playlist::save(state.currentPlaylist);
          this->shouldRender = false;
          lf_div_ungrab();
          state.infoCards.addCard("Changed thumbnail of playlist.");
          break;
        }
      default:
        break;
    }
  } else {
    // Heading
    {
      {
        LfUIElementProps props = lf_get_theme().button_props;
        props.color = LF_NO_COLOR;
        props.padding = 0;
        props.border_width = 0;
        lf_push_style_props(props);
        if(lf_image_button(((LfTexture){.id = state.icons["back"].id, .width = BACK_BUTTON_WIDTH / 2, .height = BACK_BUTTON_HEIGHT / 2})) == LF_CLICKED) {
          onPlaylistAddTab = false;
        }
        lf_pop_style_props();
      }
      LfUIElementProps props = lf_get_theme().button_props;
      props.color = lf_color_brightness(GRAY, 0.5f);
      lf_push_style_props(props);
      lf_seperator();
      lf_pop_style_props();
    }
    // Playlists
    {
      uint32_t renderedCount = 0;
      for(uint32_t i = 0; i < state.playlists.size(); i++) {
        Playlist& playlist = state.playlists[i];
        if(playlist.path.filename() == "favourites") continue;
        if(i == state.currentPlaylist) continue;
        if( Playlist::metadataContainsFile(this->path, i)) continue;

        LfUIElementProps props = secondary_button_style();
        lf_push_style_props(props);
        if(lf_button_fixed(playlist.name.c_str(), 150, -1) == LF_CLICKED) {
          if(playlist.loaded) {
            Playlist::addFile(this->path, i);
          } else {
            std::ofstream metadata(playlist.path.string() + "/.metadata", std::ios::app);
            metadata.seekp(0, std::ios::end);

            metadata << "\"" << this->path.string() << "\" ";
            metadata.close();
          }
          playlist.loaded = false;
          this->shouldRender = false;
          onPlaylistAddTab = false;
          lf_set_current_div_scroll(0.0f);
          lf_div_ungrab();
          state.infoCards.addCard("Added to \"" + playlist.name + "\"");
        }
        lf_pop_style_props();

        lf_next_line();
        renderedCount++;
      }
      if(renderedCount == 0) {
        lf_text("No other playlists.");
      }
    }
    lf_next_line();
  }

  if(lf_get_current_div().id != lf_get_selected_div().id && lf_mouse_button_is_released(GLFW_MOUSE_BUTTON_LEFT)) {
    this->shouldRender = false;
    onPlaylistAddTab = false;
    div_scroll_vel = 0.0f;
    div_scroll = 0.0f;
    lf_div_ungrab();
  }
  lf_div_end();

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
