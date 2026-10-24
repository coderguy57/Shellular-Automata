#include "dynamic_control.hpp"
#include "detail/shortcuts.hpp"

#include "imgui.h"

#include <algorithm>
#include <utility>

DynamicControl::DynamicControl(std::string title, std::vector<Action> actions)
    : _title{std::move(title)}, _window_title{_title + " controls"},
      _actions{std::move(actions)}, _pending(_actions.size(), false)
{
}

void DynamicControl::draw()
{
    const bool has_buttons = std::any_of(_actions.begin(), _actions.end(), [](const Action& action) {
        return action.show_button;
    });

    if (has_buttons && ImGui::BeginMainMenuBar()) {
        ImGui::PushID(this);
        if (ImGui::BeginMenu(_title.c_str())) {
            for (std::size_t i = 0; i < _actions.size(); i++) {
                const auto& action = _actions[i];
                if (!action.show_button) {
                    continue;
                }
                std::string shortcut;
                if (action.key && *action.key >= 32 && *action.key <= 126) {
                    shortcut.assign(1, static_cast<char>(*action.key));
                }
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::MenuItem(action.label.c_str(), shortcut.c_str(), false, bool(action.callback))) {
                    _pending[i] = true;
                }
                ImGui::PopID();
            }
            ImGui::Separator();
            ImGui::MenuItem("Controls", nullptr, &_show_controls);
            ImGui::EndMenu();
        }
        ImGui::PopID();
        ImGui::Separator();
        ImGui::EndMainMenuBar();
    }

    if (_show_controls) {
        if (ImGui::Begin(_window_title.c_str(), &_show_controls, ImGuiWindowFlags_AlwaysAutoResize)) {
            for (std::size_t i = 0; i < _actions.size(); i++) {
                const auto& action = _actions[i];
                if (!action.show_button) {
                    continue;
                }
                ImGui::PushID(static_cast<int>(i));
                ImGui::BeginDisabled(!action.callback);
                if (ImGui::Button(action.label.c_str())) {
                    _pending[i] = true;
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            }
        }
        ImGui::End();
    }

    const auto& io = ImGui::GetIO();
    if (controller_detail::shortcuts_allowed()) {
        for (std::size_t i = 0; i < _actions.size(); i++) {
            const auto& action = _actions[i];
            if (action.callback && action.key && *action.key >= 0 &&
                *action.key < IM_ARRAYSIZE(io.KeysDown) && ImGui::IsKeyPressed(*action.key, false)) {
                _pending[i] = true;
            }
        }
    }
}

void DynamicControl::update(Data& data)
{
    auto pending = std::exchange(_pending, std::vector<bool>(_actions.size(), false));
    for (std::size_t i = 0; i < _actions.size(); i++) {
        if (pending[i] && _actions[i].callback) {
            _actions[i].callback(data);
        }
    }
}
