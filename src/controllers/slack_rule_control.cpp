#include "slack_rule_control.hpp"

#include "automata/slack/rule.hpp"
#include "imgui.h"

#include <cstdint>
#include <string>

namespace {
void draw_col_button(int& id, std::uint32_t& state, unsigned offset, unsigned color,
                     bool same_line = true)
{
    ImGui::PushID(id++);
    const auto selected = state & (3u << offset);
    const ImVec4 color_vector{
        color == 0 || color == 3 ? 0.9f : 0.2f,
        color == 1 || color == 3 ? 0.9f : 0.2f,
        color == 2 || color == 3 ? 0.9f : 0.2f, 1.0f};
    ImGui::PushStyleColor(ImGuiCol_Button, selected == (color << offset)
        ? color_vector : ImVec4{0.2f, 0.3f, 0.3f, 1.0f});
    if (ImGui::Button("", ImVec2(10, 10)))
        state = (state & ~(3u << offset)) | (color << offset);
    ImGui::PopStyleColor();
    ImGui::PopID();
    if (same_line)
        ImGui::SameLine();
}

void draw_button(int& id, std::uint32_t& state, unsigned offset, bool same_line = false)
{
    ImGui::PushID(id++);
    ImGui::PushStyleColor(ImGuiCol_Button, state & (std::uint32_t{1} << offset)
        ? ImVec4{0.2f, 0.5f, 0.8f, 1.0f} : ImVec4{0.2f, 0.3f, 0.3f, 1.0f});
    if (ImGui::Button("", ImVec2(10, 10)))
        state ^= std::uint32_t{1} << offset;
    ImGui::PopStyleColor();
    ImGui::PopID();
    if (same_line)
        ImGui::SameLine();
}

void draw_slider(int& id, std::uint8_t& state)
{
    constexpr std::uint8_t minimum = 0;
    constexpr std::uint8_t maximum = 255;
    ImGui::PushID(id++);
    const auto label = std::to_string(int(state) - 128);
    ImGui::SliderScalar("", ImGuiDataType_U8, &state, &minimum, &maximum, label.c_str());
    ImGui::PopID();
    ImGui::SameLine();
}
}

SlackRuleControl::SlackRuleControl(slack::RuleModel& model)
    : RuleControl(model, "Slack rule"), model_(model) {}

bool SlackRuleControl::draw_parameters()
{
    auto& rule = model_.rule();
    const auto previous = rule;
    auto& mutation = rule.words;
    ImGui::SliderFloat("Scale", &rule.scale, 0.0f, 200.0f, "%.3f");
    ImGui::SliderFloat("Zoom", &rule.zoom, 0.0f, 1.0f);
    ImGui::SliderInt("Mode", &rule.mode, 0, 2);

    int id = 0;
    for (int row = 0; row < 24; ++row) {
        for (int column = 0; column < 16; ++column) {
            if (column > 0)
                ImGui::SameLine();
            draw_button(id, mutation[row / 2], column + (row % 2) * 16);
        }
        auto scale = static_cast<std::uint8_t>(mutation[12 + row] & 0xffu);
        ImGui::PushItemWidth(200);
        std::uint32_t used = scale != 128;
        draw_button(id, used, 0, true);
        draw_slider(id, scale);
        mutation[12 + row] = (mutation[12 + row] & ~0xffu) | scale;
        for (unsigned color = 0; color < 4; ++color)
            draw_col_button(id, mutation[38 + row / 8], (row * 4) % 32, color);
        for (unsigned color = 0; color < 4; ++color)
            draw_col_button(id, mutation[41 + row / 8], (row * 4) % 32, color);
        ImGui::PopItemWidth();
        ImGui::Separator();
    }
    return rule != previous;
}
