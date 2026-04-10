#include "setup/setups.hpp"

#include <vector>
#include <yaml-cpp/yaml.h>

#include "controllers/mutation_control.hpp"
#include "controllers/statistics_control.hpp"
#include "controllers/paint_control.hpp"
#include "controllers/recorder_control.hpp"
#include "controllers/init_texture_control.hpp"
#include "controllers/engine_control.hpp"
#include "controllers/shader_control.hpp"
#include "controllers/hotkey_control.hpp"
#include "simulation_setup.hpp"
#include "engines/compute_shader_engine.hpp"
#include "engines/fragment_shader_engine.hpp"

void create_compute_shader_step(ComputeShaderEngine* engine, std::string shader, TextureData* input, TextureData* output) {
    std::vector<ComputeShaderInput> inputs{{0, *input}};
    std::vector<ComputeShaderInput> outputs{{1, *output}};
    auto copy_step = engine->add_step<ComputeShaderStep>(shader, inputs, outputs);
    copy_step->set_data_work_group(*input);
}

ComputeShaderStep* create_compute_shader_step(
    ComputeShaderEngine* engine, 
    std::string shader, 
    std::initializer_list<TextureData*> inputs,
    std::initializer_list<TextureData*> outputs,
    int iterations = 1) 
{
    GLuint i = 0;
    std::vector<ComputeShaderInput> step_inputs;
    for (auto& input : inputs) {
        step_inputs.push_back({i, *input});
        i++;
    }
    std::vector<ComputeShaderInput> step_outputs;
    for (auto& output : outputs) {
        step_outputs.push_back({i, *output});
        i++;
    }
    auto step = engine->add_step<ComputeShaderStep>(shader, step_inputs, step_outputs);
    if (inputs.size() > 0)
        step->set_data_work_group(**inputs.begin());
    else
        step->set_data_work_group(**outputs.begin());
    step->set_iterations(iterations);
    return step;
}

void create_fft_step(ComputeShaderEngine* engine, TextureData* input, TextureData* output) {
    int size_x = input->get_texture().width;
    int size_y = input->get_texture().height;
    create_compute_shader_step(engine, "fft/fft_x.glsl", {input}, {output})
        ->set_option("complex", false)
        ->set_option("size", size_x)
        ->set_work_groups(1, size_y, 1);
    create_compute_shader_step(engine, "fft/fft_y.glsl", {output}, {output})
        ->set_option("complex", true)
        ->set_option("size", size_y)
        ->set_work_groups(size_x, 1, 1);
}

void create_ifft_step(ComputeShaderEngine* engine, TextureData* input, TextureData* output) {
    int size_x = input->get_texture().width;
    int size_y = input->get_texture().height;
    create_compute_shader_step(engine, "fft/ifft_x.glsl", {input}, {output})
        ->set_option("size", size_x)
        ->set_work_groups(1, size_y, 1);
    create_compute_shader_step(engine, "fft/ifft_y.glsl", {output}, {output})
        ->set_option("size", size_y)
        ->set_work_groups(size_x, 1, 1);
}

void multiply(ComputeShaderEngine* engine, TextureData* input1, TextureData* input2, TextureData* output) {
    create_compute_shader_step(engine, "fluidMNCA/multiply.cs", {input1, input2}, {output})
        ->set_option("layers", input1->get_texture().depth);
}

SimulationSetup setup_conservative() {

    Data data = Data();

    TextureOptions options;
    options.internal_format = GL_RGBA32F;
    int x_size = 512;
    int y_size = 512;
    auto texture_data = data.add_element<TextureData>("data_texture", x_size, y_size, 2, options);
    auto demand = data.add_element<TextureData>("demand_data", x_size, y_size, 1, options);
    auto demand_fft = data.add_element<TextureData>("demand_fft", x_size, y_size, 2, options);
    auto x_vel_data = data.add_element<TextureData>("x_vel_data", x_size, y_size, 1, options);
    auto y_vel_data = data.add_element<TextureData>("y_vel_data", x_size, y_size, 1, options);
    auto temp_texture = data.add_element<TextureData>("temp_texture", x_size, y_size, 1, options);
    auto temp_texture2 = data.add_element<TextureData>("temp_texture2", x_size, y_size, 1, options);
    auto x_fft = data.add_element<TextureData>("x_fft", x_size, y_size, 2, options);
    auto y_fft = data.add_element<TextureData>("y_fft", x_size, y_size, 2, options);

    auto gaussian_fft = data.add_element<TextureData>("gaussian", x_size, y_size, 2, options);

    {
        std::unique_ptr<ComputeShaderEngine> pre_engine = std::make_unique<ComputeShaderEngine>();
        create_compute_shader_step(pre_engine.get(), "fluidMNCA/create_gaus.cs", {}, {temp_texture});
        create_fft_step(pre_engine.get(), temp_texture, gaussian_fft);
        pre_engine->step(data);
    }

    auto fragment_shader = data.add_element<FragmentProgramData>("frag_shader", "MNCA_asymptotic.fs");

    std::unique_ptr<ComputeShaderEngine> engine = std::make_unique<ComputeShaderEngine>();

    std::vector<FragmentShaderInput> inputs;

    engine->add_step<FragmentShaderStep>(*fragment_shader, inputs, *texture_data, 1);
    create_compute_shader_step(engine.get(), "util/copy.glsl", {texture_data}, {demand});
    create_compute_shader_step(engine.get(), "util/copy.glsl", {demand}, {temp_texture2});


    create_fft_step(engine.get(), demand, demand_fft);
    // create_compute_shader_step(engine.get(), "fluidMNCA/ca_fft_diff.cs", {demand_fft}, {demand_fft});
    multiply(engine.get(), demand_fft, gaussian_fft, demand_fft);
    create_ifft_step(engine.get(), demand_fft, demand_fft);

    create_compute_shader_step(engine.get(), "fluidMNCA/sobel_x.cs", {demand_fft, x_vel_data}, {x_vel_data});
    create_compute_shader_step(engine.get(), "fluidMNCA/sobel_y.cs", {demand_fft, y_vel_data}, {y_vel_data});

    create_fft_step(engine.get(), x_vel_data, x_fft);
    create_fft_step(engine.get(), y_vel_data, y_fft);
    create_compute_shader_step(engine.get(), "fluidMNCA/ca_fft_diff.cs", {x_fft}, {x_fft});
    create_compute_shader_step(engine.get(), "fluidMNCA/ca_fft_diff.cs", {y_fft}, {y_fft});
    create_compute_shader_step(engine.get(), "fluidMNCA/ca_fft_project.cs", {x_fft, y_fft}, {x_fft, y_fft});
    create_ifft_step(engine.get(), x_fft, x_fft);
    create_ifft_step(engine.get(), y_fft, y_fft);

    create_compute_shader_step(engine.get(), "util/copy.glsl", {x_fft}, {x_vel_data});
    create_compute_shader_step(engine.get(), "util/copy.glsl", {y_fft}, {y_vel_data});

    create_compute_shader_step(engine.get(), "fluidMNCA/ca_advect_vel.cs", {x_vel_data, y_vel_data}, {x_vel_data, y_vel_data});

    create_fft_step(engine.get(), x_vel_data, x_fft);
    create_fft_step(engine.get(), y_vel_data, y_fft);
    create_compute_shader_step(engine.get(), "fluidMNCA/ca_fft_diff.cs", {x_fft}, {x_fft});
    create_compute_shader_step(engine.get(), "fluidMNCA/ca_fft_diff.cs", {y_fft}, {y_fft});
    create_compute_shader_step(engine.get(), "fluidMNCA/ca_fft_project.cs", {x_fft, y_fft}, {x_fft, y_fft});
    create_ifft_step(engine.get(), x_fft, x_fft);
    create_ifft_step(engine.get(), y_fft, y_fft);

    create_compute_shader_step(engine.get(), "fluidMNCA/ca_advect.cs", {x_vel_data, y_vel_data, texture_data}, {texture_data});

    create_compute_shader_step(engine.get(), "fluidMNCA/ca_div.cs", {x_vel_data}, {temp_texture});

    std::string view_shader = "viewer.fs";
    std::string output_shader = "basic.fs";
    auto& texture = texture_data->get_texture();
    auto size = texture.get_size();
    
    auto viewer = std::make_unique<Viewer>(
        size.x, 
        size.y, 
        size.z, 
        view_shader, 
        output_shader, 
        texture_data->get_texture().options.internal_format, 
        *texture_data);

    auto init_shader = data.add_element<FragmentProgramData>("init_mnca", "fluidMNCA/init_mnca.fs");

    auto clear_vel = [&](Data& data) {
        std::vector<ComputeShaderInput> step_inputs{};
        auto x_step = ComputeShaderStep("fluid_sim/clear.cs", step_inputs, {{0, "x_vel_data"}});
        x_step.set_data_work_group("x_vel_data");
        x_step.do_step(data);

        auto y_step = ComputeShaderStep("fluid_sim/clear.cs", step_inputs, {{0, "y_vel_data"}});
        y_step.set_data_work_group("y_vel_data");
        y_step.do_step(data);
    };
    std::vector<HotkeyControl::HotkeyCommand> hotkeys;
    hotkeys.push_back({'Q', clear_vel});

    SimulationSetup simulation_setup(std::move(engine), std::move(viewer), std::move(data));

    simulation_setup.add_controller_back(std::make_unique<MutationControl>(*fragment_shader));

    simulation_setup.add_controller_back(std::make_unique<RecorderControl>(*texture_data));

    simulation_setup.add_controller_back(std::make_unique<InitTextureControl>(*init_shader, *texture_data));

    simulation_setup.add_controller_back(std::make_unique<HotkeyControl>(std::move(hotkeys)));

    simulation_setup.add_controller_back(std::make_unique<ShaderControl>());

    return simulation_setup;
}