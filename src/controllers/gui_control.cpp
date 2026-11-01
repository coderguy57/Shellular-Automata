#include "gui_control.hpp"

#include "imgui.h"

bool GuiControl::shortcuts_allowed()
{
    // A focused panel alone must not disable application shortcuts.
    return !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}
