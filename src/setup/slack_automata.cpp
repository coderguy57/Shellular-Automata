#include "setup/setups.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "automata/rule_browser.hpp"
#include "automata/slack/rule_model.hpp"
#include "controllers/control_group.hpp"
#include "controllers/dynamic_control.hpp"
#include "controllers/init_texture_control.hpp"
#include "controllers/slack_rule_control.hpp"
#include "controllers/paint_control.hpp"
#include "controllers/recorder_control.hpp"
#include "controllers/rule_browser_control.hpp"
#include "controllers/shader_control.hpp"
#include "controllers/statistics_control.hpp"
#include "engines/compute_shader_engine.hpp"
#include "engines/select_step.hpp"

static constexpr char ca_shader[] = "slack_automata/slack_automata.cs";

static std::unique_ptr<Step> create_compute_shader_step(
    ComputeProgram& program, TextureData& texture, rules::RuleModel& rules)
{
    auto step = std::make_unique<ComputeShaderStep>(program, ca_shader,
        std::vector<ComputeShaderInput>{{0, texture}, {4, rules}},
        std::vector<ComputeShaderInput>{{2, texture}});
    step->set_data_work_group(texture);
    return step;
}

static void clear_texture(Data& data)
{
    auto& texture = data.get_element<TextureData>("data_texture").get_texture();
    constexpr float clear_color[4]{};
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    glClearTexImage(texture.gl_tex_num, 0, GL_RGBA, GL_FLOAT, clear_color);
}

SimulationSetup setup_slack_automata() {
    constexpr uint32_t width = 512;
    constexpr uint32_t height = 512;

    Data data;
    auto* texture = data.add_element<TextureData>("data_texture", width, height, 1);
    auto* previews = data.add_element<TextureData>("rule_browser_state",
        rule_browser::preview_size, rule_browser::preview_size, rule_browser::browser_count, texture->options());
    auto* display = data.add_element<TextureData>("rule_browser_display", width, height, 1);
    auto& browser = *data.add_element<rule_browser::State>("rule_browser");
    auto& main_rules = *data.add_element<slack::RuleModel>("slack_rules");
    auto& preview_rules = *data.add_element<slack::RuleModel>("rule_browser_rules", rule_browser::browser_count);
    auto& program = data.add_element<ComputeProgramData>("slack_ca", ca_shader)->get_program();
    auto* init = data.add_element<FragmentProgramData>("init_mnca", "slack_automata/init.fs");

    auto engine = std::make_unique<ComputeShaderEngine>();
    std::vector<std::unique_ptr<Step>> alternatives;
    alternatives.push_back(create_compute_shader_step(program, *texture, main_rules));
    alternatives.push_back(create_compute_shader_step(program, *previews, preview_rules));
    engine->add_step<SelectStep>([&browser] { return browser.enabled ? 1u : 0u; }, std::move(alternatives));

    auto viewer = std::make_unique<Viewer>(width, height, 1,
        "viewer.fs", "basic.fs", texture->options().internal_format, *display);

    std::vector<DynamicControl::Action> actions{
        {"Clear", clear_texture, 'P'},
    };
    std::vector<ShaderControl::Entry> shader_entries{
        {"CA", &program},
        {"Initialization", &init->get_program()},
    };

    auto browser_control = std::make_unique<RuleBrowserControl>(browser, main_rules, preview_rules,
        *texture, *previews, *display, init->get_program(), "Slack rule");
    std::vector<std::unique_ptr<GuiControl>> normal_controls;
    normal_controls.push_back(std::make_unique<SlackRuleControl>(main_rules));
    normal_controls.push_back(std::make_unique<RecorderControl>(*texture));
    normal_controls.push_back(std::make_unique<StatisticsControl>(*texture));
    normal_controls.push_back(std::make_unique<InitTextureControl>(*init, *texture));
    normal_controls.push_back(std::make_unique<PaintControl>("fluidMNCA/paint.cs", *texture));
    normal_controls.push_back(std::make_unique<DynamicControl>("Simulation", std::move(actions)));

    SimulationSetup simulation(std::move(engine), std::move(viewer), std::move(data));
    simulation.add_controller_back(std::move(browser_control));
    simulation.add_controller_back(std::make_unique<ControlGroup>([&browser] { return !browser.enabled; },
        std::move(normal_controls)));
    simulation.add_controller_back(std::make_unique<ShaderControl>(std::move(shader_entries)));
    return simulation;
}
