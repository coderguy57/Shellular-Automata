#include "window.hpp"
#include "engine.hpp"
#include <vector>
#include "controllers/mutation_control.hpp"
#include "controllers/statistics_control.hpp"

int main(int, char **)
{
    Window window{1024, 512};
    // Engine *e = new Engine(512, 512, 2, "slack_layer.fs");
    // Engine *e = new Engine(512, 512, 1, "slackerz.fs");
    // Engine *e = new Engine(1024, 512, 3, "conservative_layer.fs", 3);
    // Engine *e = new Engine(1024, 512, 3, "conservative_force.fs", 3);
    Engine *e = new Engine(1024, 512, 3, "conservative_predator.fs", 3);
    // Engine *e = new Engine(1024, 512, 1, "conservative.fs", 4);
    // Engine *e = new Engine(512, 512, 1, "slack_automata.fs");
    std::vector<GuiControl*> controls;
    controls.push_back(new MutationControl());
    controls.push_back(new StatisticsControl());
    window.run(e, controls);
}