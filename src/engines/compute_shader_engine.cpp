#include "compute_shader_engine.hpp"

#include "render/shader.hpp"
#include "render/texture.hpp"
#include "data.hpp"

void ComputeShaderEngine::step(Data &data)
{
    for (auto &step : steps)
    {
        step.do_step(data);
    }
}

ComputeShaderStep::ComputeShaderStep(
    std::string compute_path,
    std::vector<ComputeShaderInput> inputs, std::vector<ComputeShaderInput> outputs)
{
    step_ = std::make_unique<ComputeProgram>(compute_path);
    inputs_ = inputs;
    outputs_ = outputs;
}
void ComputeShaderStep::do_step(Data &data)
{
    for (int i = 0; i < iterations_; i++) {
        step_->use();
        GLuint memory_barrier_bits = 0;
        for (const auto &input : inputs_)
        {
            auto &data_element = data.get_element(input.name);
            data_element.bind(input.layout);
            memory_barrier_bits |= data_element.memory_barrier_bits();
        }
        for (const auto &output : outputs_)
        {
            auto &data_element = data.get_element(output.name);
            data_element.bind_out(output.layout);
            memory_barrier_bits |= data_element.memory_barrier_bits();
        }

        if (data_work_group_ == "")
            step_->run(work_group_x_, work_group_y_, work_group_z_);
        else {
            auto& texture_data = data.get_element<TextureData>(data_work_group_);
            auto& texture = texture_data.get_texture();
            step_->run(texture.width / step_->local_size_x(), texture.height / step_->local_size_y(), 1);
        }
        if (memory_barrier_bits != 0)
            glMemoryBarrier(memory_barrier_bits);

        for (const auto &output : outputs_)
        {
            auto &data_element = data.get_element(output.name);
            data_element.update();
        }
    }
}