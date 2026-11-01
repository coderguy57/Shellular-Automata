#include "rule_control.hpp"

#include "automata/rule_model.hpp"
#include "imgui.h"

#include <utility>

RuleControl::RuleControl(rules::RuleModel& model, std::string label)
    : model_(model), label_(std::move(label)), window_name_(label_ + " controls") {}

void RuleControl::draw()
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu(label_.c_str())) {
            ImGui::MenuItem("Controls", nullptr, &show_window_);
            ImGui::Separator();
            if (ImGui::MenuItem("Mutate", "V"))
                model_.mutate(model_.mutation);
            if (ImGui::MenuItem("Randomize"))
                model_.randomize();
            if (ImGui::MenuItem("Reset rule"))
                model_.initialize();
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "S"))
                model_.save();
            if (ImGui::MenuItem("Restore", "L"))
                model_.restore();
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::EndMainMenuBar();
    }

    if (show_window_)
        draw_window();
    if (shortcuts_allowed()) {
        if (ImGui::IsKeyPressed('S', false))
            model_.save();
        if (ImGui::IsKeyPressed('L', false))
            model_.restore();
        if (ImGui::IsKeyPressed('V', false))
            model_.mutate(model_.mutation);
    }
}

void RuleControl::draw_window()
{
    ImGui::SetNextWindowSizeConstraints(ImVec2(360, 0), ImVec2(480, 600));
    if (ImGui::Begin(window_name_.c_str(), &show_window_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushItemWidth(240);
        if (ImGui::Button("Mutate"))
            model_.mutate(model_.mutation);
        ImGui::SameLine();
        if (ImGui::Button("Randomize"))
            model_.randomize();
        ImGui::SameLine();
        if (ImGui::Button("Reset rule"))
            model_.initialize();
        if (ImGui::Button("Save"))
            model_.save();
        ImGui::SameLine();
        if (ImGui::Button("Restore"))
            model_.restore();

        ImGui::Separator();
        ImGui::SliderFloat("Mutation strength", &model_.mutation.strength,
            0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Mutation rate", &model_.mutation.rate,
            0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        constexpr std::uint32_t seed_step = 1u;
        if (ImGui::InputScalar("Seed", ImGuiDataType_U32, &model_.seed, &seed_step))
            model_.reseed(model_.seed);

        ImGui::Separator();
        model_.dirty |= draw_parameters();
        ImGui::PopItemWidth();
    }
    ImGui::End();
}
