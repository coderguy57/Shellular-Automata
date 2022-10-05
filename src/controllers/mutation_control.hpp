#pragma once

#include <vector>
#include <cstdint>

#include "gui_control.hpp"

class Program;
class MutationControl : public GuiControl
{
public:
    MutationControl();
    ~MutationControl();
    void draw() override;
    void update(Program *program) override;
    void post_process(Texture *texture) override{};

private:
    void draw_mutation_window();

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
