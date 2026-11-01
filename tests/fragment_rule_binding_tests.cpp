#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "automata/slack/rule_model.hpp"
#include "automata/slack/rule.hpp"
#include "data.hpp"
#include "slack_fragment_reference.hpp"
#include "engines/compute_shader_engine.hpp"
#include "engines/fragment_shader_engine.hpp"
#include "render/detail/fullscreen_pass.hpp"
#include "render/framebuffer.hpp"
#include "render/renderSurface.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

bool InitOpenGLForTesting(GLFWwindow** window, std::string& error);

namespace {
constexpr int width = 32;
constexpr int height = 16;
constexpr const char* shader = "slack_automata/slack_automata.fs";

class FragmentBindingContext {
public:
    FragmentBindingContext() {
        std::string error;
        if (!InitOpenGLForTesting(&window_, error))
            SKIP("OpenGL unavailable: " << error);
    }

    ~FragmentBindingContext() {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }

private:
    GLFWwindow* window_ = nullptr;
};

TextureData& add_field(Data& data, const std::string& name) {
    TextureOptions options;
    options.target = GL_TEXTURE_2D_ARRAY;
    options.internal_format = GL_RGBA32F;
    auto& field = *data.add_element<TextureData>(name, width, height, 1, options);
    std::vector<float> pixels(width * height * 4);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        for (int channel = 0; channel < 3; ++channel)
            pixels[pixel * 4 + channel] = 0.2f + ((pixel * 73 + channel * 151) % 1024) / 1705.0f;
        pixels[pixel * 4 + 3] = 1.0f;
    }
    for (auto* target : {&field.get_texture(), &field.get_last_texture()})
        glTextureSubImage3D(target->gl_tex_num, 0, 0, 0, 0, width, height, 1,
                            GL_RGBA, GL_FLOAT, pixels.data());
    return field;
}

std::vector<float> read_field(TextureData& field) {
    std::vector<float> pixels(width * height * 4);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    glGetTextureImage(field.get_texture().gl_tex_num, 0, GL_RGBA, GL_FLOAT,
                      static_cast<GLsizei>(pixels.size() * sizeof(float)), pixels.data());
    return pixels;
}

void require_same_field(TextureData& actual, TextureData& reference) {
    const auto actual_pixels = read_field(actual);
    const auto expected_pixels = read_field(reference);
    float maximum_error = 0.0f;
    for (std::size_t i = 0; i < actual_pixels.size(); ++i) {
        REQUIRE(std::isfinite(actual_pixels[i]));
        maximum_error = std::max(maximum_error, std::abs(actual_pixels[i] - expected_pixels[i]));
    }
    REQUIRE(maximum_error <= 1.0e-6f);
}

slack::Rule active_rule(bool increase) {
    auto rule = slack::baseline_rule();
    std::fill_n(rule.words.begin(), 12, 0x00010001u);
    std::fill_n(rule.words.begin() + 12, 24, 0xff00ff00u);
    std::fill_n(rule.words.begin() + 36, 2, increase ? UINT32_MAX : 0u);
    std::fill_n(rule.words.begin() + 38, 9, 0u);
    rule.scale = 256.0f;
    rule.zoom = 0.0f;
    rule.mode = 0;
    return rule;
}

void manual_pass(FragmentProgram& program, TextureData& field, const slack::Rule& rule) {
    Framebuffer framebuffer;
    RenderSurface surface;
    REQUIRE(program.use());
    framebuffer.use();
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    framebuffer.set_texture(0, field.get_last_texture());
    REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    render_detail::setup_fullscreen_pass(width, height);
    program.set_texture(0, "tex", &field.get_texture());
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_FRAMEBUFFER_BARRIER_BIT);
    slack_test::bind_rule(program, rule);
    surface.draw();
    field.update();
}
}

TEST_CASE("Fragment preparation binds the latest rule before every stage without UI updates",
          "[fragment-rule-binding][opengl]") {
    const bool whole_engine = GENERATE(false, true);
    CAPTURE(whole_engine);
    FragmentBindingContext context;
    Data data;
    auto& actual = add_field(data, "actual");
    auto& reference = add_field(data, "reference");
    auto& program = data.add_element<FragmentProgramData>("ca", shader)->get_program();
    FragmentProgram reference_program("basic.vs", shader);
    REQUIRE(program.is_linked());
    REQUIRE(reference_program.is_linked());
    slack::RuleModel state;
    state.rule() = active_rule(false);
    constexpr int stages = 3;
    int preparations = 0;
    auto prepare = [&](FragmentProgram& active) {
        GLint current = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &current);
        CHECK(&active == &program);
        CHECK(static_cast<GLuint>(current) == program.get_id());
        REQUIRE(active.is_linked());
        slack_test::bind_rule(active, state.rule());
        state.dirty = false;
        ++preparations;
    };
    FragmentShaderStep step("ca", {}, "actual", stages, prepare);
    FragmentShaderEngine engine("ca", "actual", stages, prepare);

    for (bool increase : {false, true}) {
        CAPTURE(increase);
        state.rule() = active_rule(increase);
        state.dirty = true;
        REQUIRE(program.use());
        slack_test::bind_rule(program, active_rule(!increase));
        REQUIRE(reference_program.use());
        const auto before = read_field(actual);
        const int previous_preparations = preparations;
        if (whole_engine)
            engine.step(data);
        else
            step.do_step(data);
        CHECK(preparations == previous_preparations + stages);
        CHECK_FALSE(state.dirty);
        for (int stage = 0; stage < stages; ++stage)
            manual_pass(reference_program, reference, state.rule());
        require_same_field(actual, reference);
        CHECK(read_field(actual) != before);
        CHECK(glGetError() == GL_NO_ERROR);
    }
}

TEST_CASE("Fragment preparation remains optional for manually bound rules",
          "[fragment-rule-binding][opengl]") {
    const bool whole_engine = GENERATE(false, true);
    CAPTURE(whole_engine);
    FragmentBindingContext context;
    Data data;
    auto& actual = add_field(data, "actual");
    auto& reference = add_field(data, "reference");
    auto& program = data.add_element<FragmentProgramData>("ca", shader)->get_program();
    FragmentProgram reference_program("basic.vs", shader);
    REQUIRE(program.is_linked());
    REQUIRE(reference_program.is_linked());
    const auto rule = active_rule(true);
    REQUIRE(program.use());
    slack_test::bind_rule(program, rule);
    glUseProgram(0);
    FragmentShaderStep step("ca", {}, "actual", 1);
    FragmentShaderEngine engine("ca", "actual", 1);
    if (whole_engine)
        engine.step(data);
    else
        step.do_step(data);
    manual_pass(reference_program, reference, rule);
    require_same_field(actual, reference);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Fragment preparation is skipped when the program cannot be used",
          "[fragment-rule-binding][opengl]") {
    const bool whole_engine = GENERATE(false, true);
    CAPTURE(whole_engine);
    FragmentBindingContext context;
    Data data;
    auto& field = add_field(data, "state");
    auto& program = data.add_element<FragmentProgramData>(
        "invalid", "fragment_rule_binding_missing_shader.fs")->get_program();
    REQUIRE_FALSE(program.is_linked());
    const auto before = read_field(field);
    const auto input_id = field.get_texture().gl_tex_num;
    const auto output_id = field.get_last_texture().gl_tex_num;
    int preparations = 0;
    auto prepare = [&](FragmentProgram&) { ++preparations; };
    FragmentShaderStep step("invalid", {}, "state", 3, prepare);
    FragmentShaderEngine engine("invalid", "state", 3, prepare);
    if (whole_engine)
        engine.step(data);
    else
        step.do_step(data);
    CHECK(preparations == 0);
    CHECK(field.get_texture().gl_tex_num == input_id);
    CHECK(field.get_last_texture().gl_tex_num == output_id);
    CHECK(read_field(field) == before);
    CHECK(glGetError() == GL_NO_ERROR);
}
