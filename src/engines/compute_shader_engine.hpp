#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <GL/glew.h>

#include "engines/engine.hpp"
#include "render/texture.hpp"
#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"

class Texture;
class ComputeProgram;

struct FragmentShaderInput
{
    std::string shader_name;
    std::string data_name;
};
struct Step {
    virtual ~Step() {};
    virtual void do_step(Data &data) = 0;
};
struct FragmentShaderStep : public Step
{
public:
    FragmentShaderStep(std::string fragment_name, std::vector<FragmentShaderInput> inputs, std::string output, int stages);
    void do_step(Data &data) override;

private:
    std::unique_ptr<Framebuffer> fbo_;
    std::unique_ptr<RenderSurface> surface_;
    std::string fragment_name_;
    std::vector<FragmentShaderInput> inputs_;
    std::string output_;
    uint32_t stages_;
    uint32_t counter_ = 1560281088;
};

struct ComputeShaderInput
{
    GLuint layout;
    std::string name;
};
class ComputeShaderStep : public Step
{
public:
    ComputeShaderStep(std::string compute_path, std::vector<ComputeShaderInput> inputs, std::vector<ComputeShaderInput> outputs);
    void do_step(Data &data) override;
    void set_iterations(int iterations) {
        iterations_ = iterations;
    };
    void set_work_groups(int x, int y, int z) {
        work_group_x_ = x;
        work_group_y_ = y;
        work_group_z_ = z;
    };
    void set_data_work_group(std::string data_work_group) {
        data_work_group_ = data_work_group;
    };

private:
    std::unique_ptr<ComputeProgram> step_;
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
    void add_step(std::unique_ptr<Step> step)
    {
        steps.push_back(std::move(step));
    }

private:
    std::vector<std::unique_ptr<Step>> steps;
};