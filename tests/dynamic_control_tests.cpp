#include <catch2/catch_test_macros.hpp>

#include "controllers/dynamic_control.hpp"
#include "data.hpp"
#include "imgui.h"
#include "imgui_internal.h"

#include <functional>
#include <string>

namespace {
class DynamicControlTestContext {
public:
    DynamicControlTestContext() {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(640, 480);
        io.DeltaTime = 1.0f / 60.0f;
        io.Fonts->Build();
    }

    ~DynamicControlTestContext() {
        ImGui::DestroyContext();
    }

    void draw_frame(GuiControl& control, const std::function<void()>& draw_extra = {}) {
        ImGui::NewFrame();
        if (draw_extra) {
            draw_extra();
        }
        control.draw();
        ImGui::EndFrame();
    }

    void click(GuiControl& control, ImVec2 position, const std::function<void()>& draw_extra = {}) {
        auto& io = ImGui::GetIO();
        io.MousePos = position;
        draw_frame(control, draw_extra);
        io.MouseDown[0] = true;
        draw_frame(control, draw_extra);
        io.MouseDown[0] = false;
        draw_frame(control, draw_extra);
    }

    void open_menu(GuiControl& control) {
        draw_frame(control);
        auto& bar = window("##MainMenuBar");
        click(control, ImVec2(bar.Pos.x + bar.DC.MenuBarOffset.x * 0.5f,
                              bar.MenuBarRect().GetCenter().y));
        draw_frame(control);
        REQUIRE_FALSE(window("##Menu_00").Hidden);
    }

    void open_controls(GuiControl& control) {
        open_menu(control);
        auto& menu = window("##Menu_00");
        click(control, ImVec2(menu.WorkRect.GetCenter().x,
                              menu.DC.CursorPosPrevLine.y + ImGui::GetFontSize() * 0.5f));
        draw_frame(control);
        REQUIRE_FALSE(window("Simulation controls").Hidden);
    }

    ImVec2 menu_item(int row) {
        auto& menu = window("##Menu_00");
        const float height = ImGui::GetFontSize();
        return ImVec2(menu.WorkRect.GetCenter().x,
                      menu.DC.CursorStartPos.y + row * (height + ImGui::GetStyle().ItemSpacing.y) + height * 0.5f);
    }

    ImVec2 button(int row) {
        auto& controls = window("Simulation controls");
        const float height = ImGui::GetFrameHeight();
        return ImVec2(controls.DC.CursorStartPos.x + ImGui::GetStyle().FramePadding.x + 1.0f,
                      controls.DC.CursorStartPos.y + row * (height + ImGui::GetStyle().ItemSpacing.y) + height * 0.5f);
    }

private:
    static ImGuiWindow& window(const char* name) {
        auto* result = ImGui::FindWindowByName(name);
        INFO(name);
        REQUIRE(result != nullptr);
        REQUIRE(result->Active);
        return *result;
    }
};
}

TEST_CASE("Dynamic control menu actions are deferred and coalesced", "[controllers][dynamic]") {
    DynamicControlTestContext context;
    Data data;
    int clears = 0;
    int button_only = 0;
    int hidden = 0;
    Data* received_data = nullptr;
    DynamicControl control("Simulation", {
        {"Clear", [&](Data& received) {
            clears++;
            received_data = &received;
        }},
        {"Hidden reset", [&](Data&) { hidden++; }, 'V', false},
        {"Button only", [&](Data&) { button_only++; }},
        {"Disabled", {}},
    });
    GuiControl& gui = control;

    context.open_menu(gui);
    context.click(gui, context.menu_item(2));
    CHECK_FALSE(ImGui::GetCurrentContext()->OpenPopupStack.empty());
    gui.update(data);
    CHECK(clears == 0);
    CHECK(button_only == 0);

    context.click(gui, context.menu_item(0));
    context.open_menu(gui);
    context.click(gui, context.menu_item(0));
    gui.post_process(data);
    CHECK(clears == 0);
    CHECK(received_data == nullptr);
    gui.update(data);
    CHECK(clears == 1);
    CHECK(received_data == &data);
    gui.update(data);
    CHECK(clears == 1);

    context.open_menu(gui);
    context.click(gui, context.menu_item(1));
    CHECK(button_only == 0);
    gui.update(data);
    CHECK(button_only == 1);
    CHECK(hidden == 0);

    context.open_menu(gui);
    context.click(gui, context.menu_item(0));
    gui.update(data);
    CHECK(clears == 2);
}

TEST_CASE("Dynamic control buttons support actions without shortcuts", "[controllers][dynamic]") {
    DynamicControlTestContext context;
    Data data;
    int clears = 0;
    int button_only = 0;
    int hidden = 0;
    DynamicControl control("Simulation", {
        {"Clear", [&](Data&) { clears++; }, 'P'},
        {"Hidden reset", [&](Data&) { hidden++; }, 'V', false},
        {"Button only", [&](Data&) { button_only++; }},
        {"Disabled", {}},
    });
    GuiControl& gui = control;

    context.open_controls(gui);
    context.click(gui, context.button(2));
    gui.update(data);
    CHECK(clears == 0);
    CHECK(button_only == 0);

    context.click(gui, context.button(1));
    CHECK(button_only == 0);
    gui.post_process(data);
    CHECK(button_only == 0);
    gui.update(data);
    CHECK(button_only == 1);
    gui.update(data);
    CHECK(button_only == 1);

    context.click(gui, context.button(0));
    // The button and shortcut queue the same action before the next update.
    ImGui::GetIO().KeysDown['P'] = true;
    context.draw_frame(gui);
    CHECK_FALSE(ImGui::GetIO().WantCaptureKeyboard);
    CHECK(clears == 0);
    gui.update(data);
    CHECK(clears == 1);
    CHECK(hidden == 0);
}

TEST_CASE("Dynamic control shortcuts fire once per press including hidden actions", "[controllers][dynamic]") {
    DynamicControlTestContext context;
    Data data;
    int clears = 0;
    int resets = 0;
    int button_only = 0;
    DynamicControl control("Simulation", {
        {"Clear", [&](Data&) { clears++; }, 'P'},
        {"Hidden reset", [&](Data&) { resets++; }, 'V', false},
        {"Button only", [&](Data&) { button_only++; }},
    });
    GuiControl& gui = control;
    auto& io = ImGui::GetIO();

    io.KeysDown['P'] = true;
    io.KeysDown['B'] = true;
    context.draw_frame(gui);
    CHECK(clears == 0);
    gui.update(data);
    CHECK(clears == 1);
    for (int frame = 0; frame < 40; frame++) {
        context.draw_frame(gui);
        gui.update(data);
    }
    CHECK(clears == 1);

    io.KeysDown['P'] = false;
    context.draw_frame(gui);
    io.KeysDown['P'] = true;
    io.KeysDown['V'] = true;
    context.draw_frame(gui);
    CHECK(clears == 1);
    CHECK(resets == 0);
    gui.update(data);
    CHECK(clears == 2);
    CHECK(resets == 1);
    CHECK(button_only == 0);

    for (int frame = 0; frame < 40; frame++) {
        context.draw_frame(gui);
        gui.update(data);
    }
    CHECK(clears == 2);
    CHECK(resets == 1);
}

TEST_CASE("Hidden-only dynamic controls expose shortcuts without a menu", "[controllers][dynamic]") {
    DynamicControlTestContext context;
    Data data;
    int calls = 0;
    DynamicControl control("Simulation", {
        {"Hidden reset", [&](Data&) { calls++; }, 'V', false},
    });
    GuiControl& gui = control;

    ImGui::GetIO().KeysDown['V'] = true;
    context.draw_frame(gui);
    CHECK(ImGui::FindWindowByName("##MainMenuBar") == nullptr);
    CHECK(ImGui::FindWindowByName("Simulation controls") == nullptr);
    CHECK(calls == 0);
    gui.update(data);
    CHECK(calls == 1);
}

TEST_CASE("Dynamic control shortcuts work while an idle ImGui window has keyboard focus", "[controllers][dynamic]") {
    DynamicControlTestContext context;
    Data data;
    int calls = 0;
    DynamicControl control("Simulation", {
        {"Clear", [&](Data&) { calls++; }, 'P'},
    });
    GuiControl& gui = control;
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.KeyMap[ImGuiKey_Space] = ' ';
    const auto draw_idle = [&] {
        ImGui::SetNextWindowPos(ImVec2(100, 100));
        ImGui::SetNextWindowSize(ImVec2(300, 100));
        ImGui::Begin("Focused idle window");
        ImGui::SetWindowFocus();
        ImGui::Button("Idle button");
        ImGui::End();
    };
    for (int i = 0; i < 4; ++i)
        context.draw_frame(gui, draw_idle);
    REQUIRE(io.WantCaptureKeyboard);
    REQUIRE_FALSE(io.WantTextInput);
    REQUIRE_FALSE(ImGui::IsAnyItemActive());
    REQUIRE(ImGui::GetCurrentContext()->NavWindow == ImGui::FindWindowByName("Focused idle window"));

    io.KeysDown['P'] = true;
    context.draw_frame(gui, draw_idle);
    REQUIRE(io.WantCaptureKeyboard);
    CHECK(calls == 0);
    gui.update(data);
    CHECK(calls == 1);
    for (int i = 0; i < 40; ++i) {
        context.draw_frame(gui, draw_idle);
        gui.update(data);
    }
    CHECK(calls == 1);
    io.KeysDown['P'] = false;
    context.draw_frame(gui, draw_idle);
    io.KeysDown['P'] = true;
    context.draw_frame(gui, draw_idle);
    gui.update(data);
    CHECK(calls == 2);
}

TEST_CASE("Dynamic control shortcuts do not interfere with active sliders", "[controllers][dynamic]") {
    DynamicControlTestContext context;
    Data data;
    int calls = 0;
    DynamicControl control("Simulation", {
        {"Clear", [&](Data&) { calls++; }, 'P'},
    });
    GuiControl& gui = control;
    auto& io = ImGui::GetIO();
    float value = 0.5f;
    ImVec2 slider_position;
    const auto draw_slider = [&] {
        ImGui::SetNextWindowPos(ImVec2(100, 100));
        ImGui::SetNextWindowSize(ImVec2(300, 100));
        ImGui::Begin("Slider interaction");
        ImGui::SliderFloat("Value", &value, 0.0f, 1.0f);
        const auto min = ImGui::GetItemRectMin();
        const auto max = ImGui::GetItemRectMax();
        slider_position = ImVec2(min.x + 30.0f, (min.y + max.y) * 0.5f);
        ImGui::End();
    };
    context.draw_frame(gui, draw_slider);
    context.draw_frame(gui, draw_slider);
    io.MousePos = slider_position;
    context.draw_frame(gui, draw_slider);
    io.MouseDown[0] = true;
    context.draw_frame(gui, draw_slider);
    REQUIRE(ImGui::IsAnyItemActive());
    REQUIRE_FALSE(io.WantTextInput);

    io.KeysDown['P'] = true;
    context.draw_frame(gui, draw_slider);
    REQUIRE(ImGui::IsAnyItemActive());
    gui.update(data);
    CHECK(calls == 0);
    io.MouseDown[0] = false;
    context.draw_frame(gui, draw_slider);
    REQUIRE_FALSE(ImGui::IsAnyItemActive());
    gui.update(data);
    CHECK(calls == 0);
    io.KeysDown['P'] = false;
    context.draw_frame(gui, draw_slider);
    io.KeysDown['P'] = true;
    context.draw_frame(gui, draw_slider);
    gui.update(data);
    CHECK(calls == 1);
}

TEST_CASE("Dynamic control shortcuts are blocked while a popup is open", "[controllers][dynamic]") {
    DynamicControlTestContext context;
    Data data;
    int calls = 0;
    DynamicControl control("Simulation", {
        {"Clear", [&](Data&) { calls++; }, 'P'},
    });
    GuiControl& gui = control;
    auto& io = ImGui::GetIO();
    bool open = false;
    bool close = false;
    const auto draw_popup = [&] {
        ImGui::Begin("Popup host");
        if (open) {
            ImGui::OpenPopup("Pending action");
            open = false;
        }
        if (ImGui::BeginPopup("Pending action")) {
            ImGui::TextUnformatted("Pending action");
            if (close)
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::End();
    };
    context.draw_frame(gui, draw_popup);
    context.draw_frame(gui, draw_popup);
    open = true;
    context.draw_frame(gui, draw_popup);
    context.draw_frame(gui, draw_popup);
    REQUIRE(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
    REQUIRE_FALSE(ImGui::IsAnyItemActive());
    REQUIRE_FALSE(io.WantTextInput);
    io.KeysDown['P'] = true;
    context.draw_frame(gui, draw_popup);
    gui.update(data);
    CHECK(calls == 0);

    close = true;
    context.draw_frame(gui, draw_popup);
    REQUIRE_FALSE(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
    gui.update(data);
    CHECK(calls == 0);
    io.KeysDown['P'] = false;
    context.draw_frame(gui, draw_popup);
    io.KeysDown['P'] = true;
    context.draw_frame(gui, draw_popup);
    gui.update(data);
    CHECK(calls == 1);
}

TEST_CASE("Dynamic control shortcuts respect active text input", "[controllers][dynamic]") {
    DynamicControlTestContext context;
    Data data;
    int calls = 0;
    DynamicControl control("Simulation", {
        {"Clear", [&](Data&) { calls++; }, 'P'},
    });
    GuiControl& gui = control;
    auto& io = ImGui::GetIO();
    char text[32] = {};
    ImVec2 input_position;
    auto draw_input = [&] {
        ImGui::SetNextWindowPos(ImVec2(100, 100));
        ImGui::SetNextWindowSize(ImVec2(300, 100));
        ImGui::Begin("Text input");
        ImGui::InputText("Name", text, sizeof(text));
        const auto min = ImGui::GetItemRectMin();
        const auto max = ImGui::GetItemRectMax();
        input_position = ImVec2(min.x + 10.0f, (min.y + max.y) * 0.5f);
        ImGui::End();
    };

    context.draw_frame(gui, draw_input);
    context.draw_frame(gui, draw_input);
    context.click(gui, input_position, draw_input);
    REQUIRE(io.WantCaptureKeyboard);
    io.KeysDown['P'] = true;
    io.AddInputCharacter('p');
    context.draw_frame(gui, draw_input);
    CHECK(io.WantCaptureKeyboard);
    CHECK(std::string(text) == "p");
    gui.update(data);
    CHECK(calls == 0);

    context.click(gui, ImVec2(600, 400), draw_input);
    context.draw_frame(gui, draw_input);
    REQUIRE_FALSE(io.WantCaptureKeyboard);
    gui.update(data);
    CHECK(calls == 0);

    io.KeysDown['P'] = false;
    context.draw_frame(gui, draw_input);
    io.KeysDown['P'] = true;
    context.draw_frame(gui, draw_input);
    CHECK(calls == 0);
    gui.update(data);
    CHECK(calls == 1);
}
