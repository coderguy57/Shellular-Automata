#include "controllers/init_texture_control.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"
#include "data.hpp"
#include "render/shader.hpp"
#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"

InitTextureControl::InitTextureControl(std::string shader_name, std::string texture_name)
: texture_name_{texture_name}
{
    if(shader_name.compare(shader_name.length() - 3, 3, ".cs") == 0) {
        compute_program_ = std::make_unique<ComputeProgram>(shader_name);
    } else {
        fragment_name_ = shader_name;
    }
    surface_ = std::make_unique<RenderSurface>();
    fbo_ = std::make_unique<Framebuffer>();
}

void InitTextureControl::draw()
{
    if (ImGui::IsKeyPressed('Q'))
    {
        init_ = true;
    }
    if (ImGui::IsKeyPressed('V'))
    {
        init_ = true;
    }
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
        auto& texture_data = data.get_element<TextureData>(texture_name_);
        auto& last_texture = texture_data.get_last_texture();
        if (compute_program_) {
            compute_program_->use();
            GLuint memory_barrier_bits = 0;
            texture_data.bind(0);
            memory_barrier_bits |= texture_data.memory_barrier_bits();

            compute_program_->run(last_texture.width / compute_program_->local_size_x(), last_texture.height / compute_program_->local_size_y(), 1);
            if (memory_barrier_bits != 0)
                glMemoryBarrier(memory_barrier_bits);
        } else {
            auto& program_data = data.get_element<FragmentProgramData>(fragment_name_);
            auto& program = program_data.get_program();

            glViewport(0, 0, last_texture.width, last_texture.height);
            fbo_->use();
            program.use();
            std::vector<GLenum> buffers;
            for (size_t i = 0; i < last_texture.depth; i++)
            {
                buffers.push_back(GL_COLOR_ATTACHMENT0 + i);
            }

            glDrawBuffers(last_texture.depth, &buffers[0]);
            fbo_->set_texture(0, &last_texture);

            program.set_uniform("rand_seed", seed_ % (1 << 16));
            program.set_uniform("texture_width", (float)last_texture.width);
            program.set_uniform("texture_height", (float)last_texture.height);
            seed_++;

            surface_->draw();
            glFinish();

            texture_data.update();
        }
        init_ = false;
    }
}