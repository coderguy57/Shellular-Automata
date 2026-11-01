#include <iostream>

#include "window.hpp"
#include "simulation_setup_loader.hpp"

#include "setup/setups.hpp"

int main(int, char **)
{
    Window window{768, 768};
    auto simulation = setup_neural_mnca();
    window.run(std::move(simulation));
}
