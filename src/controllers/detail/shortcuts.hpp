#pragma once

#include "imgui.h"

namespace controller_detail {
inline bool shortcuts_allowed()
{
    // A focused panel alone must not disable application shortcuts.
    return !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
}
}
