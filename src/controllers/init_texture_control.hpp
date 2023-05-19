#pragma once

#include "gui_control.hpp"
#include "engines/fragment_shader_engine.hpp"
#include <string>
#include <memory>

class Program;
class Texture;
class Data;
class ComputeProgram;
class InitTextureControl : public GuiControl
{
public:
    InitTextureControl(std::string shader_name, std::string texture_name);
    ~InitTextureControl(){};
    void draw() override;
    void update(Data& data) override;
    void post_process(Data& data) override{};

private:
    bool init_ = true;
    std::unique_ptr<ComputeProgram> program_;
    std::string texture_name_;
};