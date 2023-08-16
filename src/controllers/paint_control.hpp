#pragma once

#include <glm/vec3.hpp>
#include <memory>
#include <string>

#include "gui_control.hpp"
#include "render/shader.hpp"

class Program;
class RenderSurface;
class Framebuffer;
class Data;
class PaintControl : public GuiControl
{
public:
    PaintControl(std::string shader, std::string texture);
    ~PaintControl();
    void draw() override;
    void update(Data& data) override;
    void post_process(Data& data) override {};

private:
    void draw_options();

    std::string texture_name_;
    std::unique_ptr<ComputeProgram> compute_program_;
    std::unique_ptr<Framebuffer> fbo_;
    std::unique_ptr<RenderSurface> surface_;
    std::string fragment_name_;
    glm::vec2 last_cursor_pos_;

    bool show_options_ = false;
    int cursor_size_ = 1;
    bool cursor_smooth_ = true;
    int layer_ = 0;
    glm::vec3 color_ = glm::vec3(1, 1, 1);
    glm::bvec3 color_mask_ = glm::vec3(true, true, true);
};
