#pragma once

#include "gui_control.hpp"
#include "automata/rule_browser.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

class TextureData;
class FragmentProgram;
class ComputeProgram;
class Framebuffer;
class RenderSurface;
class Texture;

class RuleBrowserControl final : public GuiControl {
public:
    RuleBrowserControl(rule_browser::State& browser, rules::RuleModel& current,
                       rules::RuleModel& candidates,
                       TextureData& main, TextureData& previews, TextureData& display,
                       FragmentProgram& initializer, std::string rule_menu = "Rules");
    ~RuleBrowserControl() override;
    void draw() override;
    void update(Data&) override;
    void post_process(Data&) override;

private:
    bool reset_previews();
    void toggle();
    void new_generation(bool mutate);
    void apply_selected();
    void draw_controls();
    void draw_grid();
    void compose();
    bool initialize(Texture& destination);

    rule_browser::State& browser_;
    rules::RuleModel& current_rules_;
    rules::RuleModel& candidate_rules_;
    std::string rule_menu_;
    TextureData& main_;
    TextureData& previews_;
    TextureData& display_;
    FragmentProgram& initializer_;
    std::unique_ptr<ComputeProgram> compositor_;
    std::unique_ptr<Framebuffer> framebuffer_;
    std::unique_ptr<RenderSurface> surface_;
    std::unique_ptr<Texture> initial_field_;
    struct InitializationOption {
        std::string name;
        std::variant<float, std::uint32_t, int, bool> value;
    };
    std::vector<InitializationOption> initial_options_;
    bool show_controls_ = false;
    bool apply_on_release_ = false;
    std::string error_;
};
