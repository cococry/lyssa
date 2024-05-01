#pragma once

extern "C" {
    #include <leif/leif.h>
}
#include <string>
#include <filesystem>
#include <functional>

enum class PopupType {
    EditPlaylistPopup = 0,
    PlaylistFileDialoguePopup,
    TwoChoicePopup,
    PopupCount
};

class Popup {
    public:
        virtual void render() = 0; 
        bool shouldRender;

        void update();
    private:
};

class EditPlaylistPopup : public Popup {
    public:
        virtual void render() override;
    private:
};

class PlaylistFileDialoguePopup : public Popup {
    public:
        PlaylistFileDialoguePopup(const std::filesystem::path& path, vec2s pos) 
            : path(path), pos(pos) {}
       virtual void render() override;

       std::filesystem::path path;
       vec2s pos;
    private:
};

class TwoChoicePopup : public Popup {
    public:
        TwoChoicePopup(uint32_t width, const std::string& title, const std::string& aStr, const std::string& bStr, 
                const std::function<void()>& aCb, const std::function<void()>& bCb) 
            : width(width), title(title), aStr(aStr), bStr(bStr), aCb(aCb), bCb(bCb) {}

        virtual void render() override;

        uint32_t width;
        std::string title;
        std::string aStr, bStr;
        std::function<void()> aCb;
        std::function<void()> bCb;
    private:
};

