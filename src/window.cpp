#include "window.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <stdio.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <atomic>
#include <optional>
#include <algorithm>
#include <array>

#include "simulation_setup.hpp"
#include "simulation_setup_loader.hpp"
#include "viewer.hpp"
#include "controllers/window_control.hpp"
#include "controllers/engine_control.hpp"
#include "controllers/viewer_control.hpp"
#include "controllers/gui_control.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace {
using Clock = std::chrono::steady_clock;

void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}
int monitor_refresh_rate(GLFWwindow* window)
{
    auto* monitor = glfwGetWindowMonitor(window);
    if (!monitor) {
        monitor = glfwGetPrimaryMonitor();
        int x = 0, y = 0, width = 0, height = 0;
        glfwGetWindowPos(window, &x, &y);
        glfwGetWindowSize(window, &width, &height);
        int count = 0;
        auto** monitors = glfwGetMonitors(&count);
        int largest_overlap = 0;
        for (int i = 0; i < count; ++i) {
            const auto* mode = glfwGetVideoMode(monitors[i]);
            if (!mode)
                continue;
            int monitor_x = 0, monitor_y = 0;
            glfwGetMonitorPos(monitors[i], &monitor_x, &monitor_y);
            const int overlap_width = std::max(0,
                std::min(x + width, monitor_x + mode->width) - std::max(x, monitor_x));
            const int overlap_height = std::max(0,
                std::min(y + height, monitor_y + mode->height) - std::max(y, monitor_y));
            const int overlap = overlap_width * overlap_height;
            if (overlap > largest_overlap) {
                largest_overlap = overlap;
                monitor = monitors[i];
            }
        }
    }
    const auto* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    // Default to 60 fps
    return mode ? mode->refreshRate : 60;
}
}

Window::Window(int width, int height)
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        throw "Failed to init glfw\n";

    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    // glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
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
    // Presentation is paced separately so buffer swaps do not stall simulation.
    glfwSwapInterval(0);

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

void Window::run(SimulationSetup initial_simulation)
{
    std::optional<SimulationSetup> simulation_storage{std::move(initial_simulation)};
    auto& simulation = *simulation_storage;
    unsigned int frames = 0;
    unsigned int steps = 0;

    std::atomic<bool> is_quit(false);
    std::atomic<bool> is_paused(false);
    std::string new_simulation_setup = "";

    simulation.add_controller_front(std::make_unique<ViewerControl>(simulation.viewer()));
    simulation.add_controller_front(std::make_unique<WindowControl>(is_quit, is_paused, new_simulation_setup));

    const int refresh_rate = monitor_refresh_rate(_window);
    const auto presentation_interval = std::chrono::ceil<Clock::duration>(std::chrono::duration<double>(1.0 / refresh_rate));

    auto rate_timer = Clock::now();
    auto next_presentation = rate_timer;
    // Keep at most two simulation steps queued on the GPU.
    std::array<GLsync, 2> pending_steps{};
    size_t next_step = 0;
    while (!glfwWindowShouldClose(_window) && !is_quit)
    {
        if (new_simulation_setup != "") {
            simulation = std::move(load_simulation_setup(new_simulation_setup));
            simulation.add_controller_front(std::make_unique<ViewerControl>(simulation.viewer()));
            simulation.add_controller_front(std::make_unique<WindowControl>(is_quit, is_paused, new_simulation_setup));
            new_simulation_setup = "";
        }

        const bool present = Clock::now() >= next_presentation;
        if (present) {
            glfwPollEvents();
            if (glfwWindowShouldClose(_window))
                break;
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
        }

        bool gpu_ready = true;
        auto& pending = pending_steps[next_step];
        if (!is_paused && pending) {
            const GLenum result = glClientWaitSync(pending, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000);
            gpu_ready = result != GL_TIMEOUT_EXPIRED;
            if (result == GL_WAIT_FAILED) {
                std::cerr << "Failed to wait for simulation GPU work\n";
                if (present)
                    ImGui::EndFrame();
                break;
            }
            if (gpu_ready) {
                glDeleteSync(pending);
                pending = nullptr;
            }
        }

        if (!is_paused && gpu_ready)
        {
            for (auto& gui : simulation.controllers())
            {
                gui->update(simulation.data());
            }
            simulation.engine().step(simulation.data());
            for (auto& gui : simulation.controllers())
            {
                gui->post_process(simulation.data());
            }
            pending = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            if (!pending)
                glFinish();
            next_step = (next_step + 1) % pending_steps.size();
            steps++;
        }

        if (present) {
            for (auto& gui : simulation.controllers())
                gui->draw();
            ImGui::Render();

            const auto presentation_started = Clock::now();
            int display_w, display_h;
            glfwGetFramebufferSize(_window, &display_w, &display_h);
            if (display_w > 0 && display_h > 0) {
                simulation.viewer().view(display_w, display_h, simulation.data());
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                glfwSwapBuffers(_window);
            }
            // Schedule from the actual presentation, never catch up with extra draws.
            next_presentation = presentation_started + presentation_interval;
            frames++;
        }

        const auto now = Clock::now();
        const auto elapsed = std::chrono::duration<double>(now - rate_timer).count();
        if (elapsed >= 1.0) {
            std::cout << "Viewer FPS: " << frames / elapsed
                      << ", simulation steps/s: " << steps / elapsed << std::endl;
            frames = 0;
            steps = 0;
            rate_timer = now;
        }
        if (is_paused && now < next_presentation) {
            glfwWaitEventsTimeout(std::chrono::duration<double>(next_presentation - now).count());
        }
    }

    for (auto pending : pending_steps) {
        if (pending)
            glDeleteSync(pending);
    }

    // Release simulation GL resources while the context is still current.
    simulation_storage.reset();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(_window);
    glfwTerminate();
}
