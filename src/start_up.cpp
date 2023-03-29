#include <vector>
#include <yaml-cpp/yaml.h>

#include "window.hpp"
#include "engine.hpp"
#include "controllers/mutation_control.hpp"
#include "controllers/statistics_control.hpp"
#include "controllers/paint_control.hpp"
#include "simulation_setup.hpp"

SimulationSetup load_simulation_setup(std::string const& path)
{
    // Load the YAML configuration file
    YAML::Node config = YAML::LoadFile("../setups/" + path + ".yaml");

    // Create the engine component
    std::unique_ptr<Engine> engine;
    auto engine_node = config["engine"];
    std::string engine_name = engine_node["type"].as<std::string>();
    if (engine_name == "FragmentShaderEngine")
    {
        int width = engine_node["width"].as<int>();
        int height = engine_node["height"].as<int>();
        int depth = engine_node["depth"].as<int>();
        int steps = engine_node["steps"].as<int>();
        std::string shader_path = engine_node["shader"].as<std::string>();
        std::string format_name = engine_node["format"].as<std::string>();
        GLuint format;
        if (format_name == "GL_RGBA16")
        {
            format = GL_RGBA16;
        }
        else if (format_name == "GL_RGBA32F")
        {
            format = GL_RGBA32F;
        }
        else
        {
            throw "Unsupporeted format\n";
        }
        engine = std::make_unique<Engine>(width, height, depth, shader_path, format, steps);
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
        auto size = engine->get_size();
        viewer = std::make_unique<Viewer>(size.x, size.y, size.z, "viewer.fs", GL_RGBA32F);
    }
    else
    {
        throw "Unsupporeted viewer\n";
    }

    SimulationSetup simulation_setup(std::move(engine), std::move(viewer));

    // Create the options controllers
    for (auto controller_node : config["controllers"])
    {
        std::string controller_name = controller_node["type"].as<std::string>();
        if (controller_name == "Mutation")
        {
            simulation_setup.add_controller(std::make_unique<MutationControl>());
        }
        else if (controller_name == "Statistics")
        {
            simulation_setup.add_controller(std::make_unique<StatisticsControl>());
        }
        else if (controller_name == "Paint")
        {
            simulation_setup.add_controller(std::make_unique<PaintControl>());
        } else {
            throw "Unsupporeted controller\n";
        }
    }
    return simulation_setup;
}

int main(int, char **)
{
    Window window{1024, 512};
    auto simulation = load_simulation_setup("slackerman");
    window.run(std::move(simulation));
    // Engine *e = new Engine(1024, 512, 2, "slack_layer.fs", GL_RGBA16);
    // Engine *e = new Engine(512, 512, 1, "slackerz.fs", GL_RGBA16);
    // Engine *e = new Engine(512, 512, 1, "MNCA_asymptotic.fs", GL_RGBA16);
    // Engine *e = new Engine(1024, 512, 1, "reaction_diffusion.fs", GL_RGBA32F);
    // Engine *e = new Engine(1024, 512, 3, "conservative_layer.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(1024, 512, 3, "conservative_rulefactor.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(256, 256, 6, "seperate_gaussian.fs", GL_RGBA32F, 2);
    // Engine *e = new Engine(256, 256, 3, "conservative_rulefactorS.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(512, 256, 1, "ECA.fs", GL_RGBA32F, 1);
    // Engine *e = new Engine(1024, 512, 3, "conservative_layer_rps.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(1024, 512, 3, "conservative_layer_radius.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(1024, 512, 2, "conservative_tann.fs", GL_RGBA32F, 2);
    // Engine *e = new Engine(1024, 512, 3, "conservative_force.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(512, 512, 3, "conservative_predator.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(1024, 512, 1, "conservative.fs", GL_RGBA32F, 4);
    // Engine *e = new Engine(1024, 512, 1, "slack_automata.fs", GL_RGBA16);
    // std::vector<GuiControl *> controls;
    // controls.push_back(new MutationControl());
    // controls.push_back(new StatisticsControl());
    // controls.push_back(new PaintControl());
}