#pragma once

#include <unordered_map>
#include <functional>
#include <vector>
#include <memory>
#include <string>
#include <GL/glew.h>

#include "engines/engine.hpp"
#include "engines/step.hpp"
#include "render/texture.hpp"
#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"
#include "render/glsl_transpiler.hpp"
#include "render/shader.hpp"

class Texture;
class ComputeProgram;

struct FragmentShaderInput
{
    std::string shader_name;
    std::string data_name;
};
struct FragmentShaderStep : public Step
{
public:
    FragmentShaderStep(std::string fragment_name, std::vector<FragmentShaderInput> inputs, std::string output, int stages,
                       std::function<void(FragmentProgram&)> prepare = {});
    void do_step(Data &data) override;

private:
    std::unique_ptr<Framebuffer> fbo_;
    std::unique_ptr<RenderSurface> surface_;
    std::string fragment_name_;
    std::vector<FragmentShaderInput> inputs_;
    std::string output_;
    std::function<void(FragmentProgram&)> prepare_;
    uint32_t stages_;
    uint32_t counter_ = 1560281088;
};

struct ComputeShaderInput
{
    ComputeShaderInput(GLuint layout_, std::string name_) : layout{layout_}, name{name_} {}
    GLuint layout;
    std::string name;
};
class ComputeShaderStep : public Step
{
public:
    ComputeShaderStep(std::string compute_path, std::vector<ComputeShaderInput> inputs, std::vector<ComputeShaderInput> outputs);
    // The borrowed program must outlive this step.
    ComputeShaderStep(ComputeProgram& program, std::string name,
                      std::vector<ComputeShaderInput> inputs, std::vector<ComputeShaderInput> outputs);
    void do_step(Data &data) override;
    ComputeProgram& program()
    {
        return *step_;
    }
    std::string const& shader_name() const
    {
        return compute_path_;
    }
    ComputeShaderStep* set_iterations(int iterations)
    {
        iterations_ = iterations;
        return this;
    };
    ComputeShaderStep* set_work_groups(int x, int y, int z)
    {
        work_group_x_ = x;
        work_group_y_ = y;
        work_group_z_ = z;
        data_work_group_ = "";
        return this;
    };
    ComputeShaderStep* set_data_work_group(std::string data_work_group)
    {
        data_work_group_ = data_work_group;
        return this;
    };
    ComputeShaderStep* set_option(std::string name, bool value)
    {
        const auto &options = step_->get_options();
        auto it = std::find_if(options.begin(), options.end(),
                               [&name](GLSL::IOption *option)
                               { return option->label == name; });
        if (it != std::end(options) && (*it)->type == GLSL::IOption::Type::Bool) {
            auto opt = static_cast<GLSL::BoolOption*>(*it);
            opt->changed = true;
            opt->value = value;
        }
        return this;
    };
    template <typename T>
    ComputeShaderStep* set_option(std::string name, T value)
    {
        const auto &options = step_->get_options();
        auto it = std::find_if(options.begin(), options.end(),
                               [&name](GLSL::IOption *option)
                               { return option->label == name; });
        if (it != std::end(options)) {
            auto option = *it;
            if (option->type == GLSL::IOption::Type::Int) {
                auto opt = static_cast<GLSL::ValueOption<int> *>(option);
                opt->changed = true;
                opt->value = static_cast<int>(value);
            } else if (option->type == GLSL::IOption::Type::UInt) {
                auto opt = static_cast<GLSL::ValueOption<uint32_t> *>(option);
                opt->changed = true;
                opt->value = static_cast<uint32_t>(value);
            } else if (option->type == GLSL::IOption::Type::Float) {
                auto opt = static_cast<GLSL::ValueOption<float> *>(option);
                opt->changed = true;
                opt->value = static_cast<float>(value);
            }
        }
        return this;
    };

private:
    std::unique_ptr<ComputeProgram> owned_program_;
    ComputeProgram* step_;
    std::string compute_path_;
    std::vector<ComputeShaderInput> inputs_;
    std::vector<ComputeShaderInput> outputs_;
    std::string data_work_group_ = "";
    uint work_group_x_ = 1;
    uint work_group_y_ = 1;
    uint work_group_z_ = 1;
    int iterations_ = 1;
};

class ComputeShaderEngine : public Engine
{
public:
    ComputeShaderEngine(){};
    ~ComputeShaderEngine(){};
    void step(Data &data) override;
    std::vector<ComputeShaderStep*> compute_steps();
    void add_step(std::unique_ptr<Step> step)
    {
        steps.push_back(std::move(step));
    }
    template <typename T, typename... Args>
    T *add_step(Args &&...args)
    {
        auto step = std::make_unique<T>(std::forward<Args>(args)...);
        auto step_ptr = step.get();
        steps.push_back(std::move(step));
        return step_ptr;
    }

private:
    std::vector<std::unique_ptr<Step>> steps;
};
