#include <catch2/catch_test_macros.hpp>

#include "controllers/shader_control.hpp"
#include "data.hpp"
#include "engines/compute_shader_engine.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <GLFW/glfw3.h>

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

bool InitOpenGLForTesting(GLFWwindow** window, std::string& error);

namespace {
class ShaderTestGpu {
public:
    ShaderTestGpu() {
        std::string error;
        if (!InitOpenGLForTesting(&window_, error)) {
            SKIP("OpenGL unavailable: " << error);
        }
    }
    ~ShaderTestGpu() {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
private:
    GLFWwindow* window_ = nullptr;
};

class ShaderTestUi {
public:
    ShaderTestUi() {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(800, 600);
        io.DeltaTime = 1.0f / 60.0f;
        io.Fonts->Build();
    }
    ~ShaderTestUi() { ImGui::DestroyContext(); }

    void frame(ShaderControl& control) {
        ImGui::NewFrame();
        control.draw();
        ImGui::EndFrame();
    }
    void click(ShaderControl& control, ImVec2 position) {
        auto& io = ImGui::GetIO();
        io.MousePos = position;
        frame(control);
        io.MouseDown[0] = true;
        frame(control);
        io.MouseDown[0] = false;
        frame(control);
    }
    void open(ShaderControl& control) {
        frame(control);
        auto& bar = window("##MainMenuBar");
        click(control, ImVec2(bar.Pos.x + bar.DC.MenuBarOffset.x * 0.5f,
                              bar.MenuBarRect().GetCenter().y));
        frame(control);
        auto& menu = window("##Menu_00");
        click(control, ImVec2(menu.WorkRect.GetCenter().x,
                              menu.DC.CursorStartPos.y + ImGui::GetFontSize() * 0.5f));
        frame(control);
        REQUIRE_FALSE(window("Shader options").Hidden);
    }
    ImVec2 row(int index) {
        auto& w = window("Shader options");
        return ImVec2(w.DC.CursorStartPos.x + ImGui::GetStyle().FramePadding.x + 3.0f,
                      w.DC.CursorStartPos.y + index * stride() + ImGui::GetFrameHeight() * 0.5f);
    }
    ImGuiID hover(ShaderControl& control, int index) {
        ImGui::GetIO().MousePos = row(index);
        frame(control);
        return ImGui::GetCurrentContext()->HoveredId;
    }
    ImGuiID header(int index, const char* label) {
        return ImHashStr(label, 0, ImHashData(&index, sizeof(index), window("Shader options").ID));
    }
    ImGuiID header(const void* pointer, const char* label) {
        return ImHashStr(label, 0, ImHashData(&pointer, sizeof(pointer), window("Shader options").ID));
    }
    void require_rows(int count) {
        auto& w = window("Shader options");
        const float height = w.DC.CursorMaxPos.y - w.DC.CursorStartPos.y;
        const float expected = count ? count * stride() - ImGui::GetStyle().ItemSpacing.y : 0.0f;
        CHECK(std::abs(height - expected) < 0.1f);
    }
private:
    float stride() { return ImGui::GetFrameHeightWithSpacing(); }
    static ImGuiWindow& window(const char* name) {
        auto* result = ImGui::FindWindowByName(name);
        INFO(name);
        REQUIRE(result != nullptr);
        REQUIRE(result->Active);
        return *result;
    }
};

ComputeShaderStep* add_compute(ComputeShaderEngine& engine) {
    auto* step = engine.add_step<ComputeShaderStep>("fluidMNCA/mnca_delta.cs",
        std::vector<ComputeShaderInput>{}, std::vector<ComputeShaderInput>{});
    REQUIRE(step->program().is_linked());
    REQUIRE_FALSE(step->program().get_options().empty());
    return step;
}

GLSL::BoolOption& absolute_demand(ComputeProgram& program) {
    for (auto* option : program.get_options()) {
        if (option->label == "Use absolute demand") {
            REQUIRE(option->type == GLSL::IOption::Type::Bool);
            return *static_cast<GLSL::BoolOption*>(option);
        }
    }
    FAIL("Missing Use absolute demand option");
    throw std::logic_error("Missing option");
}
}

TEST_CASE("Explicit shader selection preserves mixed order and excludes unlisted programs",
          "[controllers][shader][opengl]") {
    ShaderTestGpu gpu;
    ShaderTestUi ui;
    Data data;
    auto* fragment = data.add_element<FragmentProgramData>("unlisted automatic label", "viewer.fs");
    auto* unlisted = data.add_element<FragmentProgramData>("unlisted fragment", "viewer.fs");
    REQUIRE(fragment->get_program().is_linked());
    REQUIRE(unlisted->get_program().is_linked());
    ComputeShaderEngine engine;
    auto* compute = add_compute(engine);
    add_compute(engine);
    FragmentProgram no_options("basic.vs", "basic.fs");
    ComputeProgram no_compute_options("init.cs");
    REQUIRE(no_options.is_linked());
    REQUIRE(no_compute_options.is_linked());
    REQUIRE(no_options.get_options().empty());
    REQUIRE(no_compute_options.get_options().empty());

    ShaderControl control(std::vector<ShaderControl::Entry>{
        {"Compute first", &compute->program()},
        {"Null fragment", static_cast<FragmentProgram*>(nullptr)},
        {"No fragment options", &no_options},
        {"Fragment second", &fragment->get_program()},
        {"Null compute", static_cast<ComputeProgram*>(nullptr)},
        {"No compute options", &no_compute_options},
    });
    control.post_process(data);
    ui.open(control);
    ui.require_rows(2);
    CHECK(ui.hover(control, 0) == ui.header(0, "Compute first"));
    CHECK(ui.hover(control, 1) == ui.header(3, "Fragment second"));
}

TEST_CASE("Explicit empty shader selection does not fall back to automatic discovery",
          "[controllers][shader][opengl]") {
    ShaderTestGpu gpu;
    ShaderTestUi ui;
    Data data;
    auto* fragment = data.add_element<FragmentProgramData>("Fragment", "viewer.fs");
    REQUIRE(fragment->get_program().is_linked());
    auto* empty_fragment = data.add_element<FragmentProgramData>("No options", "basic.fs");
    REQUIRE(empty_fragment->get_program().is_linked());
    REQUIRE(empty_fragment->get_program().get_options().empty());
    ComputeShaderEngine engine;
    auto* compute = add_compute(engine);

    SECTION("Explicit empty") {
        ShaderControl control(std::vector<ShaderControl::Entry>{});
        control.post_process(data);
        ui.open(control);
        ui.require_rows(0);
        CHECK(ui.hover(control, 0) == 0);
    }
    SECTION("Empty engine discovers fragments") {
        ComputeShaderEngine empty_engine;
        ShaderControl control(empty_engine);
        control.post_process(data);
        ui.open(control);
        ui.require_rows(1);
        CHECK(ui.hover(control, 0) == ui.header(fragment, "Fragment"));
    }
    SECTION("Engine constructor discovers fragments and compute steps") {
        ShaderControl control(engine);
        control.post_process(data);
        ui.open(control);
        ui.require_rows(2);
        CHECK(ui.hover(control, 0) == ui.header(fragment, "Fragment"));
        CHECK(ui.hover(control, 1) == ui.header(compute, "0: fluidMNCA/mnca_delta.cs"));
    }
}

TEST_CASE("Duplicate shader labels have independent headers and compute option values",
          "[controllers][shader][opengl]") {
    ShaderTestGpu gpu;
    ShaderTestUi ui;
    Data data;
    ComputeShaderEngine engine;
    auto* first = add_compute(engine);
    auto* second = add_compute(engine);
    auto& first_option = absolute_demand(first->program());
    auto& second_option = absolute_demand(second->program());
    REQUIRE(first_option.value);
    REQUIRE(second_option.value);
    first_option.changed = false;
    second_option.changed = false;
    ShaderControl control(std::vector<ShaderControl::Entry>{
        {"Demand", &first->program()}, {"Demand", &second->program()},
    });
    control.post_process(data);
    ui.open(control);
    ui.require_rows(2);
    const auto first_id = ui.header(0, "Demand");
    const auto second_id = ui.header(1, "Demand");
    REQUIRE(first_id != second_id);
    CHECK(ui.hover(control, 0) == first_id);
    CHECK(ui.hover(control, 1) == second_id);

    ui.click(control, ui.row(0));
    ui.require_rows(4); // First header, its two options, then the closed second header.
    CHECK(ui.hover(control, 3) == second_id);
    ui.click(control, ui.row(2));
    CHECK_FALSE(first_option.value);
    CHECK(first_option.changed);
    CHECK(second_option.value);
    CHECK_FALSE(second_option.changed);

    ui.click(control, ui.row(0));
    ui.require_rows(2);
    ui.click(control, ui.row(1));
    ui.require_rows(4);
    CHECK(ui.hover(control, 0) == first_id);
    ui.click(control, ui.row(3));
    CHECK_FALSE(second_option.value);
    CHECK(second_option.changed);
    CHECK_FALSE(first_option.value);
}
