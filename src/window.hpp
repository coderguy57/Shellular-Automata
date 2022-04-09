#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <vector>

class Engine;
class GuiControl;
class Window
{
public:
    Window(int width, int heigth);
    ~Window() {};
    void run(Engine* engine, std::vector<GuiControl*> guis);
private:
    GLFWwindow *_window;
};