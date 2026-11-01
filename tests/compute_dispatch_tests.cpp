#include <catch2/catch_test_macros.hpp>

#include "data.hpp"
#include "engines/compute_shader_engine.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

bool InitOpenGLForTesting(GLFWwindow** window, std::string& error);

namespace {
class DispatchTestContext {
public:
    DispatchTestContext() {
        std::string error;
        if (!InitOpenGLForTesting(&window_, error))
            SKIP("OpenGL unavailable: " << error);
    }

    ~DispatchTestContext() {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }

private:
    GLFWwindow* window_ = nullptr;
};

class DispatchRuleData final : public DataElement {
public:
    explicit DispatchRuleData(const std::array<float, 3>& bias) {
        std::array<float, 1411> coefficients{};
        std::copy(bias.begin(), bias.end(), coefficients.begin() + 1408);
        glCreateBuffers(1, &buffer_);
        glNamedBufferData(buffer_, sizeof(coefficients), coefficients.data(), GL_STATIC_DRAW);
    }

    ~DispatchRuleData() override { glDeleteBuffers(1, &buffer_); }

    void bind(GLuint layout) override {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, layout, buffer_);
    }

private:
    GLuint buffer_ = 0;
};

std::vector<float> read_image(Texture& image) {
    std::vector<float> pixels(image.width * image.height * image.depth * 4);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    glGetTextureImage(image.gl_tex_num, 0, GL_RGBA, GL_FLOAT,
                      static_cast<GLsizei>(pixels.size() * sizeof(float)), pixels.data());
    return pixels;
}

void fill_images(TextureData& state, const std::array<float, 4>& color) {
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    for (auto* image : {&state.get_texture(), &state.get_last_texture()})
        glClearTexImage(image->gl_tex_num, 0, GL_RGBA, GL_FLOAT, color.data());
}

std::vector<float> advance(std::vector<float> pixels, const std::array<float, 3>& bias,
                           float timestep, bool modulation, float offset, int iterations = 1) {
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
            for (int channel = 0; channel < 3; ++channel) {
                float& value = pixels[pixel + channel];
                const float centered = value - 0.5f;
                const float amplitude = modulation ? 3.0f * centered * centered + offset : 1.0f;
                value = std::clamp(value + timestep * bias[channel] * amplitude, 0.0f, 1.0f);
            }
            pixels[pixel + 3] = 1.0f;
        }
    }
    return pixels;
}

void require_pixels(const std::vector<float>& actual, const std::vector<float>& expected) {
    REQUIRE(actual.size() == expected.size());
    float error = 0.0f;
    for (size_t index = 0; index < actual.size(); ++index) {
        REQUIRE(std::isfinite(actual[index]));
        error = std::max(error, std::abs(actual[index] - expected[index]));
    }
    REQUIRE(error < 2.0e-5f);
}

GLSL::IOption& option(ComputeProgram& program, const std::string& name) {
    const auto options = program.get_options();
    const auto found = std::find_if(options.begin(), options.end(), [&](const auto* value) {
        return value->name == name;
    });
    INFO(name);
    REQUIRE(found != options.end());
    return **found;
}
}

TEST_CASE("Compute dispatch steps share program options and explicitly bind their own rules", "[compute-dispatch][opengl]") {
    DispatchTestContext context;
    Data data;
    auto& first_state = *data.add_element<TextureData>("first", 16, 16, 1);
    auto& second_state = *data.add_element<TextureData>("second", 32, 16, 1);
    const std::array<float, 3> first_bias{0.03f, -0.05f, 0.07f};
    const std::array<float, 3> second_bias{-0.08f, 0.04f, -0.02f};
    auto& first_rule = *data.add_element<DispatchRuleData>("first rule", first_bias);
    auto& second_rule = *data.add_element<DispatchRuleData>("second rule", second_bias);
    fill_images(first_state, {0.2f, 0.4f, 0.7f, 0.25f});
    fill_images(second_state, {0.65f, 0.3f, 0.45f, 0.5f});
    ComputeProgram shared("neural_mnca/ca.cs");
    REQUIRE(shared.is_linked());
    const GLuint program_id = shared.get_id();
    {
        ComputeShaderStep first(shared, "first dispatch",
            {{0, "first"}, {4, "first rule"}}, {{2, "first"}});
        ComputeShaderStep second(shared, "second dispatch",
            {{0, "second"}, {4, "second rule"}}, {{2, "second"}});
        first.set_work_groups(1, 1, 1);
        second.set_data_work_group("second");
        REQUIRE(&first.program() == &shared);
        REQUIRE(&second.program() == &shared);
        CHECK(first.shader_name() == "first dispatch");
        CHECK(second.shader_name() == "second dispatch");
        auto& modulation_option = option(shared, "DO_MODULATION");
        REQUIRE(modulation_option.type == GLSL::IOption::Type::Bool);
        REQUIRE(modulation_option.compile_constant);
        for (bool modulation : {true, false}) {
            CAPTURE(modulation);
            const float timestep = modulation ? 0.2f : 0.35f;
            constexpr float offset = 0.4f;
            // Mutating either step changes the same program, including compile-time options.
            first.set_option("Do modulation", modulation);
            first.set_option("Time step", timestep);
            second.set_option("Modulation", offset);
            second.set_option("Diffusion", 0.0f);
            auto& step_option = option(second.program(), "step_size");
            REQUIRE(step_option.type == GLSL::IOption::Type::Float);
            CHECK(static_cast<GLSL::ValueOption<float>&>(step_option).value == timestep);
            for (int iteration = 0; iteration < 2; ++iteration) {
                CAPTURE(iteration);
                const auto first_input = read_image(first_state.get_texture());
                const auto second_input = read_image(second_state.get_texture());
                second_rule.bind(4);
                first.do_step(data);
                REQUIRE(shared.is_linked());
                CHECK(shared.get_id() == program_id);
                CHECK_FALSE(modulation_option.changed);
                require_pixels(read_image(first_state.get_texture()),
                               advance(first_input, first_bias, timestep, modulation, offset));
                CHECK(read_image(first_state.get_last_texture()) == first_input);
                CHECK(read_image(second_state.get_texture()) == second_input);

                first_rule.bind(4);
                second.do_step(data);
                require_pixels(read_image(second_state.get_texture()),
                               advance(second_input, second_bias, timestep, modulation, offset));
                CHECK(read_image(second_state.get_last_texture()) == second_input);
                require_pixels(read_image(first_state.get_texture()),
                               advance(first_input, first_bias, timestep, modulation, offset));
            }
        }
    }
    // Both dispatchers are gone, but the owner can still use and dispatch its program.
    CHECK(glIsProgram(program_id) == GL_TRUE);
    REQUIRE(shared.use());
    const auto before = read_image(first_state.get_texture());
    {
        ComputeShaderStep after(shared, "after destruction",
            {{0, "first"}, {4, "first rule"}}, {{2, "first"}});
        after.set_work_groups(1, 1, 1);
        second_rule.bind(4);
        after.do_step(data);
    }
    require_pixels(read_image(first_state.get_texture()), advance(before, first_bias, 0.35f, false, 0.4f));
    CHECK(glIsProgram(program_id) == GL_TRUE);
    glUseProgram(0);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Compute dispatch path constructor retains ownership and iteration behavior", "[compute-dispatch][opengl]") {
    DispatchTestContext context;
    Data data;
    auto& state = *data.add_element<TextureData>("state", 16, 16, 1);
    const std::array<float, 3> bias{0.03f, -0.05f, 0.07f};
    data.add_element<DispatchRuleData>("rule", bias);
    fill_images(state, {0.2f, 0.4f, 0.7f, 0.25f});
    const auto initial = read_image(state.get_texture());
    GLuint program_id = 0;
    {
        ComputeShaderStep owned("neural_mnca/ca.cs", {{0, "state"}, {4, "rule"}}, {{2, "state"}});
        REQUIRE(owned.program().is_linked());
        CHECK(owned.shader_name() == "neural_mnca/ca.cs");
        program_id = owned.program().get_id();
        owned.set_work_groups(1, 1, 1)->set_iterations(3);
        owned.set_option("Time step", 0.25f);
        owned.set_option("Diffusion", 0.0f);
        owned.do_step(data);
        require_pixels(read_image(state.get_texture()), advance(initial, bias, 0.25f, true, 0.25f, 3));
        require_pixels(read_image(state.get_last_texture()), advance(initial, bias, 0.25f, true, 0.25f, 2));
        glUseProgram(0);
    }
    CHECK(glIsProgram(program_id) == GL_FALSE);
    CHECK(glGetError() == GL_NO_ERROR);
}
