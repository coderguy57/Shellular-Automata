#include "simulation_setup.hpp"

Engine& SimulationSetup::engine() const {
	return *engine_;	
}
Viewer& SimulationSetup::viewer() const {
	return *viewer_;
}
std::vector<std::unique_ptr<GuiControl>> const& SimulationSetup::controllers() const {
	return controllers_;
}

void SimulationSetup::add_controller(std::unique_ptr<GuiControl> controller) {
	controllers_.emplace_back(std::move(controller));
}
