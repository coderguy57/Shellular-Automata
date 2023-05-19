#pragma once

#include <memory>
#include <vector>

class Engine;
#include "viewer.hpp"
#include "data.hpp"
#include "controllers/gui_control.hpp"

class SimulationSetup
{
public:
	SimulationSetup(std::unique_ptr<Engine> engine, std::unique_ptr<Viewer> viewer, Data&& data)
		: engine_{std::move(engine)}, viewer_{std::move(viewer)}, data_{std::move(data)} {}

	SimulationSetup &operator=(SimulationSetup &) = delete;
	SimulationSetup(SimulationSetup &) = delete;
	SimulationSetup &operator=(SimulationSetup &&) = default;
	SimulationSetup(SimulationSetup &&) = default;

	Engine &engine() const;
	Viewer &viewer() const;
	std::vector<std::unique_ptr<GuiControl>> const &controllers() const;
	void add_controller_back(std::unique_ptr<GuiControl> controller);
	void add_controller_front(std::unique_ptr<GuiControl> controller);
    // void add_data_element(std::string name, std::unique_ptr<DataElement> data_element) {
    //     data_.add_element(name, std::move(data_element));
    // };
    Data& data() {
        return data_;
    }
    

private:
    Data data_;
	std::unique_ptr<Engine> engine_;
	std::unique_ptr<Viewer> viewer_;
	std::vector<std::unique_ptr<GuiControl>> controllers_;
};