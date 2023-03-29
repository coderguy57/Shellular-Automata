#pragma once

#include <memory>
#include "engine.hpp"
#include "viewer.hpp"
#include "controllers/gui_control.hpp"

class SimulationSetup
{
public:
	SimulationSetup(std::unique_ptr<Engine> engine, std::unique_ptr<Viewer> viewer)
		: engine_{std::move(engine)}, viewer_{std::move(viewer)} {}

	SimulationSetup &operator=(SimulationSetup &) = delete;
	SimulationSetup(SimulationSetup &) = delete;
	SimulationSetup &operator=(SimulationSetup &&) = default;
	SimulationSetup(SimulationSetup &&) = default;

	Engine &engine() const;
	Viewer &viewer() const;
	std::vector<std::unique_ptr<GuiControl>> const &controllers() const;
	void add_controller(std::unique_ptr<GuiControl> controller);

private:
	std::unique_ptr<Engine> engine_;
	std::unique_ptr<Viewer> viewer_;
	std::vector<std::unique_ptr<GuiControl>> controllers_;
};