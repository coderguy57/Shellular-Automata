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
    int size_z = input->get_texture().depth;
    create_compute_shader_step(engine, "fft2D/fft.glsl", {input}, {output})
        ->set_option("complex_in", false)
        ->set_option("horizontal", true)
        ->set_option("size", size_x)
        ->set_work_groups(1, size_y, size_z);
    create_compute_shader_step(engine, "fft2D/fft.glsl", {output}, {output})
        ->set_option("horizontal", false)
        ->set_option("size", size_y)
        ->set_work_groups(size_x, 1, size_z);
}

static void create_ifft_step(ComputeShaderEngine* engine, TextureData* input, TextureData* output) {
    int size_x = output->get_texture().width;
    int size_y = output->get_texture().height;
    int size_z = output->get_texture().depth;
    create_compute_shader_step(engine, "fft2D/fft.glsl", {input}, {input})
        ->set_option("horizontal", true)
        ->set_option("inverse", true)
        ->set_option("size", size_x)
        ->set_work_groups(1, size_y, size_z);
    create_compute_shader_step(engine, "fft2D/fft.glsl", {input}, {output})
        ->set_option("horizontal", false)
        ->set_option("inverse", true)
        ->set_option("normalization", size_x * size_y)
        ->set_option("complex_out", false)
        ->set_option("size", size_y)
        ->set_work_groups(size_x, 1, size_z);
}

static void multiply(ComputeShaderEngine* engine, TextureData* input1, TextureData* input2, TextureData* output) {
    create_compute_shader_step(engine, "fluid2D/multiply.cs", {input1, input2}, {output});
}

SimulationSetup setup_fluid() {
    Data data = Data();

    TextureOptions options;
    options.internal_format = GL_RGBA32F;
    int x_size = 512;
    int y_size = 512;
    auto data_texture = data.add_element<TextureData>("texture", x_size, y_size, 2, options);
    auto temp_texture = data.add_element<TextureData>("temp_texture", x_size, y_size, 2, options);
    auto temp_texture2 = data.add_element<TextureData>("temp_texture2", x_size, y_size, 2, options);

    std::unique_ptr<ComputeShaderEngine> engine = std::make_unique<ComputeShaderEngine>();

    create_compute_shader_step(engine.get(), "fluid2D/copy.cs", {data_texture}, {temp_texture});
    create_compute_shader_step(engine.get(), "fluid2D/diffusion.cs", {data_texture, temp_texture}, {data_texture}, 20);

    create_compute_shader_step(engine.get(), "fluid2D/div.cs", {data_texture}, {temp_texture});
    create_compute_shader_step(engine.get(), "fluid2D/clear.cs", {}, {temp_texture2});
    create_compute_shader_step(engine.get(), "fluid2D/lin_solve.cs", {temp_texture2, temp_texture}, {temp_texture2}, 20);
    create_compute_shader_step(engine.get(), "fluid2D/project.cs", {data_texture, temp_texture2}, {data_texture});

    create_compute_shader_step(engine.get(), "fluid2D/advect_vel.cs", {data_texture}, {data_texture});

    create_compute_shader_step(engine.get(), "fluid2D/div.cs", {data_texture}, {temp_texture});
    create_compute_shader_step(engine.get(), "fluid2D/clear.cs", {}, {temp_texture2});
    create_compute_shader_step(engine.get(), "fluid2D/lin_solve.cs", {temp_texture2, temp_texture}, {temp_texture2}, 20);
    create_compute_shader_step(engine.get(), "fluid2D/project.cs", {data_texture, temp_texture2}, {data_texture});

    create_compute_shader_step(engine.get(), "fluid2D/advect.cs", {data_texture}, {data_texture});

    std::string view_shader = "fluid2D/fluid_viewer.fs";
    std::string output_shader = "basic.fs";
    auto& texture = data_texture->get_texture();
    auto size = texture.get_size();
    
    auto viewer = std::make_unique<Viewer>(
        size.x, 
        size.y, 
        size.z, 
        view_shader, 
        output_shader, 
        data_texture->get_texture().options.internal_format, 
        "texture");

    auto* compute_engine = engine.get();
    SimulationSetup simulation_setup(std::move(engine), std::move(viewer), std::move(data));

    simulation_setup.add_controller_back(std::make_unique<InitTextureControl>("init.cs", *data_texture));

    simulation_setup.add_controller_back(std::make_unique<PaintControl>("fluid2D/paint_vel.cs", *data_texture));

    simulation_setup.add_controller_back(std::make_unique<ShaderControl>(*compute_engine));

    return simulation_setup;
}
