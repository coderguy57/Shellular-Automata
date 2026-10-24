#include <catch2/catch_test_macros.hpp>

#include "controllers/gui_control.hpp"
#include "controllers/window_control.hpp"
#include "data.hpp"
#include "engines/engine.hpp"
#include "imgui.h"
#include "simulation_setup.hpp"
#include "viewer.hpp"
#include "window.hpp"

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

bool InitOpenGLForTesting(GLFWwindow** window, std::string& error);

namespace {
class WindowControlTestContext {
public:
    WindowControlTestContext() : previous_(ImGui::GetCurrentContext()) {
        context_ = ImGui::CreateContext();
        ImGui::SetCurrentContext(context_);
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(640, 480);
        io.DeltaTime = 1.0f / 60.0f;
        io.KeyRepeatDelay = 0.1f;
        io.KeyRepeatRate = 0.05f;
        io.Fonts->Build();
    }

    ~WindowControlTestContext() {
        ImGui::DestroyContext(context_);
        ImGui::SetCurrentContext(previous_);
    }

    bool draw_frame(WindowControl& control, int key, bool down,
                    bool text_input = false, bool captured = false) {
        auto& io = ImGui::GetIO();
        io.KeysDown[key] = down;
        ImGui::NewFrame();
        // NewFrame computes these flags itself; inject the state seen by the controller.
        io.WantCaptureKeyboard = text_input || captured;
        io.WantTextInput = text_input;
        const bool repeating = down && io.KeysDownDuration[key] > io.KeyRepeatDelay &&
                               ImGui::IsKeyPressed(key, true);
        control.draw();
        ImGui::EndFrame();
        return repeating;
    }

private:
    ImGuiContext* previous_;
    ImGuiContext* context_;
};

enum class Phase { ready, updated, stepped };
enum class InputScript { running, pause_step_resume };

struct LoopResults {
    uint64_t updates = 0;
    uint64_t steps = 0;
    uint64_t posts = 0;
    std::vector<uint64_t> steps_at_draw;
    std::vector<std::chrono::steady_clock::time_point> presentations;
    size_t max_pending_steps = 0;
    size_t pending_steps_at_exit = 0;
    int max_refresh_rate = 0;
    Phase phase = Phase::ready;
    bool ordered = true;
    bool timed_out = false;
    bool shaders_linked = false;
    bool engine_destroyed_with_context = false;
    bool controller_destroyed_with_context = false;
    GLenum gl_error = GL_NO_ERROR;
};

class GpuLoopLog {
public:
    GpuLoopLog(LoopResults& results, GLuint view_program)
        : results_(results), view_program_(view_program), use_(glUseProgram),
          fence_(glFenceSync), delete_(glDeleteSync) {
        active_ = this;
        glUseProgram = use;
        glFenceSync = fence;
        glDeleteSync = delete_sync;
    }

    ~GpuLoopLog() {
        results_.pending_steps_at_exit = pending_.size();
        glUseProgram = use_;
        glFenceSync = fence_;
        glDeleteSync = delete_;
        active_ = nullptr;
    }

private:
    static void GLAPIENTRY use(GLuint program) {
        if (program == active_->view_program_)
            active_->results_.presentations.push_back(std::chrono::steady_clock::now());
        active_->use_(program);
    }

    static GLsync GLAPIENTRY fence(GLenum condition, GLbitfield flags) {
        auto sync = active_->fence_(condition, flags);
        if (sync) {
            active_->pending_.push_back(sync);
            active_->results_.max_pending_steps = std::max(
                active_->results_.max_pending_steps, active_->pending_.size());
        }
        return sync;
    }

    static void GLAPIENTRY delete_sync(GLsync sync) {
        auto& pending = active_->pending_;
        pending.erase(std::remove(pending.begin(), pending.end(), sync), pending.end());
        active_->delete_(sync);
    }

    static inline GpuLoopLog* active_ = nullptr;
    LoopResults& results_;
    GLuint view_program_;
    PFNGLUSEPROGRAMPROC use_;
    PFNGLFENCESYNCPROC fence_;
    PFNGLDELETESYNCPROC delete_;
    std::vector<GLsync> pending_;
};

void require_window_support() {
    GLFWwindow* probe = nullptr;
    std::string error;
    if (!InitOpenGLForTesting(&probe, error))
        SKIP("OpenGL unavailable: " << error);
    // Window owns GLFW initialization and teardown; leave no probe context alive.
    glfwDestroyWindow(probe);
    glfwTerminate();
}

class CountingEngine final : public Engine {
public:
    CountingEngine(LoopResults& results, GLFWwindow* window)
        : results_(results), window_(window), started_(std::chrono::steady_clock::now()) {}

    ~CountingEngine() override {
        results_.engine_destroyed_with_context = glfwGetCurrentContext() == window_;
    }

    void step(Data&) override {
        results_.ordered &= results_.phase == Phase::updated;
        results_.phase = Phase::stepped;
        ++results_.steps;
        // Keep a broken presentation deadline from spinning indefinitely.
        if (std::chrono::steady_clock::now() - started_ > std::chrono::seconds(10)) {
            results_.timed_out = true;
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
    }

private:
    LoopResults& results_;
    GLFWwindow* window_;
    std::chrono::steady_clock::time_point started_;
};

class CountingControl final : public GuiControl {
public:
    CountingControl(LoopResults& results, GLFWwindow* window, InputScript script)
        : results_(results), window_(window), script_(script) {}

    ~CountingControl() override {
        results_.controller_destroyed_with_context = glfwGetCurrentContext() == window_;
    }

    void update(Data&) override {
        results_.ordered &= results_.phase == Phase::ready;
        results_.phase = Phase::updated;
        ++results_.updates;
    }

    void post_process(Data&) override {
        results_.ordered &= results_.phase == Phase::stepped;
        results_.phase = Phase::ready;
        ++results_.posts;
    }

    void draw() override {
        results_.ordered &= results_.phase == Phase::ready;
        results_.steps_at_draw.push_back(results_.steps);
        const auto frame = results_.steps_at_draw.size();
        const auto error = glGetError();
        if (error != GL_NO_ERROR)
            results_.gl_error = error;

        if (script_ == InputScript::pause_step_resume) {
            // WindowControl draws before this controller, so these reach it next frame.
            auto& io = ImGui::GetIO();
            io.KeysDown[GLFW_KEY_SPACE] = frame == 2 || frame == 10;
            io.KeysDown[GLFW_KEY_0] = frame == 6;
        }

        const size_t last_frame = script_ == InputScript::running ? 8 : 13;
        if (frame == last_frame)
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
    }

private:
    LoopResults& results_;
    GLFWwindow* window_;
    InputScript script_;
};

LoopResults run_counting_setup(InputScript script) {
    require_window_support();
    LoopResults results;
    Window window(128, 128);
    auto* native = glfwGetCurrentContext();
    glfwHideWindow(native);
    ImGui::GetIO().IniFilename = nullptr;
    while (glGetError() != GL_NO_ERROR) {}
    int monitor_count = 0;
    auto** monitors = glfwGetMonitors(&monitor_count);
    for (int i = 0; i < monitor_count; ++i) {
        if (const auto* mode = glfwGetVideoMode(monitors[i]))
            results.max_refresh_rate = std::max(results.max_refresh_rate, mode->refreshRate);
    }

    Data data;
    auto& state = *data.add_element<TextureData>("state", 4, 4, 1);
    const std::array<float, 4> color{0.25f, 0.5f, 0.75f, 1.0f};
    for (auto* texture : {&state.get_texture(), &state.get_last_texture()})
        glClearTexImage(texture->gl_tex_num, 0, GL_RGBA, GL_FLOAT, color.data());
    auto viewer = std::make_unique<Viewer>(4, 4, 1, "basic.fs", "basic.fs", GL_RGBA32F, "state");
    results.shaders_linked = viewer->program->is_linked() && viewer->output_program->is_linked();
    const GLuint view_program = viewer->program->get_id();
    SimulationSetup setup(std::make_unique<CountingEngine>(results, native),
                          std::move(viewer), std::move(data));
    setup.add_controller_back(std::make_unique<CountingControl>(results, native, script));
    {
        GpuLoopLog log(results, view_program);
        window.run(std::move(setup));
    }
    return results;
}

void check_balanced_steps(const LoopResults& results) {
    CHECK(results.shaders_linked);
    CHECK_FALSE(results.timed_out);
    CHECK(results.ordered);
    CHECK(results.phase == Phase::ready);
    CHECK(results.updates == results.steps);
    CHECK(results.posts == results.steps);
    CHECK(results.engine_destroyed_with_context);
    CHECK(results.controller_destroyed_with_context);
    CHECK(results.gl_error == GL_NO_ERROR);
    CHECK(results.max_pending_steps > 0);
    CHECK(results.max_pending_steps <= 2);
    CHECK(results.pending_steps_at_exit == 0);
    CHECK(results.presentations.size() == results.steps_at_draw.size());
}
}

TEST_CASE("Window shortcuts ignore text editing input", "[window-loop][controllers]") {
    for (const int key : {' ', '9', '0'}) {
        for (const bool initially_paused : {false, true}) {
            CAPTURE(key, initially_paused);
            WindowControlTestContext context;
            std::atomic<bool> quit{false};
            std::atomic<bool> paused{initially_paused};
            std::string new_setup;
            WindowControl control(quit, paused, new_setup);
            Data data;

            bool saw_repeat = false;
            for (int frame = 0; frame < 48; ++frame) {
                saw_repeat |= context.draw_frame(control, key, true, true);
                CHECK(paused.load() == initially_paused);
                control.update(data);
                CHECK(paused.load() == initially_paused);
            }
            CHECK(saw_repeat);

            // Finishing text editing while still holding the key must not replay it.
            context.draw_frame(control, key, true);
            CHECK(paused.load() == initially_paused);
            context.draw_frame(control, key, false);
            paused = false;
            control.update(data);
            CHECK_FALSE(paused.load());
            control.update(data);
            CHECK_FALSE(paused.load());
            CHECK_FALSE(quit.load());
            CHECK(new_setup.empty());
        }
    }
}

TEST_CASE("Window pause and step allow keyboard capture without text editing", "[window-loop][controllers]") {
    for (const int key : {' ', '9', '0'}) {
        CAPTURE(key);
        WindowControlTestContext context;
        std::atomic<bool> quit{false};
        std::atomic<bool> paused{key == '0'};
        std::string new_setup;
        WindowControl control(quit, paused, new_setup);
        Data data;

        context.draw_frame(control, key, true, false, true);
        CHECK(paused.load() == (key != '0'));
        control.update(data);
        CHECK(paused.load());
        for (int frame = 0; frame < 48; ++frame) {
            context.draw_frame(control, key, true, false, true);
            CHECK(paused.load());
            control.update(data);
            CHECK(paused.load());
        }
        context.draw_frame(control, key, false, false, true);
        context.draw_frame(control, key, true, false, true);
        CHECK_FALSE(paused.load());
        control.update(data);
        CHECK(paused.load() == (key == '0'));
        CHECK_FALSE(quit.load());
        CHECK(new_setup.empty());
    }
}

TEST_CASE("Window pause shortcuts toggle once per press without keyboard repeat", "[window-loop][controllers]") {
    for (const int key : {' ', '9'}) {
        CAPTURE(key);
        WindowControlTestContext context;
        std::atomic<bool> quit{false};
        std::atomic<bool> paused{false};
        std::string new_setup;
        WindowControl control(quit, paused, new_setup);
        Data data;

        context.draw_frame(control, key, true);
        CHECK(paused.load());
        bool saw_repeat = false;
        for (int frame = 0; frame < 48; ++frame) {
            saw_repeat |= context.draw_frame(control, key, true);
            CHECK(paused.load());
            control.update(data);
            CHECK(paused.load());
        }
        CHECK(saw_repeat);

        context.draw_frame(control, key, false);
        CHECK(paused.load());
        context.draw_frame(control, key, true);
        CHECK_FALSE(paused.load());
        control.update(data);
        CHECK_FALSE(paused.load());
        CHECK_FALSE(quit.load());
        CHECK(new_setup.empty());
    }
}

TEST_CASE("Window single step is queued once per press without keyboard repeat", "[window-loop][controllers]") {
    WindowControlTestContext context;
    std::atomic<bool> quit{false};
    std::atomic<bool> paused{true};
    std::string new_setup;
    WindowControl control(quit, paused, new_setup);
    Data data;

    context.draw_frame(control, '0', true);
    CHECK_FALSE(paused.load());
    control.update(data);
    CHECK(paused.load());

    // The step is consumed, so another update must not pause a resumed simulation.
    paused = false;
    control.update(data);
    CHECK_FALSE(paused.load());
    paused = true;

    bool saw_repeat = false;
    for (int frame = 0; frame < 48; ++frame) {
        saw_repeat |= context.draw_frame(control, '0', true);
        CHECK(paused.load());
        control.update(data);
        CHECK(paused.load());
    }
    CHECK(saw_repeat);

    context.draw_frame(control, '0', false);
    CHECK(paused.load());
    context.draw_frame(control, '0', true);
    CHECK_FALSE(paused.load());
    control.update(data);
    CHECK(paused.load());
    paused = false;
    control.update(data);
    CHECK_FALSE(paused.load());
    CHECK_FALSE(quit.load());
    CHECK(new_setup.empty());
}

TEST_CASE("Window advances simulation independently of UI presentation", "[window-loop][opengl]") {
    const auto results = run_counting_setup(InputScript::running);
    check_balanced_steps(results);
    REQUIRE(results.steps_at_draw.size() == 8);
    CHECK(results.steps > results.steps_at_draw.size());

    bool multiple_steps_between_draws = false;
    for (size_t frame = 1; frame < results.steps_at_draw.size(); ++frame) {
        CHECK(results.steps_at_draw[frame] > results.steps_at_draw[frame - 1]);
        multiple_steps_between_draws |= results.steps_at_draw[frame] - results.steps_at_draw[frame - 1] > 1;
    }
    CHECK(multiple_steps_between_draws);
    if (results.max_refresh_rate > 0) {
        const double interval = 1.0 / results.max_refresh_rate;
        for (size_t frame = 1; frame < results.presentations.size(); ++frame) {
            const double elapsed = std::chrono::duration<double>(
                results.presentations[frame] - results.presentations[frame - 1]).count();
            CAPTURE(elapsed, interval);
            CHECK(elapsed >= interval - 0.0005);
        }
    }
}

TEST_CASE("Window pause and single step remain exact with independent presentation", "[window-loop][opengl]") {
    const auto results = run_counting_setup(InputScript::pause_step_resume);
    check_balanced_steps(results);
    REQUIRE(results.steps_at_draw.size() == 13);
    const auto paused = results.steps_at_draw[3];
    REQUIRE(paused > 0);
    CHECK(results.steps_at_draw[4] == paused);
    CHECK(results.steps_at_draw[5] == paused);

    CHECK(results.steps_at_draw[7] == paused + 1);
    CHECK(results.steps_at_draw[8] == paused + 1);
    CHECK(results.steps_at_draw[9] == paused + 1);
    CHECK(results.steps_at_draw[11] > paused + 1);
    CHECK(results.steps_at_draw[12] > results.steps_at_draw[11]);
}
