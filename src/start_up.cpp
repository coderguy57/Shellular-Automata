#include "window.hpp"
#include "engine.hpp"
#include <vector>
#include "controllers/mutation_control.hpp"
#include "controllers/statistics_control.hpp"

int main(int, char **)
{
    Window window{1024, 512};
    // Engine *e = new Engine(1024, 512, 2, "slack_layer.fs", GL_RGBA16);
    Engine *e = new Engine(1024, 512, 1, "slackerz.fs", GL_RGBA16);
    // Engine *e = new Engine(1024, 512, 3, "conservative_layer.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(1024, 512, 3, "conservative_force.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(1024, 512, 3, "conservative_predator.fs", GL_RGBA32F, 3);
    // Engine *e = new Engine(1024, 512, 1, "conservative.fs", GL_RGBA32F, 4);
    // Engine *e = new Engine(1024, 512, 1, "slack_automata.fs", GL_RGBA16);
    std::vector<GuiControl *> controls;
    controls.push_back(new MutationControl());
    controls.push_back(new StatisticsControl());
    window.run(e, controls);
}