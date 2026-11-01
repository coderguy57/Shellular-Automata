#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "automata/slack/rule.hpp"
#include "automata/slack/rule_model.hpp"
#include "data.hpp"
#include "engines/compute_shader_engine.hpp"
#include "render/glsl_transpiler.hpp"
#include "render/shader.hpp"
#include "slack_fragment_reference.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

bool InitOpenGLForTesting(GLFWwindow** window, std::string& error);

namespace {
class ComputeTestContext {
public:
    ComputeTestContext() {
        std::string error;
        if (!InitOpenGLForTesting(&window_, error))
            SKIP("OpenGL unavailable: " << error);
    }
    ~ComputeTestContext() {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
private:
    GLFWwindow* window_ = nullptr;
};

constexpr int width = 19;
constexpr int height = 13;
constexpr int depth = 3;

std::vector<float> initial_pixels() {
    std::vector<float> pixels(width * height * depth * 4);
    for (int i = 0; i < static_cast<int>(pixels.size()); ++i)
        pixels[i] = i % 4 == 3 ? 1.0f : ((i * 73 + i / 23) % 1024) / 1023.0f;
    return pixels;
}

TextureData& add_state(Data& data, const std::string& name, const std::vector<float>& pixels) {
    TextureOptions options;
    options.target = GL_TEXTURE_2D_ARRAY;
    options.internal_format = GL_RGBA32F;
    auto& state = *data.add_element<TextureData>(name, width, height, depth, options);
    for (auto* texture : {&state.get_texture(), &state.get_last_texture()})
        glTextureSubImage3D(texture->gl_tex_num, 0, 0, 0, 0, width, height, depth,
                            GL_RGBA, GL_FLOAT, pixels.data());
    return state;
}

std::vector<float> read_pixels(TextureData& state) {
    std::vector<float> pixels(width * height * depth * 4);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    glGetTextureImage(state.get_texture().gl_tex_num, 0, GL_RGBA, GL_FLOAT,
                      static_cast<GLsizei>(pixels.size() * sizeof(float)), pixels.data());
    return pixels;
}

void set_radius(const std::vector<GLSL::IOption*>& options, uint32_t radius) {
    const auto it = std::find_if(options.begin(), options.end(), [](auto* option) {
        return option->name == "MAX_RADIUS";
    });
    REQUIRE(it != options.end());
    REQUIRE((*it)->type == GLSL::IOption::Type::UInt);
    auto* option = static_cast<GLSL::ValueOption<uint32_t>*>(*it);
    option->value = radius;
    option->changed = true;
}

void dispatch(ComputeProgram& program, TextureData& state, slack::RuleModel& rules,
              int base_layer = 0) {
    REQUIRE(program.use());
    rules.bind(4);
    program.set_uniform("layer", base_layer);
    glBindImageTexture(0, state.get_texture().gl_tex_num, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(2, state.get_last_texture().gl_tex_num, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    // Round up XY and deliberately overdispatch Z to exercise all bounds checks.
    program.run((width + 15) / 16, (height + 15) / 16, depth + 1);
    state.update();
}

GLuint bound_rule_buffer() {
    GLint buffer = 0;
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 4, &buffer);
    REQUIRE(buffer != 0);
    return static_cast<GLuint>(buffer);
}

std::vector<uint32_t> read_rule_buffer(GLuint buffer) {
    GLint64 size = 0;
    glGetNamedBufferParameteri64v(buffer, GL_BUFFER_SIZE, &size);
    REQUIRE(size > 0);
    REQUIRE(size % sizeof(uint32_t) == 0);
    std::vector<uint32_t> words(size / sizeof(uint32_t));
    glGetNamedBufferSubData(buffer, 0, size, words.data());
    return words;
}

void require_uploaded_rule(const std::vector<uint32_t>& words, size_t layer, const slack::Rule& rule) {
    constexpr size_t stride = 50;
    const auto offset = layer * stride;
    REQUIRE(words.size() >= offset + stride);
    CHECK(std::equal(rule.words.begin(), rule.words.begin() + 47, words.begin() + offset));
    CHECK(words[offset + 47] == static_cast<uint32_t>(rule.mode));
    CHECK(words[offset + 48] == std::bit_cast<uint32_t>(rule.zoom));
    CHECK(words[offset + 49] == std::bit_cast<uint32_t>(rule.scale));
}
}

TEST_CASE("Slack compute matches fragment evolution", "[slack][slack-compute][opengl]") {
    const int mode = GENERATE(0, 1, 2);
    const int scenario = GENERATE(0, 1, 2);
    CAPTURE(mode, scenario);
    ComputeTestContext context;
    Data data;
    auto& expected_state = add_state(data, "fragment", initial_pixels());
    auto& actual_state = add_state(data, "compute", initial_pixels());
    FragmentProgram fragment("basic.vs", "slack_automata/slack_automata.fs");
    ComputeProgram compute("slack_automata/slack_automata.cs");
    REQUIRE(fragment.is_linked());
    REQUIRE(compute.is_linked());

    std::mt19937 random(427);
    auto rule = scenario == 1 ? slack::random_rule(random) : slack::baseline_rule();
    rule.mode = mode;
    rule.scale = 256.0f;
    rule.zoom = 0.2f;
    const uint32_t radius = scenario == 0 ? 12u : scenario == 1 ? 16u : 2u;
    if (scenario == 2)
        std::fill_n(rule.words.begin(), 12, 0u);
    set_radius(fragment.get_options(), radius);
    set_radius(compute.get_options(), radius);

    slack::RuleModel rules(depth);
    for (std::size_t layer = 0; layer < rules.size(); ++layer)
        rules.rule(layer) = rule;
    rules.upload();
    slack_test::FragmentReference fragment_step(fragment, "fragment",
        [&](FragmentProgram& program, std::size_t) { slack_test::bind_rule(program, rule); });
    for (int step = 0; step < 4; ++step) {
        CAPTURE(step);
        fragment_step.do_step(data);
        dispatch(compute, actual_state, rules);
        const auto expected = read_pixels(expected_state);
        const auto actual = read_pixels(actual_state);
        float max_error = 0.0f;
        for (std::size_t i = 0; i < actual.size(); ++i) {
            REQUIRE(std::isfinite(actual[i]));
            max_error = std::max(max_error, std::abs(actual[i] - expected[i]));
        }
        REQUIRE(max_error <= 1.0e-6f);
        REQUIRE(glGetError() == GL_NO_ERROR);
    }
}

TEST_CASE("Slack compute base layer leaves earlier layers untouched", "[slack][slack-compute][opengl]") {
    ComputeTestContext context;
    Data data;
    const auto initial = initial_pixels();
    auto& state = add_state(data, "compute", initial);
    ComputeProgram compute("slack_automata/slack_automata.cs");
    REQUIRE(compute.is_linked());
    auto rule = slack::baseline_rule();
    std::fill_n(rule.words.begin(), 12, 0u);
    slack::RuleModel rules(depth);
    for (std::size_t layer = 0; layer < rules.size(); ++layer)
        rules.rule(layer) = rule;
    rules.upload();
    dispatch(compute, state, rules, 1);
    const auto actual = read_pixels(state);
    const int layer_size = width * height * 4;
    REQUIRE(std::equal(actual.begin(), actual.begin() + layer_size, initial.begin()));
    REQUIRE_FALSE(std::equal(actual.begin() + layer_size, actual.end(), initial.begin() + layer_size));
    REQUIRE(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Slack GPU rules pack contiguous scalar records", "[slack][slack-compute][rules][opengl]") {
    ComputeTestContext context;
    std::mt19937 random(5819u);
    std::array<slack::Rule, 2> rules{slack::random_rule(random), slack::random_rule(random)};
    rules[0].mode = 1;
    rules[0].zoom = 0.25f;
    rules[0].scale = 57.0f;
    rules[1].mode = 2;
    rules[1].zoom = -0.125f;
    rules[1].scale = 113.5f;
    slack::RuleModel gpu_rules(rules.size());
    for (std::size_t layer = 0; layer < rules.size(); ++layer)
        gpu_rules.rule(layer) = rules[layer];
    gpu_rules.upload();
    gpu_rules.bind(4);
    const auto words = read_rule_buffer(bound_rule_buffer());
    REQUIRE(words.size() == rules.size() * 50);
    for (size_t layer = 0; layer < rules.size(); ++layer) {
        CAPTURE(layer);
        require_uploaded_rule(words, layer, rules[layer]);
    }
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Slack rule models initialize clean buffers and upload dirty edits lazily", "[slack][slack-compute][rules][opengl]") {
    ComputeTestContext context;
    slack::RuleModel gpu_rules;
    gpu_rules.dirty = false;
    slack::RuleModel independent;
    independent.dirty = false;
    gpu_rules.bind(4);
    const auto buffer = bound_rule_buffer();
    const auto baseline = read_rule_buffer(buffer);
    REQUIRE(baseline.size() == 50);
    require_uploaded_rule(baseline, 0, gpu_rules.rule());
    CHECK_FALSE(gpu_rules.dirty);
    independent.bind(4);
    CHECK(bound_rule_buffer() != buffer);
    CHECK(read_rule_buffer(bound_rule_buffer()) == baseline);
    CHECK_FALSE(gpu_rules.dirty);

    gpu_rules.reseed(318u);
    gpu_rules.randomize();
    CHECK(gpu_rules.dirty);
    CHECK(read_rule_buffer(buffer) == baseline);
    gpu_rules.bind(4);
    CHECK(bound_rule_buffer() == buffer);
    CHECK_FALSE(gpu_rules.dirty);
    const auto edited = read_rule_buffer(buffer);
    REQUIRE(edited != baseline);
    require_uploaded_rule(edited, 0, gpu_rules.rule());

    // A clean bind must not silently rewrite the GPU buffer.
    const uint32_t sentinel = std::bit_cast<uint32_t>(42.5f);
    glNamedBufferSubData(buffer, 49 * sizeof(uint32_t), sizeof(sentinel), &sentinel);
    gpu_rules.bind(4);
    auto unchanged = edited;
    unchanged[49] = sentinel;
    CHECK(read_rule_buffer(buffer) == unchanged);
    gpu_rules.restore();
    CHECK(gpu_rules.dirty);
    CHECK(read_rule_buffer(buffer) == unchanged);
    gpu_rules.bind(4);
    CHECK_FALSE(gpu_rules.dirty);
    CHECK(read_rule_buffer(buffer) == baseline);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Slack compute leaves layers without a rule untouched", "[slack][slack-compute][opengl]") {
    ComputeTestContext context;
    Data data;
    const auto initial = initial_pixels();
    auto& state = add_state(data, "compute", initial);
    ComputeProgram compute("slack_automata/slack_automata.cs");
    REQUIRE(compute.is_linked());
    auto rule = slack::baseline_rule();
    std::fill_n(rule.words.begin(), 12, 0u);
    const std::array<slack::Rule, 1> available_rules{rule};
    slack::RuleModel rules(available_rules.size());
    rules.rule() = available_rules[0];
    rules.upload();
    dispatch(compute, state, rules);
    const auto actual = read_pixels(state);
    constexpr int layer_size = width * height * 4;
    CHECK_FALSE(std::equal(actual.begin(), actual.begin() + layer_size, initial.begin()));
    CHECK(std::equal(actual.begin() + layer_size, actual.end(), initial.begin() + layer_size));
    CHECK(glGetError() == GL_NO_ERROR);
}
