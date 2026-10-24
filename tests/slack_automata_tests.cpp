#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "controllers/mutation_control_NN.hpp"
#include "controllers/init_texture_control.hpp"
#include "data.hpp"
#include "engines/compute_shader_engine.hpp"
#include "imgui.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

bool InitOpenGLForTesting(GLFWwindow** window, std::string& error);

namespace {
constexpr int width = 64;
constexpr int height = 32;
constexpr const char* ca_shader = "slack_automata/slack_automata.fs";
constexpr const char* init_shader = "slack_automata/init.fs";

class SlackTestContext {
public:
    SlackTestContext() {
        std::string error;
        if (!InitOpenGLForTesting(&window_, error))
            SKIP("OpenGL unavailable: " << error);
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(width, height);
        io.DeltaTime = 1.0f / 60.0f;
        io.MouseDown[0] = io.MouseDown[1] = false;
    }
    ~SlackTestContext() {
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
private:
    GLFWwindow* window_ = nullptr;
};

std::vector<float> initial_field() {
    std::vector<float> pixels(width * height * 4);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int index = (y * width + x) * 4;
            for (int channel = 0; channel < 3; ++channel)
                pixels[index + channel] =
                    ((x * 73 + y * 151 + channel * 293 + x * y * (channel + 1)) % 1024) / 1023.0f;
            pixels[index + 3] = 1.0f;
        }
    }
    return pixels;
}

TextureData& add_texture(Data& data, const std::string& name, const std::vector<float>& pixels) {
    TextureOptions options;
    options.target = GL_TEXTURE_2D_ARRAY;
    options.internal_format = GL_RGBA32F;
    auto& texture = *data.add_element<TextureData>(name, width, height, 1, options);
    for (auto* target : {&texture.get_texture(), &texture.get_last_texture()})
        glTextureSubImage3D(target->gl_tex_num, 0, 0, 0, 0, width, height, 1,
                            GL_RGBA, GL_FLOAT, pixels.data());
    return texture;
}

std::vector<float> read_texture(TextureData& texture) {
    std::vector<float> pixels(width * height * 4);
    glGetTextureImage(texture.get_texture().gl_tex_num, 0, GL_RGBA, GL_FLOAT,
                      static_cast<GLsizei>(pixels.size() * sizeof(float)), pixels.data());
    return pixels;
}

void require_valid_pixels(const std::vector<float>& pixels) {
    REQUIRE(pixels.size() == width * height * 4);
    for (size_t i = 0; i < pixels.size(); ++i) {
        CAPTURE(i);
        REQUIRE(std::isfinite(pixels[i]));
        REQUIRE((pixels[i] >= 0.0f && pixels[i] <= 1.0f));
        if (i % 4 == 3)
            REQUIRE(pixels[i] == 1.0f);
    }
}
}

TEST_CASE("Slack single-pass CA preserves legacy raw evolution", "[slack][opengl]") {
    SlackTestContext context;
    Data data;
    const auto field = initial_field();
    auto& legacy_texture = add_texture(data, "legacy texture", field);
    auto& current_texture = add_texture(data, "current texture", field);
    auto& legacy = data.add_element<FragmentProgramData>("legacy", "slack_automata copy 2.fs")->get_program();
    auto& current = data.add_element<FragmentProgramData>("current", ca_shader)->get_program();
    REQUIRE(legacy.is_linked());
    REQUIRE(current.is_linked());

    const auto options = current.get_options();
    const auto radius = std::find_if(options.begin(), options.end(), [](GLSL::IOption* option) {
        return option->name == "MAX_RADIUS";
    });
    REQUIRE(radius != options.end());
    REQUIRE((*radius)->type == GLSL::IOption::Type::UInt);
    CHECK(static_cast<GLSL::ValueOption<uint32_t>*>(*radius)->value == 12u);

    MutationControlNN legacy_control("legacy");
    MutationControlNN current_control("current");
    // The second upload disables reset and leaves frames at 1 for every step.
    for (int upload = 0; upload < 2; ++upload) {
        legacy_control.update(data);
        current_control.update(data);
    }
    REQUIRE(glGetError() == GL_NO_ERROR);

    FragmentShaderStep legacy_step("legacy", {}, "legacy texture", 1);
    FragmentShaderStep current_step("current", {}, "current texture", 1);
    for (int step = 0; step < 8; ++step) {
        CAPTURE(step);
        legacy_step.do_step(data);
        current_step.do_step(data);
        const auto expected = read_texture(legacy_texture);
        const auto actual = read_texture(current_texture);
        require_valid_pixels(expected);
        require_valid_pixels(actual);
        for (size_t i = 0; i < actual.size(); ++i) {
            CAPTURE(i);
            REQUIRE(std::abs(actual[i] - expected[i]) <= 1.0e-6f);
        }
        CHECK(glGetError() == GL_NO_ERROR);
    }
}

TEST_CASE("Slack empty selected neighborhoods produce only finite decay", "[slack][opengl]") {
    const uint32_t mask = GENERATE(0u, 0xf000f000u);
    CAPTURE(mask);
    SlackTestContext context;
    Data data;
    const auto field = initial_field();
    auto& texture = add_texture(data, "state", field);
    auto& program = data.add_element<FragmentProgramData>("ca", ca_shader)->get_program();
    REQUIRE(program.is_linked());
    REQUIRE(program.use());

    std::array<uint32_t, 12> neighborhoods{};
    neighborhoods.fill(mask);
    const std::array<uint32_t, 24> rules{};
    const std::array<uint32_t, 2> signs{};
    const std::array<uint32_t, 3> channels{};
    program.set_uniform("nb", 12, neighborhoods.data());
    program.set_uniform("ur", 24, rules.data());
    program.set_uniform("us", 2, signs.data());
    program.set_uniform("ch", 3, channels.data());
    program.set_uniform("ch2", 3, channels.data());
    program.set_uniform("ch3", 3, channels.data());
    program.set_uniform("mode", uint32_t{0});
    program.set_uniform("zoom", 0.0f);
    // Zero thresholds would activate a mistaken zero-valued empty-ring fallback.
    program.set_uniform("scale", 0.0f);

    FragmentShaderStep step("ca", {}, "state", 1);
    step.do_step(data);
    const auto actual = read_texture(texture);
    require_valid_pixels(actual);
    constexpr float pi = 3.14159265359f;
    constexpr float decay = (1.0f / 65536.0f) * 48.0f * 2.0f;
    for (size_t i = 0; i < actual.size(); ++i) {
        if (i % 4 == 3)
            continue;
        CAPTURE(i);
        const float value = field[i];
        const float sigmoid = 1.0f / (1.0f + std::exp(-value * (pi / 2.0f) + 0.5f * (pi / 2.0f)));
        const float expected = std::clamp(value - (sigmoid * decay * 64.0f + decay), 0.0f, 1.0f);
        REQUIRE(std::abs(actual[i] - expected) <= 1.0e-6f);
        REQUIRE(actual[i] <= value);
    }
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Slack mutation and initialization distinguish text editing from keyboard capture", "[slack][opengl]") {
    SlackTestContext context;
    ImGui::GetIO().Fonts->Build();
    Data data;
    const std::vector<float> marker(width * height * 4, 0.25f);
    auto& state = add_texture(data, "state", marker);
    auto& program = data.add_element<FragmentProgramData>("ca", ca_shader)->get_program();
    auto& initializer = data.add_element<FragmentProgramData>("init", init_shader)->get_program();
    REQUIRE(program.is_linked());
    REQUIRE(initializer.is_linked());
    MutationControlNN mutation("ca");
    InitTextureControl initialization("init", "state");

    auto read_rule = [&] {
        std::vector<uint32_t> rule;
        auto append = [&](const char* name, int count) {
            for (int i = 0; i < count; ++i) {
                const auto element = std::string(name) + "[" + std::to_string(i) + "]";
                INFO(element);
                const GLint location = glGetUniformLocation(program.get_id(), element.c_str());
                REQUIRE(location >= 0);
                GLuint value = 0;
                glGetUniformuiv(program.get_id(), location, &value);
                rule.push_back(value);
            }
        };
        append("nb", 12);
        append("ur", 24);
        append("us", 2);
        append("ch", 3);
        append("ch2", 3);
        append("ch3", 3);
        return rule;
    };
    auto write_marker = [&] {
        glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
        for (auto* target : {&state.get_texture(), &state.get_last_texture()})
            glTextureSubImage3D(target->gl_tex_num, 0, 0, 0, 0, width, height, 1,
                                GL_RGBA, GL_FLOAT, marker.data());
    };
    auto frame = [&](int key = 0, bool text_input = false, bool captured = false) {
        auto& io = ImGui::GetIO();
        for (int shortcut : {'S', 'L', 'V', 'Q'})
            io.KeysDown[shortcut] = shortcut == key;
        ImGui::NewFrame();
        io.WantCaptureKeyboard = text_input || captured;
        io.WantTextInput = text_input;
        mutation.draw();
        initialization.draw();
        mutation.update(data);
        initialization.update(data);
        ImGui::EndFrame();
    };
    auto unchanged = [&](const std::vector<uint32_t>& rule) {
        CHECK(read_rule() == rule);
        CHECK(read_texture(state) == marker);
    };
    auto mutate = [&](const std::vector<uint32_t>& before, bool captured = false) {
        auto after = before;
        // Mutation is stochastic: a single press is allowed to leave the rule unchanged.
        for (int attempt = 0; attempt < 8 && after == before; ++attempt) {
            CAPTURE(attempt);
            frame(0, false, captured);
            write_marker();
            frame('V', false, captured);
            REQUIRE(read_texture(state) != marker);
            after = read_rule();
        }
        REQUIRE(after != before);
        return after;
    };

    frame();
    const auto original = read_rule();
    write_marker();
    frame('S', false, true);
    unchanged(original);
    frame('V', true);
    unchanged(original);
    frame('V', false, true);
    unchanged(original);

    // A selected non-editing window may capture keyboard input without blocking V.
    const auto first_mutation = mutate(original, true);
    write_marker();
    const auto& io = ImGui::GetIO();
    REQUIRE(40 * io.DeltaTime > io.KeyRepeatDelay + io.KeyRepeatRate);
    for (int i = 0; i < 40; ++i) {
        CAPTURE(i);
        frame('V', false, true);
        unchanged(first_mutation);
    }
    frame('S', true);
    unchanged(first_mutation);
    const auto second_mutation = mutate(first_mutation);
    write_marker();
    frame('L', true);
    unchanged(second_mutation);
    frame();
    frame('L', false, true);
    // S during text editing must not have replaced the original saved rule.
    unchanged(original);

    frame('Q', true);
    unchanged(original);
    frame();
    frame('Q', false, true);
    CHECK(read_rule() == original);
    CHECK(read_texture(state) != marker);
    frame();
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Slack initialization is single-output and seed deterministic", "[slack][opengl]") {
    SlackTestContext context;
    Data data;
    auto& texture = add_texture(data, "state", std::vector<float>(width * height * 4, 0.0f));
    auto& program = data.add_element<FragmentProgramData>("init", init_shader)->get_program();
    REQUIRE(program.is_linked());
    GLint output_count = 0;
    glGetProgramInterfaceiv(program.get_id(), GL_PROGRAM_OUTPUT, GL_ACTIVE_RESOURCES, &output_count);
    REQUIRE(output_count == 1);
    const GLenum property = GL_LOCATION;
    GLint location = -1;
    glGetProgramResourceiv(program.get_id(), GL_PROGRAM_OUTPUT, 0, 1, &property, 1, nullptr, &location);
    CHECK(location == 0);

    const auto options = program.get_options();
    const auto seed = std::find_if(options.begin(), options.end(), [](GLSL::IOption* option) {
        return option->name == "seed";
    });
    REQUIRE(seed != options.end());
    REQUIRE((*seed)->type == GLSL::IOption::Type::UInt);
    CHECK(static_cast<GLSL::ValueOption<uint32_t>*>(*seed)->value == 93u);

    FragmentShaderStep step("init", {}, "state", 1);
    const auto initialize = [&](uint32_t rand_seed) {
        REQUIRE(program.use());
        program.set_uniform("rand_seed", rand_seed);
        step.do_step(data);
        auto pixels = read_texture(texture);
        require_valid_pixels(pixels);
        return pixels;
    };
    const auto first = initialize(0u);
    const auto repeated = initialize(0u);
    const auto changed = initialize(1u);
    CHECK(first == repeated);
    CHECK(first != changed);
    std::array<bool, 3> nonzero{};
    for (size_t i = 0; i < first.size(); ++i) {
        if (i % 4 != 3 && first[i] > 0.0f)
            nonzero[i % 4] = true;
    }
    CHECK(nonzero[0]);
    CHECK(nonzero[1]);
    CHECK(nonzero[2]);
    CHECK(glGetError() == GL_NO_ERROR);
}
