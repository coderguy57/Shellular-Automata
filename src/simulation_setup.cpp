#include "simulation_setup.hpp"

#include "engines/engine.hpp"
#include "viewer.hpp"
#include "controllers/gui_control.hpp"

Engine& SimulationSetup::engine() const {
	return *engine_;
}
Viewer& SimulationSetup::viewer() const {
	return *viewer_;
}
std::vector<std::unique_ptr<GuiControl>> const& SimulationSetup::controllers() const {
	return controllers_;
}

void SimulationSetup::add_controller_back(std::unique_ptr<GuiControl> controller) {
	controllers_.emplace_back(std::move(controller));
}

void SimulationSetup::add_controller_front(std::unique_ptr<GuiControl> controller) {
	controllers_.insert(controllers_.begin(), std::move(controller));
}
