#include "setup/setups.hpp"

#include <vector>
#include <yaml-cpp/yaml.h>

#include "automata/slack/rule_model.hpp"
#include "controllers/slack_rule_control.hpp"
#include "controllers/statistics_control.hpp"
#include "controllers/paint_control.hpp"
#include "controllers/recorder_control.hpp"
#include "controllers/init_texture_control.hpp"
#include "controllers/engine_control.hpp"
#include "controllers/shader_control.hpp"
#include "controllers/dynamic_control.hpp"
#include "simulation_setup.hpp"
#include "engines/compute_shader_engine.hpp"
#include "engines/fragment_shader_engine.hpp"

static void create_compute_shader_step(ComputeShaderEngine* engine, std::string shader, TextureData* input, TextureData* output) {
    std::vector<ComputeShaderInput> inputs{{0, *input}};
    std::vector<ComputeShaderInput> outputs{{1, *output}};
    auto copy_step = engine->add_step<ComputeShaderStep>(shader, inputs, outputs);
    copy_step->set_data_work_group(*input);
}

static ComputeShaderStep* create_compute_shader_step(
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

static void create_fft_step(ComputeShaderEngine* engine, TextureData* input, TextureData* output) {
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

static void create_ifft_step(ComputeShaderEngine* engine, TextureData* input, TextureData* output) {
    int size_x = input->get_texture().width;
    int size_y = input->get_texture().height;
    create_compute_shader_step(engine, "fft/ifft_x.glsl", {input}, {output})
        ->set_option("size", size_x)
        ->set_work_groups(1, size_y, 1);
    create_compute_shader_step(engine, "fft/ifft_y.glsl", {output}, {output})
        ->set_option("size", size_y)
        ->set_work_groups(size_x, 1, 1);
}

static void multiply(ComputeShaderEngine* engine, TextureData* input1, TextureData* input2, TextureData* output) {
    create_compute_shader_step(engine, "fluid2D/multiply.cs", {input1, input2}, {output});
}

static void clear_texture(Data& data, std::string const& name) {
    ComputeShaderStep clear_step("fluid2D/clear.cs", {}, {{0, name}});
    clear_step.set_data_work_group(name);
    clear_step.do_step(data);
}

SimulationSetup setup_conservative() {

    Data data = Data();

    TextureOptions options;
    options.internal_format = GL_RGBA32F;
    int x_size = 512;
    int y_size = 512;
    auto texture_data = data.add_element<TextureData>("data_texture", x_size, y_size, 1, options);
    auto mnca_texture = data.add_element<TextureData>("mnca_texture", x_size, y_size, 1, options);
    auto demand = data.add_element<TextureData>("demand_data", x_size, y_size, 1, options);
    auto demand_flux_total = data.add_element<TextureData>("demand_flux_total", x_size, y_size, 1, options);
    auto demand_fft = data.add_element<TextureData>("demand_fft", x_size, y_size, 2, options);
    auto x_vel_data = data.add_element<TextureData>("x_vel_data", x_size, y_size, 1, options);
    auto y_vel_data = data.add_element<TextureData>("y_vel_data", x_size, y_size, 1, options);
    auto temp_texture = data.add_element<TextureData>("temp_texture", x_size, y_size, 1, options);
    auto x_fft = data.add_element<TextureData>("x_fft", x_size, y_size, 2, options);
    auto y_fft = data.add_element<TextureData>("y_fft", x_size, y_size, 2, options);

    auto gaussian_fft = data.add_element<TextureData>("gaussian", x_size, y_size, 2, options);

    {
        std::unique_ptr<ComputeShaderEngine> pre_engine = std::make_unique<ComputeShaderEngine>();
        create_compute_shader_step(pre_engine.get(), "fluidMNCA/create_gaus.cs", {}, {temp_texture});
        create_fft_step(pre_engine.get(), temp_texture, gaussian_fft);
        pre_engine->step(data);
    }

    auto fragment_shader = data.add_element<FragmentProgramData>("frag_shader", "MNCA_NN.fs");
    auto& rule_model = *data.add_element<slack::RuleModel>("slack_rule");

    std::unique_ptr<ComputeShaderEngine> engine = std::make_unique<ComputeShaderEngine>();

    std::vector<FragmentShaderInput> inputs;

    create_compute_shader_step(engine.get(), "util/copy.glsl", {texture_data}, {mnca_texture});
    engine->add_step<FragmentShaderStep>(*fragment_shader, inputs, *mnca_texture, 1,
        [&rule_model](FragmentProgram& program) {
            slack::bind_rule(program, rule_model.rule());
            rule_model.dirty = false;
        });
    auto* demand_step = create_compute_shader_step(engine.get(), "fluidMNCA/mnca_delta.cs", {mnca_texture, texture_data}, {demand})
        ->set_option("Demand scale", 5.0f)
        ->set_option("Use absolute demand", true);

    create_compute_shader_step(engine.get(), "fluidMNCA/demand_flux_total.cs", {demand}, {demand_flux_total});
    create_compute_shader_step(engine.get(), "fluidMNCA/demand_flux.cs", {texture_data, demand, demand_flux_total}, {texture_data});

    create_fft_step(engine.get(), demand, demand_fft);
    // create_compute_shader_step(engine.get(), "fluidMNCA/ca_fft_diff.cs", {demand_fft}, {demand_fft});
    multiply(engine.get(), demand_fft, gaussian_fft, demand_fft);
    create_ifft_step(engine.get(), demand_fft, demand_fft);

    auto* force_x_step = create_compute_shader_step(engine.get(), "fluidMNCA/gradient_x.cs", {demand_fft, x_vel_data}, {x_vel_data})
        ->set_option("Force scale", 11.0f);
    auto* force_y_step = create_compute_shader_step(engine.get(), "fluidMNCA/gradient_y.cs", {demand_fft, y_vel_data}, {y_vel_data})
        ->set_option("Force scale", 11.0f);

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

    create_compute_shader_step(engine.get(), "util/copy.glsl", {x_fft}, {x_vel_data});
    create_compute_shader_step(engine.get(), "util/copy.glsl", {y_fft}, {y_vel_data});

    auto* velocity_step = create_compute_shader_step(engine.get(), "fluidMNCA/velocity_flux.cs", {x_vel_data, y_vel_data, texture_data}, {texture_data})
        ->set_option("Velocity scale", 2000.0f)
        ->set_option("Velocity flux fraction", 0.95f);

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
        "data_texture");

    auto init_shader = data.add_element<FragmentProgramData>("init_mnca", "fluidMNCA/init_mnca.fs");

    auto clear_vel = [](Data& data) {
        clear_texture(data, "x_vel_data");
        clear_texture(data, "y_vel_data");
    };
    auto clear_material = [clear_vel](Data& data) {
        clear_texture(data, "data_texture");
        clear_vel(data);
    };
    std::vector<DynamicControl::Action> actions{
        {"Clear", clear_material, 'P'},
        {"Reset velocity", clear_vel, 'Q'},
        {"Reset velocity on mutation", clear_vel, 'V', false},
    };

    SimulationSetup simulation_setup(std::move(engine), std::move(viewer), std::move(data));

    simulation_setup.add_controller_back(std::make_unique<SlackRuleControl>(rule_model));

    simulation_setup.add_controller_back(std::make_unique<RecorderControl>(*texture_data));

    simulation_setup.add_controller_back(std::make_unique<StatisticsControl>(*texture_data));

    simulation_setup.add_controller_back(std::make_unique<InitTextureControl>(*init_shader, *texture_data));

    simulation_setup.add_controller_back(std::make_unique<PaintControl>("fluidMNCA/paint.cs", *texture_data));

    simulation_setup.add_controller_back(std::make_unique<DynamicControl>("Simulation", std::move(actions)));

    std::vector<ShaderControl::Entry> shader_entries{
        {"CA", &fragment_shader->get_program()},
        {"Demand", &demand_step->program()},
        {"Force X", &force_x_step->program()},
        {"Force Y", &force_y_step->program()},
        {"Material transport", &velocity_step->program()},
    };
    simulation_setup.add_controller_back(std::make_unique<ShaderControl>(std::move(shader_entries)));

    return simulation_setup;
}
