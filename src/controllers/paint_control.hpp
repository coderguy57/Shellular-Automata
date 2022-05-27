#pragma once

#include <glm/vec3.hpp>

#include "gui_control.hpp"

class Program;
class PaintControl : public GuiControl
{
public:
    PaintControl();
    ~PaintControl();
    void draw() override;
    void update(Program *program) override;
    void post_process(Texture *texture) override {};

private:
    void draw_options();

    bool _show_options = false;
    int _cursor_size = 1;
    bool _cursor_smooth = true;
    int _layer = 0;
    glm::vec3 _color = glm::vec3(1, 1, 1);
    glm::bvec3 _color_mask = glm::vec3(true, true, true);
};
