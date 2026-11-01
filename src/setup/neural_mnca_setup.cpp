#include "setup/setups.hpp"

#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "automata/neural/rule_model.hpp"
#include "automata/rule_browser.hpp"
#include "controllers/control_group.hpp"
#include "controllers/dynamic_control.hpp"
#include "controllers/init_texture_control.hpp"
#include "controllers/rule_browser_control.hpp"
#include "controllers/neural_rule_control.hpp"
#include "controllers/paint_control.hpp"
#include "controllers/recorder_control.hpp"
#include "controllers/shader_control.hpp"
#include "controllers/statistics_control.hpp"
#include "engines/compute_shader_engine.hpp"
#include "engines/select_step.hpp"
#include "render/glsl_transpiler.hpp"

static constexpr char ca_shader[] = "neural_mnca/ca.cs";

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

static void randomize_initialization_seed(FragmentProgram& program)
{
    std::random_device random;
    for (auto* option : program.get_options()) {
        if (option->name == "seed" && option->type == GLSL::IOption::Type::UInt) {
            auto& seed = *static_cast<GLSL::ValueOption<uint32_t>*>(option);
            seed.value = std::uniform_int_distribution<uint32_t>{seed.min, seed.max}(random);
        }
    }
}

SimulationSetup setup_neural_mnca() {
    constexpr uint32_t width = 1024;
    constexpr uint32_t height = 1024;

    Data data;
    auto* state = data.add_element<TextureData>("data_texture", width, height, 1);
    auto* previews = data.add_element<TextureData>("rule_browser_state",
        rule_browser::preview_size, rule_browser::preview_size, rule_browser::browser_count, state->options());
    auto* display = data.add_element<TextureData>("rule_browser_display", width, height, 1);
    auto& browser = *data.add_element<rule_browser::State>("rule_browser");
    auto& program = data.add_element<ComputeProgramData>("neural_ca", ca_shader)->get_program();
    auto& main_rules = *data.add_element<neural::RuleModel>("neural_rules");
    auto& preview_rules = *data.add_element<neural::RuleModel>("rule_browser_rules", rule_browser::browser_count);
    auto* init = data.add_element<FragmentProgramData>("init_mnca", "neural_mnca/init.fs");
    randomize_initialization_seed(init->get_program());

    auto engine = std::make_unique<ComputeShaderEngine>();
    std::vector<std::unique_ptr<Step>> alternatives;
    alternatives.push_back(create_compute_shader_step(program, *state, main_rules));
    alternatives.push_back(create_compute_shader_step(program, *previews, preview_rules));
    engine->add_step<SelectStep>([&browser] { return browser.enabled ? 1u : 0u; }, std::move(alternatives));

    auto viewer = std::make_unique<Viewer>(width, height, 1, "neural_mnca/view.fs",
        "neural_mnca/view.fs", state->options().internal_format, *display);
    std::vector<DynamicControl::Action> actions{
        {"Clear", clear_texture, 'P'},
    };
    std::vector<ShaderControl::Entry> shaders{
        {"Neural CA", &program},
        {"Initialization", &init->get_program()},
    };

    auto browser_control = std::make_unique<RuleBrowserControl>(browser, main_rules, preview_rules,
        *state, *previews, *display, init->get_program(), "Neural rule");
    std::vector<std::unique_ptr<GuiControl>> normal_controls;
    normal_controls.push_back(std::make_unique<NeuralRuleControl>(main_rules));
    normal_controls.push_back(std::make_unique<InitTextureControl>(*init, *state));
    normal_controls.push_back(std::make_unique<PaintControl>("neural_mnca/paint.cs", *state));
    normal_controls.push_back(std::make_unique<DynamicControl>("Simulation", std::move(actions)));
    normal_controls.push_back(std::make_unique<StatisticsControl>(*state));
    normal_controls.push_back(std::make_unique<RecorderControl>(*state));

    SimulationSetup simulation(std::move(engine), std::move(viewer), std::move(data));
    simulation.add_controller_back(std::move(browser_control));
    simulation.add_controller_back(std::make_unique<ControlGroup>([&browser] { return !browser.enabled; },
        std::move(normal_controls)));
    simulation.add_controller_back(std::make_unique<ShaderControl>(std::move(shaders)));
    return simulation;
}
