#pragma once

#include "gui_control.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

class Data;
class ComputeProgram;
class ComputeShaderEngine;
class FragmentProgram;
class ShaderControl : public GuiControl
{
public:
    struct Entry {
        std::string label;
        // Programs are non-owning and must outlive the controller.
        std::variant<FragmentProgram*, ComputeProgram*> program;
    };

    ShaderControl() = default;
    explicit ShaderControl(ComputeShaderEngine& compute_engine);
    // An explicit list, even an empty one, disables automatic discovery.
    explicit ShaderControl(std::vector<Entry> entries);
    ~ShaderControl() = default;
    void draw() override;
    void update(Data& data) override{};
    void post_process(Data& data) override;

private:
    void draw_shader_options_menu();
    
    // TODO: Add data to draw instead
    Data* data_ = nullptr;
    ComputeShaderEngine* compute_engine_ = nullptr;
    std::optional<std::vector<Entry>> entries_;

    bool show_shader_options_ = false;
};
