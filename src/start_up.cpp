#include <iostream>

#include "window.hpp"
#include "simulation_setup_loader.hpp"

int main(int, char **)
{
    Window window{512, 512};
    auto simulation = load_simulation_setup("fluid.yaml");
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