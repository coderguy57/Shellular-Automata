#include "setup/setups.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "controllers/dynamic_control.hpp"
#include "controllers/init_texture_control.hpp"
#include "controllers/mutation_control_NN.hpp"
#include "controllers/paint_control.hpp"
#include "controllers/recorder_control.hpp"
#include "controllers/shader_control.hpp"
#include "controllers/statistics_control.hpp"
#include "engines/compute_shader_engine.hpp"

SimulationSetup setup_mnca_nn() {
    Data data;
    auto* texture = data.add_element<TextureData>("data_texture", 512, 512, 1);
    auto* ca = data.add_element<FragmentProgramData>("frag_shader", "slack_automata/slack_automata.fs");
    auto* init = data.add_element<FragmentProgramData>("init_mnca", "slack_automata/init.fs");

    auto engine = std::make_unique<ComputeShaderEngine>();
    engine->add_step<FragmentShaderStep>(*ca, std::vector<FragmentShaderInput>{}, *texture, 1);

    auto viewer = std::make_unique<Viewer>(texture->width(), texture->height(), texture->depth(),
        "viewer.fs", "basic.fs", texture->options().internal_format, *texture);

    std::vector<DynamicControl::Action> actions{
        {"Clear", [](Data& data) {
            auto& texture = data.get_element<TextureData>("data_texture").get_texture();
            constexpr float clear_color[4]{};
            glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
            glClearTexImage(texture.gl_tex_num, 0, GL_RGBA, GL_FLOAT, clear_color);
        }, 'P'},
    };
    std::vector<ShaderControl::Entry> shader_entries{
        {"CA", &ca->get_program()},
        {"Initialization", &init->get_program()},
    };

    SimulationSetup simulation(std::move(engine), std::move(viewer), std::move(data));
    simulation.add_controller_back(std::make_unique<MutationControlNN>(*ca));
    simulation.add_controller_back(std::make_unique<RecorderControl>(*texture));
    simulation.add_controller_back(std::make_unique<StatisticsControl>(*texture));
    simulation.add_controller_back(std::make_unique<InitTextureControl>(*init, *texture));
    simulation.add_controller_back(std::make_unique<PaintControl>("fluidMNCA/paint.cs", *texture));
    simulation.add_controller_back(std::make_unique<DynamicControl>("Simulation", std::move(actions)));
    simulation.add_controller_back(std::make_unique<ShaderControl>(std::move(shader_entries)));
    return simulation;
}
