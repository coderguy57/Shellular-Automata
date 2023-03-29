#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <vector>
#include <vector>

class SimulationSetup;
class Window
{
public:
    Window(int width, int heigth);
    ~Window() {};
    void run(SimulationSetup simulation_setup);
private:
    GLFWwindow *_window;
};