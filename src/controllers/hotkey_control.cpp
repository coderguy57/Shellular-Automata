#include "controllers/hotkey_control.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"
#include "data.hpp"

HotkeyControl::HotkeyControl(std::vector<HotkeyCommand>&& hotkeys)
: hotkeys{std::move(hotkeys)}
{
}

void HotkeyControl::draw()
{
}

void HotkeyControl::update(Data& data)
{
    for (auto& hotkey : hotkeys) {
        if (ImGui::IsKeyPressed(hotkey.key))
            hotkey.command(data);
    }
}