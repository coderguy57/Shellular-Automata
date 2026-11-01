#pragma once

#include "automata/slack/rule.hpp"
#include "data.hpp"
#include "engines/step.hpp"
#include "render/detail/fullscreen_pass.hpp"
#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"
#include "render/shader.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <utility>

namespace slack_test {
inline void bind_rule(FragmentProgram& program, const slack::Rule& rule) {
    program.set_uniform("nb", 12, rule.words.data());
    program.set_uniform("ur", 24, rule.words.data() + 12);
    program.set_uniform("us", 2, rule.words.data() + 36);
    program.set_uniform("ch", 3, rule.words.data() + 38);
    program.set_uniform("ch2", 3, rule.words.data() + 41);
    program.set_uniform("ch3", 3, rule.words.data() + 44);
    program.set_uniform("mode", static_cast<std::uint32_t>(rule.mode));
    program.set_uniform("zoom", rule.zoom);
    program.set_uniform("scale", rule.scale);
}

// Fragment oracle for verifying the compute implementation, not a simulation backend.
class FragmentReference final : public Step {
public:
    FragmentReference(FragmentProgram& program, std::string texture_name,
                      std::function<void(FragmentProgram&, std::size_t)> prepare)
        : program_(program), texture_name_(std::move(texture_name)), prepare_(std::move(prepare)) {}

    void do_step(Data& data) override {
        if (!program_.use())
            return;
        auto& state = data.get_element<TextureData>(texture_name_);
        const auto& source = state.get_texture();
        const auto& target = state.get_last_texture();
        framebuffer_.use();
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        render_detail::setup_fullscreen_pass(target.width, target.height);
        program_.set_texture(0, "tex", &source);
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
        for (std::size_t layer = 0; layer < target.depth; ++layer) {
            glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      target.gl_tex_num, 0, static_cast<GLint>(layer));
            program_.set_uniform("layer", static_cast<int>(layer));
            prepare_(program_, layer);
            surface_.draw();
        }
        state.update();
    }

private:
    FragmentProgram& program_;
    std::string texture_name_;
    std::function<void(FragmentProgram&, std::size_t)> prepare_;
    Framebuffer framebuffer_;
    RenderSurface surface_;
};
}
