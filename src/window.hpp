#pragma once
#include "log.hpp"
#include "config.hpp"

#include <cstddef>
#include <functional>
#include <string>

#include <GLFW/glfw3.h>

class Window {
    public:
        Window() = default;
        Window(const std::string& title, uint32_t width, uint32_t height) 
            : _title(title), _width(width), _height(height) {
            
            _window = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
            if(!_window) {
                LOG_ERROR("Failed to create GLFW window '%s'.", title.c_str());
                return;
            }
            glfwMakeContextCurrent(_window);
            
            glfwSwapInterval(WIN_VSYNC);
        }

        ~Window() {
            glfwDestroyWindow(_window);
        }

        void swapBuffers() const {
            glfwSwapBuffers(_window);
        }

        bool shouldClose() const {
            return glfwWindowShouldClose(_window);
        }

        GLFWwindow* getRawWindow() const {
            return _window;
        }

        const std::string& getTitle() const {
            return _title;
        }

        uint32_t getWidth() const {
            return _width;
        }

        uint32_t getHeight() const {
            return _height;
        }

        void setTitle(const std::string& title) {
            glfwSetWindowTitle(_window, title.c_str());
            _title = title;
        }
        
        void setWidth(uint32_t width) {
            _width = width;
        }

        void setHeight(uint32_t height) {
            _height = height;
        }

        bool isFocused() const {
          return focused;
        }

        void setFocused(bool focused) {
          this->focused = focused;
        } 

    private:
        GLFWwindow* _window;

        std::string _title = "";
        uint32_t _width = 0, _height = 0;
        bool focused;
};
