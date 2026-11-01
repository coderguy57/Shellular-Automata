#include "rule_browser_control.hpp"

#include "data.hpp"
#include "imgui.h"
#include "render/detail/fullscreen_pass.hpp"
#include "render/framebuffer.hpp"
#include "render/glsl_transpiler.hpp"
#include "render/renderSurface.hpp"
#include "render/shader.hpp"

#include <algorithm>
#include <cstdio>
#include <utility>
#include <vector>

RuleBrowserControl::RuleBrowserControl(rule_browser::State& browser, rules::RuleModel& current,
        rules::RuleModel& candidates,
        TextureData& main, TextureData& previews, TextureData& display, FragmentProgram& initializer,
        std::string rule_menu)
    : browser_(browser), current_rules_(current), candidate_rules_(candidates),
      rule_menu_(std::move(rule_menu)), main_(main),
      previews_(previews), display_(display), initializer_(initializer),
      compositor_(std::make_unique<ComputeProgram>("rule_browser/view.cs")),
      framebuffer_(std::make_unique<Framebuffer>()), surface_(std::make_unique<RenderSurface>()),
      initial_field_(std::make_unique<Texture>(previews.width(), previews.height(), 1, previews.options()))
{
}

RuleBrowserControl::~RuleBrowserControl() = default;

bool RuleBrowserControl::initialize(Texture& destination)
{
    if (!initializer_.use()) {
        error_ = "Initialization shader did not link";
        return false;
    }
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    framebuffer_->use();
    framebuffer_->set_texture(0, destination);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        error_ = "Initialization framebuffer is incomplete";
        return false;
    }
    render_detail::setup_fullscreen_pass(destination.width, destination.height);
    initializer_.set_uniform("rand_seed", 0u);
    initializer_.set_uniform("texture_width", static_cast<float>(destination.width));
    initializer_.set_uniform("texture_height", static_cast<float>(destination.height));
    surface_->draw();
    error_.clear();
    return true;
}

bool RuleBrowserControl::reset_previews()
{
    if (!initialize(*initial_field_))
        return false;
    initial_options_.clear();
    for (auto* option : initializer_.get_options()) {
        if (option->type == GLSL::IOption::Type::Float)
            initial_options_.push_back({option->name, static_cast<GLSL::ValueOption<float>*>(option)->value});
        else if (option->type == GLSL::IOption::Type::UInt)
            initial_options_.push_back({option->name, static_cast<GLSL::ValueOption<std::uint32_t>*>(option)->value});
        else if (option->type == GLSL::IOption::Type::Int)
            initial_options_.push_back({option->name, static_cast<GLSL::ValueOption<int>*>(option)->value});
        else if (option->type == GLSL::IOption::Type::Bool)
            initial_options_.push_back({option->name, static_cast<GLSL::BoolOption*>(option)->value});
    }
    // All candidates start from exactly the same field, including both ping-pong images.
    for (auto* target : {&previews_.get_texture(), &previews_.get_last_texture()}) {
        for (int layer = 0; layer < rule_browser::browser_count; ++layer)
            glCopyImageSubData(initial_field_->gl_tex_num, initial_field_->target, 0, 0, 0, 0,
                               target->gl_tex_num, target->target, 0, 0, 0, layer,
                               previews_.width(), previews_.height(), 1);
    }
    browser_.steps = 0;
    return true;
}

void RuleBrowserControl::new_generation(bool mutate)
{
    if (current_rules_.size() != 1 || candidate_rules_.size() != rule_browser::browser_count) {
        error_ = "Rule counts do not match the browser";
        return;
    }
    if (!reset_previews())
        return;
    if (mutate) {
        // Slot zero keeps the parent unchanged while the remaining slots become offspring.
        if (!candidate_rules_.copy_from(candidate_rules_, browser_.selected, 0)) {
            error_ = "Cannot copy the selected rule";
            return;
        }
        for (int index = 1; index < rule_browser::browser_count; ++index) {
            candidate_rules_.copy_from(candidate_rules_, 0, index);
            candidate_rules_.mutate(browser_.mutation, index);
        }
    } else {
        const bool first_generation = browser_.generation == 0;
        if (first_generation && !candidate_rules_.copy_from(current_rules_)) {
            error_ = "Incompatible rule models";
            return;
        }
        for (int index = first_generation ? 1 : 0; index < rule_browser::browser_count; ++index)
            candidate_rules_.randomize(index);
    }
    browser_.selected = 0;
    browser_.steps = 0;
    ++browser_.generation;
}

void RuleBrowserControl::toggle()
{
    if (!browser_.enabled && browser_.generation == 0) {
        new_generation(false);
        if (browser_.generation == 0)
            return;
    }
    browser_.enabled = !browser_.enabled;
    apply_on_release_ = false;
}

void RuleBrowserControl::apply_selected()
{
    // Promote the initialization settings that actually produced this generation.
    for (auto* option : initializer_.get_options()) {
        const auto saved = std::find_if(initial_options_.begin(), initial_options_.end(), [&](const auto& value) {
            return value.name == option->name;
        });
        if (saved == initial_options_.end())
            continue;
        if (option->type == GLSL::IOption::Type::Float && std::holds_alternative<float>(saved->value)) {
            auto* value = static_cast<GLSL::ValueOption<float>*>(option);
            value->value = std::get<float>(saved->value);
            value->changed = true;
        } else if (option->type == GLSL::IOption::Type::UInt && std::holds_alternative<std::uint32_t>(saved->value)) {
            auto* value = static_cast<GLSL::ValueOption<std::uint32_t>*>(option);
            value->value = std::get<std::uint32_t>(saved->value);
            value->changed = true;
        } else if (option->type == GLSL::IOption::Type::Int && std::holds_alternative<int>(saved->value)) {
            auto* value = static_cast<GLSL::ValueOption<int>*>(option);
            value->value = std::get<int>(saved->value);
            value->changed = true;
        } else if (option->type == GLSL::IOption::Type::Bool && std::holds_alternative<bool>(saved->value)) {
            auto* value = static_cast<GLSL::BoolOption*>(option);
            value->value = std::get<bool>(saved->value);
            value->changed = true;
        }
    }
    if (!initialize(main_.get_last_texture()))
        return;
    if (!current_rules_.copy_from(candidate_rules_, browser_.selected)) {
        error_ = "Cannot apply the selected rule";
        return;
    }
    main_.update();
    browser_.enabled = false;
    apply_on_release_ = false;
}

void RuleBrowserControl::draw_controls()
{
    ImGui::SetNextWindowSizeConstraints(ImVec2(290, 0), ImVec2(380, 480));
    if (ImGui::Begin("Rule browser controls", &show_controls_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Generation %llu | Step %llu",
                    static_cast<unsigned long long>(browser_.generation),
                    static_cast<unsigned long long>(browser_.steps));
        ImGui::Text("Candidate %d", browser_.selected + 1);
        if (ImGui::Button("Fresh batch"))
            new_generation(false);
        ImGui::SameLine();
        if (ImGui::Button("Mutate selected"))
            new_generation(true);
        if (ImGui::Button("Reset previews"))
            reset_previews();
        ImGui::SameLine();
        if (ImGui::Button("Use selected rule"))
            apply_selected();
        ImGui::SetNextItemWidth(140);
        ImGui::SliderFloat("Mutation strength", &browser_.mutation.strength, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SetNextItemWidth(140);
        ImGui::SliderFloat("Mutation rate", &browser_.mutation.rate, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::Button("Close browser"))
            toggle();
        if (!error_.empty())
            ImGui::TextWrapped("%s", error_.c_str());
    }
    ImGui::End();
}

void RuleBrowserControl::draw_grid()
{
    const auto& io = ImGui::GetIO();
    if (io.DisplaySize.x <= 0 || io.DisplaySize.y <= 0)
        return;
    const ImVec2 cell(io.DisplaySize.x / rule_browser::browser_columns,
                      io.DisplaySize.y / rule_browser::browser_columns);
    if (!io.WantCaptureMouse && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) &&
        ImGui::IsMouseClicked(0, false) && io.MousePos.x >= 0 && io.MousePos.y >= 0 &&
        io.MousePos.x < io.DisplaySize.x && io.MousePos.y < io.DisplaySize.y) {
        browser_.selected = static_cast<int>(io.MousePos.y / cell.y) * rule_browser::browser_columns
            + static_cast<int>(io.MousePos.x / cell.x);
        if (ImGui::IsMouseDoubleClicked(0))
            apply_on_release_ = true;
    }
    auto* draw = ImGui::GetBackgroundDrawList();
    for (int index = 0; index < rule_browser::browser_count; ++index) {
        const ImVec2 min((index % rule_browser::browser_columns) * cell.x,
                         (index / rule_browser::browser_columns) * cell.y);
        const ImVec2 max(min.x + cell.x, min.y + cell.y);
        draw->AddRect(ImVec2(min.x + 1, min.y + 1), ImVec2(max.x - 1, max.y - 1),
                      index == browser_.selected ? IM_COL32(130, 230, 130, 255) : IM_COL32(65, 65, 65, 255),
                      0.0f, 0, index == browser_.selected ? 3.0f : 1.0f);
        const float label_y = min.y + (index < rule_browser::browser_columns ? ImGui::GetFrameHeight() + 4 : 6);
        char label[8];
        std::snprintf(label, sizeof(label), "%d", index + 1);
        const ImVec2 text(min.x + 8, label_y);
        const ImVec2 size = ImGui::CalcTextSize(label);
        draw->AddRectFilled(ImVec2(text.x - 3, text.y - 2),
                            ImVec2(text.x + size.x + 3, text.y + size.y + 2), IM_COL32(0, 0, 0, 190));
        draw->AddText(text, IM_COL32(235, 235, 235, 255), label);
    }
}

void RuleBrowserControl::compose()
{
    if (!compositor_->use()) {
        error_ = "Browser display shader did not link";
        return;
    }
    const auto& source = browser_.enabled ? previews_.get_texture() : main_.get_texture();
    const auto& target = display_.get_last_texture();
    compositor_->set_texture(0, "source_texture", &source);
    glBindImageTexture(1, target.gl_tex_num, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    compositor_->set_uniform("browser_enabled", browser_.enabled);
    compositor_->set_uniform("columns", rule_browser::browser_columns);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    compositor_->run((display_.width() + 15) / 16, (display_.height() + 15) / 16, 1);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
    display_.update();
}

void RuleBrowserControl::draw()
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu(browser_.enabled ? "Rule browser" : rule_menu_.c_str())) {
            if (ImGui::MenuItem(browser_.enabled ? "Enabled" : "Rule browser", "B", browser_.enabled))
                toggle();
            if (browser_.enabled) {
                if (ImGui::MenuItem("Fresh batch"))
                    new_generation(false);
                if (ImGui::MenuItem("Mutate selected", "V"))
                    new_generation(true);
                if (ImGui::MenuItem("Reset previews", "Q"))
                    reset_previews();
                if (ImGui::MenuItem("Use selected rule"))
                    apply_selected();
                ImGui::MenuItem("Controls", nullptr, &show_controls_);
            }
            if (!error_.empty())
                ImGui::TextWrapped("%s", error_.c_str());
            ImGui::EndMenu();
        }
        if (browser_.enabled)
            ImGui::Separator();
        ImGui::EndMainMenuBar();
    }
    if (browser_.enabled && show_controls_)
        draw_controls();
    if (shortcuts_allowed()) {
        if (ImGui::IsKeyPressed('B', false))
            toggle();
        if (browser_.enabled && ImGui::IsKeyPressed('V', false))
            new_generation(true);
        if (browser_.enabled && ImGui::IsKeyPressed('Q', false))
            reset_previews();
    }
    if (browser_.enabled) {
        draw_grid();
        if (apply_on_release_ && !ImGui::IsMouseDown(0))
            apply_selected();
    }
    compose();
}

void RuleBrowserControl::update(Data&)
{
    if (!browser_.enabled)
        return;
    candidate_rules_.upload();
}

void RuleBrowserControl::post_process(Data&)
{
    if (browser_.enabled)
        ++browser_.steps;
}
