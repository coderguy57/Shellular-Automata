#include "simulation_setup_loader.hpp"

#include <vector>
#include <yaml-cpp/yaml.h>

#include "controllers/mutation_control.hpp"
#include "controllers/statistics_control.hpp"
#include "controllers/paint_control.hpp"
#include "controllers/recorder_control.hpp"
#include "controllers/init_texture_control.hpp"
#include "simulation_setup.hpp"
#include "engines/compute_shader_engine.hpp"
#include "engines/fragment_shader_engine.hpp"

GLuint get_format(std::string format_name)
{
    if (format_name == "GL_RGBA16")
    {
        return GL_RGBA16;
    }
    else if (format_name == "GL_RGBA32F")
    {
        return GL_RGBA32F;
    }
    else
    {
        throw "Unsupporeted format\n";
    }
}

SimulationSetup load_simulation_setup(std::string const &path)
{
    // Load the YAML configuration file
    YAML::Node config = YAML::LoadFile("../setups/" + path);

    Data data = Data();

    for (auto data_element : config["data"])
    {
        std::string type = data_element["type"].as<std::string>();
        std::string name = data_element["name"].as<std::string>();
        if (type == "texture")
        {
            int width = data_element["width"].as<int>();
            int height = data_element["height"].as<int>();
            std::string format_name = data_element["format"].as<std::string>();
            TextureOptions options;
            options.internal_format = get_format(format_name);
            auto texture_data = std::make_unique<TextureData>(width, height, options);
            data.add_element(name, std::move(texture_data));
        }
        else if (type == "fragment_shader") {
            std::string path = data_element["path"].as<std::string>();
            auto fragment_shader_data = std::make_unique<FragmentProgramData>(path);
            data.add_element(name, std::move(fragment_shader_data));
        }
        else
        {
            throw "Unsupporeted data type\n";
        }
    }

    // Create the engine component
    std::unique_ptr<Engine> engine;
    auto engine_node = config["engine"];
    auto test = engine_node["type"];
    std::string engine_name = engine_node["type"].as<std::string>();
    if (engine_name == "FragmentShaderEngine")
    {
        int steps = engine_node["steps"].as<int>();
        std::string shader_name = engine_node["shader"].as<std::string>();
        std::string texture_name = engine_node["texture"].as<std::string>();
        engine = std::make_unique<FragmentShaderEngine>(shader_name, texture_name, steps);
    }
    else if (engine_name == "ComputeShaderEngine")
    {
        std::unique_ptr<ComputeShaderEngine> compute_engine = std::make_unique<ComputeShaderEngine>();
        for (auto step : engine_node["steps"])
        {
            std::string compute_shader = step["name"].as<std::string>();
            YAML::Node inputs_node = step["inputs"];
            std::vector<ComputeShaderInput> inputs;
            for(YAML::const_iterator it=inputs_node.begin();it!=inputs_node.end();++it) {
                GLuint key = it->first.as<GLuint>();
                std::string name = it->second.as<std::string>();
                inputs.push_back({key, name});
            }
            YAML::Node outputs_node = step["outputs"];
            std::vector<ComputeShaderInput> outputs;
            for(YAML::const_iterator it=outputs_node.begin();it!=outputs_node.end();++it) {
                GLuint key = it->first.as<GLuint>();
                std::string name = it->second.as<std::string>();
                outputs.push_back({key, name});
            }
            ComputeShaderStep compute_shader_step(compute_shader, inputs, outputs);
            if (step["work_groups"]) {
                std::vector<GLuint> work_groups = step["work_groups"].as<std::vector<GLuint>>();
                if (work_groups.size() != 3) {
                    throw "Unsupporeted ComputeShaderEngine step work groups\n";
                }
                compute_shader_step.set_work_groups(work_groups[0], work_groups[1], work_groups[2]);
            } else {
                std::string work_group_data = step["work_group_data"].as<std::string>();
                compute_shader_step.set_data_work_group(work_group_data);
            }

            if (step["iterations"]) {
                int iterations = step["iterations"].as<int>();
                compute_shader_step.set_iterations(iterations);
            }
            compute_engine->add_step(std::move(compute_shader_step));
        }
        engine = std::move(compute_engine);
    }
    else
    {
        throw "Unsupporeted engine\n";
    }

    // Create the visualizer
    std::unique_ptr<Viewer> viewer;
    auto viewer_node = config["visualizer"];
    std::string viewer_name = viewer_node["type"].as<std::string>();
    if (viewer_name == "2DRenderer")
    {
        std::string input_name = viewer_node["input"].as<std::string>();
        std::string view_shader = viewer_node["view_shader"].as<std::string>();
        std::string output_shader = viewer_node["output_shader"].as<std::string>();
        auto& texture_data = data.get_element<TextureData>(input_name);
        auto& texture = texture_data.get_texture();
        auto size = texture.get_size();
        
        viewer = std::make_unique<Viewer>(size.x, size.y, size.z, view_shader, output_shader, texture.options.internal_format, input_name);
    }
    else
    {
        throw "Unsupporeted viewer\n";
    }

    SimulationSetup simulation_setup(std::move(engine), std::move(viewer), std::move(data));

    // Create the options controllers
    for (auto controller_node : config["controllers"])
    {
        std::string controller_name = controller_node["type"].as<std::string>();
        if (controller_name == "Mutation")
        {
            std::string fragment_name = controller_node["shader"].as<std::string>();
            simulation_setup.add_controller_back(std::make_unique<MutationControl>(fragment_name));
        }
        else if (controller_name == "Statistics")
        {
            std::string texture_name = controller_node["texture"].as<std::string>();
            simulation_setup.add_controller_back(std::make_unique<StatisticsControl>(texture_name));
        }
        else if (controller_name == "Paint")
        {
            std::string shader_name = controller_node["shader"].as<std::string>();
            std::string texture_name = controller_node["texture"].as<std::string>();
            simulation_setup.add_controller_back(std::make_unique<PaintControl>(shader_name, texture_name));
        }
        else if (controller_name == "Recorder")
        {
            std::string texture_name = controller_node["texture"].as<std::string>();
            simulation_setup.add_controller_back(std::make_unique<RecorderControl>(texture_name));
        }
        else if (controller_name == "InitTexture")
        {
            std::string shader_name = controller_node["shader"].as<std::string>();
            std::string texture_name = controller_node["texture"].as<std::string>();
            simulation_setup.add_controller_back(std::make_unique<InitTextureControl>(shader_name, texture_name));
        }
        else
        {
            throw "Unsupporeted controller\n";
        }
    }
    return simulation_setup;
}
