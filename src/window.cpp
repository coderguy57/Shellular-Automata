#include "window.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <stdio.h>

#include <iostream>
#include <vector>

#include "engine.hpp"
#include "controllers/window_control.hpp"
#include "controllers/gui_control.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}
double clockToMilliseconds(clock_t ticks)
{
    return (ticks / (double)CLOCKS_PER_SEC) * 1000.0;
}

Window::Window(int width, int height)
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        throw "Failed to init glfw\n";

    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // 3.0+ only

    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    // Create window with graphics context
    _window = glfwCreateWindow(width, height, "Shellular Automata", NULL, NULL);
    if (_window == NULL)
        throw "Failed to create window\n";
    glfwMakeContextCurrent(_window);
    glfwSwapInterval(0); // Enable vsync

    if (glewInit() != GLEW_OK)
        throw "Failed to init glew\n";

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(_window, true);
    const char *glsl_version = "#version 460";
    ImGui_ImplOpenGL3_Init(glsl_version);

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(GLDebugMessageCallback, 0);
}

void Window::run(Engine *engine, std::vector<GuiControl *> guis)
{
    clock_t deltaTime = 0;
    unsigned int frames = 0;
    double frameRate = 30;
    double averageFrameTimeMilliseconds = 33.333;

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    WindowControl *window_controller = new WindowControl;
    guis.insert(guis.begin(), window_controller);

    // Main loop
    clock_t beginFrame = clock();
    clock_t endFrame = clock();
    RenderSurface surface{};
    FragmentProgram output_program{"basic.vs", "basic.fs"};
    while (!glfwWindowShouldClose(_window))
    {
        if (window_controller->is_quit())
        {
            break;
        }

        endFrame = clock();
        if (!window_controller->is_paused())
        {
            deltaTime += endFrame - beginFrame;
            frames++;
        }
        beginFrame = clock();

        if (clockToMilliseconds(deltaTime) > 1000.0)
        {                                                       // every second
            frameRate = (double)frames * 0.5 + frameRate * 0.5; // more stable
            frames = 0;
            deltaTime -= CLOCKS_PER_SEC;
            averageFrameTimeMilliseconds = 1000.0 / (frameRate == 0 ? 0.001 : frameRate);

            std::cout << "FrameTime was:" << frameRate << std::endl;
        }
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        engine->start();
        auto engine_shader = engine->program;
        if (!window_controller->is_paused())
        {
            for (auto gui : guis)
            {
                gui->update(engine_shader);
            }
            engine->step();
            engine->step();
            engine->step();
            engine->step();
            for (auto gui : guis)
            {
                gui->post_process(engine->current_texture());
            }
        }

        // ImGui::ShowDemoWindow();
        for (auto gui : guis)
        {
            gui->draw();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        output_program.use();
        output_program.set_texture(0, "tex", engine->current_texture());
        surface.draw();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(_window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(_window);
    glfwTerminate();
}
