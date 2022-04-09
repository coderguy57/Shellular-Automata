#include "window.hpp"
#include "engine.hpp"
#include <vector>
#include "controllers/mutation_control.hpp"
#include "controllers/statistics_control.hpp"

int main(int, char **)
{
    Window window{1280, 720};
    // Engine *e = new Engine(512, 512, 2, "slack_layer.fs");
    // Engine *e = new Engine(1024, 512, 1, "slakerz.fs");
    Engine *e = new Engine(1024, 512, 1, "slack_automata.fs");
    std::vector<GuiControl*> controls;
    controls.push_back(new MutationControl());
    controls.push_back(new StatisticsControl());
    window.run(e, controls);
}