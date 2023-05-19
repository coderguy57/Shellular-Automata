#pragma once

#include <vector>
#include <cstdint>
#include <string>

#include "gui_control.hpp"

class Program;
class Data;
class MutationControl : public GuiControl
{
public:
    MutationControl(std::string fragment_name);
    ~MutationControl();
    void draw() override;
    void update(Data& data) override;
    void post_process(Data& data) override{};

private:
    void draw_mutation_window();

    std::string _fragment_name;
    bool _show_mutation_window;
    std::vector<uint32_t> _mutation;
    std::vector<uint32_t> _mutation_save;
    std::vector<uint8_t> _mutation_scale;
    int _mutation_chance = 100;
    float _scale = 70;
    float _zoom = 0;
    bool _reset = true;
    bool _clear = false;
    int _mode = 0;
    uint32_t _frames = 0;
    uint32_t _seed = 0;
};
