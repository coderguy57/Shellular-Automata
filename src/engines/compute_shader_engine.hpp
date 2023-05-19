#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <string>
#include <GL/glew.h>

#include "engines/engine.hpp"
#include "render/texture.hpp"

class Texture;
class ComputeProgram;

struct ComputeShaderInput
{
    GLuint layout;
    std::string name;
};
class ComputeShaderStep
{
public:
    ComputeShaderStep(std::string compute_path, std::vector<ComputeShaderInput> inputs, std::vector<ComputeShaderInput> outputs);
    void do_step(Data &data);
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
    void add_step(ComputeShaderStep &&step)
    {
        steps.push_back(std::move(step));
    }

private:
    std::vector<ComputeShaderStep> steps;
};