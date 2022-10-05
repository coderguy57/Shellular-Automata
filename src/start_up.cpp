#include "window.hpp"
#include "engine.hpp"
#include <vector>
#include "controllers/mutation_control.hpp"
#include "controllers/statistics_control.hpp"
#include "controllers/paint_control.hpp"

int main(int, char **)
{
    Window window{1024, 512};
    // Engine *e = new Engine(1024, 512, 2, "slack_layer.fs", GL_RGBA16);
    // Engine *e = new Engine(512, 512, 1, "slackerz.fs", GL_RGBA16);
    // Engine *e = new Engine(512, 512, 1, "MNCA_asymptotic.fs", GL_RGBA16);
    // Engine *e = new Engine(1024, 512, 1, "reaction_diffusion.fs", GL_RGBA32F);
    // Engine *e = new Engine(1024, 512, 3, "conservative_layer.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(1024, 512, 3, "conservative_rulefactor.fs", GL_RGBA32F, 3);
    Engine *e = new Engine(1024, 512, 3, "conservative_rulefactorS.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(1024, 512, 3, "conservative_layer_rps.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(1024, 512, 3, "conservative_layer_radius.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(1024, 512, 2, "conservative_tann.fs", GL_RGBA32F, 2);
    // Engine *e = new Engine(1024, 512, 3, "conservative_force.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(512, 512, 3, "conservative_predator.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(1024, 512, 1, "conservative.fs", GL_RGBA32F, 4);
    // Engine *e = new Engine(1024, 512, 1, "slack_automata.fs", GL_RGBA16);
    std::vector<GuiControl *> controls;
    controls.push_back(new MutationControl());
    controls.push_back(new StatisticsControl());
    controls.push_back(new PaintControl());
    window.run(e, controls);
}