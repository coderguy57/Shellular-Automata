#include "setup/setups.hpp"

#include <memory>
#include <random>
#include <utility>
#include <vector>

#include "controllers/dynamic_control.hpp"
#include "controllers/init_texture_control.hpp"
#include "controllers/neural_mutation_control.hpp"
#include "controllers/paint_control.hpp"
#include "controllers/recorder_control.hpp"
#include "controllers/shader_control.hpp"
#include "controllers/statistics_control.hpp"
#include "engines/compute_shader_engine.hpp"
#include "render/glsl_transpiler.hpp"

SimulationSetup setup_neural_mnca() {
    const uint32_t WIDTH = 1024;
    const uint32_t HEIGHT = 1024;

    Data data;
    auto* state = data.add_element<TextureData>("data_texture", WIDTH, HEIGHT, 1);
    auto* init = data.add_element<FragmentProgramData>("init_mnca", "neural_mnca/init.fs");
    std::random_device random;
    for (auto* option : init->get_program().get_options()) {
        if (option->name == "seed" && option->type == GLSL::IOption::Type::UInt) {
            auto& seed = *static_cast<GLSL::ValueOption<uint32_t>*>(option);
            seed.value = std::uniform_int_distribution<uint32_t>{seed.min, seed.max}(random);
        }
    }

    auto engine = std::make_unique<ComputeShaderEngine>();
    auto* update = engine->add_step<ComputeShaderStep>("neural_mnca/ca.cs",
        std::vector<ComputeShaderInput>{{0, *state}},
        std::vector<ComputeShaderInput>{{2, *state}});
    update->set_data_work_group(*state);

    auto viewer = std::make_unique<Viewer>(WIDTH, HEIGHT, 1, "neural_mnca/view.fs",
        "neural_mnca/view.fs", state->options().internal_format, *state);
    std::vector<DynamicControl::Action> actions{
        {"Clear", [](Data& data) {
            auto& texture = data.get_element<TextureData>("data_texture").get_texture();
            constexpr float clear_color[4]{};
            glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
            glClearTexImage(texture.gl_tex_num, 0, GL_RGBA, GL_FLOAT, clear_color);
        }, 'P'},
    };
    std::vector<ShaderControl::Entry> shaders{
        {"Neural CA", &update->program()},
        {"Initialization", &init->get_program()},
    };

    SimulationSetup simulation(std::move(engine), std::move(viewer), std::move(data));
    simulation.add_controller_back(std::make_unique<NeuralMutationControl>(update->program()));
    simulation.add_controller_back(std::make_unique<InitTextureControl>(*init, *state));
    simulation.add_controller_back(std::make_unique<PaintControl>("neural_mnca/paint.cs", *state));
    simulation.add_controller_back(std::make_unique<DynamicControl>("Simulation", std::move(actions)));
    simulation.add_controller_back(std::make_unique<StatisticsControl>(*state));
    simulation.add_controller_back(std::make_unique<RecorderControl>(*state));
    simulation.add_controller_back(std::make_unique<ShaderControl>(std::move(shaders)));
    return simulation;
}
