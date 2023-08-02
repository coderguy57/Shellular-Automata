#include "controllers/init_texture_control.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"
#include "data.hpp"
#include "render/shader.hpp"

InitTextureControl::InitTextureControl(std::string shader_name, std::string texture_name)
: texture_name_{texture_name}
{
    program_ = std::make_unique<ComputeProgram>(shader_name);
}

void InitTextureControl::draw()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Texture init"))
        {
            if (ImGui::MenuItem("Init", "..."))
            {
                init_ = true;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();

        ImGui::EndMainMenuBar();
    }
}

void InitTextureControl::update(Data& data)
{
    if (init_) {
        program_->use();
        GLuint memory_barrier_bits = 0;
        auto& data_element = data.get_element<TextureData>(texture_name_);
        auto& texture = data_element.get_texture();
        data_element.bind(0);
        memory_barrier_bits |= data_element.memory_barrier_bits();

        program_->run(texture.width / program_->local_size_x(), texture.height / program_->local_size_y(), 1);
        if (memory_barrier_bits != 0)
            glMemoryBarrier(memory_barrier_bits);
        init_ = false;
    }
}