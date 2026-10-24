#include <catch2/catch_test_macros.hpp>

#include "controllers/neural_mutation_control.hpp"
#include "controllers/init_texture_control.hpp"
#include "data.hpp"
#include "engines/compute_shader_engine.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "setup/setups.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

bool InitOpenGLForTesting(GLFWwindow** window, std::string& error);

namespace {
constexpr int input_count = 39;
constexpr int hidden_count = 32;
constexpr int centers_offset = input_count * hidden_count;
constexpr int widths_offset = centers_offset + hidden_count;
constexpr int output_weights_offset = widths_offset + hidden_count;
constexpr int output_bias_offset = output_weights_offset + 3 * hidden_count;
constexpr int weight_count = output_bias_offset + 3;
using Rule = std::array<float, weight_count>;
using Features = std::array<float, input_count>;

static_assert(centers_offset == 1248);
static_assert(widths_offset == 1280);
static_assert(output_weights_offset == 1312);
static_assert(output_bias_offset == 1408);
static_assert(weight_count == 1411);

class TestRuleBuffer {
public:
    TestRuleBuffer() {
        glCreateBuffers(1, &buffer_);
        glNamedBufferData(buffer_, sizeof(Rule), nullptr, GL_DYNAMIC_DRAW);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, buffer_);
    }

    ~TestRuleBuffer() { glDeleteBuffers(1, &buffer_); }

    void upload(const Rule& rule) {
        glNamedBufferSubData(buffer_, 0, sizeof(rule), rule.data());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, buffer_);
    }

private:
    GLuint buffer_ = 0;
};

class NeuralTestContext {
public:
    NeuralTestContext() : previous_(ImGui::GetCurrentContext()) {
        std::string error;
        if (!InitOpenGLForTesting(&window_, error))
            SKIP("OpenGL unavailable: " << error);
        context_ = ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(512, 512);
        io.DeltaTime = 1.0f / 60.0f;
        io.Fonts->Build();
    }

    ~NeuralTestContext() {
        ImGui::DestroyContext(context_);
        ImGui::SetCurrentContext(previous_);
        glfwDestroyWindow(window_);
        glfwTerminate();
    }

private:
    GLFWwindow* window_ = nullptr;
    ImGuiContext* previous_ = nullptr;
    ImGuiContext* context_ = nullptr;
};

void write_texture(TextureData& texture, const std::vector<float>& pixels) {
    REQUIRE(pixels.size() == texture.width() * texture.height() * texture.depth() * 4);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    for (auto* target : {&texture.get_texture(), &texture.get_last_texture()})
        glTextureSubImage3D(target->gl_tex_num, 0, 0, 0, 0,
                            texture.width(), texture.height(), texture.depth(),
                            GL_RGBA, GL_FLOAT, pixels.data());
}

std::vector<float> read_texture(TextureData& texture) {
    std::vector<float> pixels(texture.width() * texture.height() * texture.depth() * 4);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    glGetTextureImage(texture.get_texture().gl_tex_num, 0, GL_RGBA, GL_FLOAT,
                      static_cast<GLsizei>(pixels.size() * sizeof(float)), pixels.data());
    return pixels;
}

std::vector<float> constant_layer(int width, int height, const std::array<float, 4>& color) {
    std::vector<float> pixels(width * height * 4);
    for (size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = color[i % 4];
    return pixels;
}

void require_close(const std::vector<float>& actual, const std::vector<float>& expected) {
    REQUIRE(actual.size() == expected.size());
    float error = 0.0f;
    for (size_t i = 0; i < actual.size(); ++i) {
        REQUIRE(std::isfinite(actual[i]));
        error = std::max(error, std::abs(actual[i] - expected[i]));
    }
    REQUIRE(error < 2.0e-5f);
}

const std::array<std::vector<std::pair<int, int>>, 12>& ring_offsets() {
    static const auto rings = [] {
        std::array<std::vector<std::pair<int, int>>, 12> result;
        for (int r = 1; r <= 12; ++r) {
            // Integer comparisons independently implement the half-pixel annulus bounds.
            const int inner = (2 * r - 1) * (2 * r - 1);
            const int outer = (2 * r + 1) * (2 * r + 1);
            for (int dy = -12; dy <= 12; ++dy)
                for (int dx = -12; dx <= 12; ++dx) {
                    const int distance = 4 * (dx * dx + dy * dy);
                    if (distance >= inner && distance < outer)
                        result[r - 1].emplace_back(dx, dy);
                }
        }
        return result;
    }();
    return rings;
}

int wrap(int value, int size) {
    return (value % size + size) % size;
}

std::vector<Features> ring_features(const std::vector<float>& pixels, int width, int height) {
    std::vector<Features> result(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            auto& features = result[y * width + x];
            for (int channel = 0; channel < 3; ++channel)
                features[channel] = pixels[(y * width + x) * 4 + channel];
            for (int ring = 0; ring < 12; ++ring) {
                std::array<double, 3> sum{};
                for (const auto& [dx, dy] : ring_offsets()[ring]) {
                    const int source = (wrap(y + dy, height) * width + wrap(x + dx, width)) * 4;
                    for (int channel = 0; channel < 3; ++channel)
                        sum[channel] += pixels[source + channel];
                }
                for (int channel = 0; channel < 3; ++channel)
                    features[3 + ring * 3 + channel] = sum[channel] / ring_offsets()[ring].size();
            }
        }
    }
    return result;
}

std::vector<float> infer(const std::vector<Features>& features, const Rule& rule,
                         float time_step, float diffusion) {
    std::vector<float> result(features.size() * 4, 1.0f);
    for (size_t pixel = 0; pixel < features.size(); ++pixel) {
        const auto& input = features[pixel];
        std::array<float, hidden_count> hidden{};
        for (int h = 0; h < hidden_count; ++h) {
            float density = 0.0f;
            for (int f = 0; f < input_count; ++f)
                density += rule[h * input_count + f] * input[f];
            const float distance = (density - rule[centers_offset + h])
                / std::max(rule[widths_offset + h], 0.005f);
            hidden[h] = std::exp(-distance * distance);
        }
        for (int channel = 0; channel < 3; ++channel) {
            float value = rule[output_bias_offset + channel];
            for (int h = 0; h < hidden_count; ++h)
                value += rule[output_weights_offset + channel * hidden_count + h] * hidden[h];
            const float centered = input[channel] - 0.5f;
            const float amplitude = 3.0f * centered * centered + 0.25f;
            const float rate = value * amplitude
                + diffusion * (input[3 + channel] - input[channel]);
            result[pixel * 4 + channel] = std::clamp(input[channel] + time_step * rate, 0.0f, 1.0f);
        }
    }
    return result;
}

std::vector<float> varied_field(int width, int height) {
    auto field = constant_layer(width, height, {0.0f, 0.0f, 0.0f, 0.25f});
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            for (int channel = 0; channel < 3; ++channel)
                field[(y * width + x) * 4 + channel] =
                    0.04f + ((x * 17 + y * 31 + channel * 23 + x * y * (channel + 1)) % 101) * 0.007f;
    return field;
}

GLint uniform_location(Program& program, const std::string& name) {
    const GLint location = glGetUniformLocation(program.get_id(), name.c_str());
    INFO(name);
    REQUIRE(location >= 0);
    return location;
}

GLuint bound_rule_buffer() {
    GLint buffer = 0;
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 4, &buffer);
    REQUIRE(buffer != 0);
    return static_cast<GLuint>(buffer);
}

std::vector<float> read_weights() {
    const auto buffer = bound_rule_buffer();
    GLint64 size = 0;
    glGetNamedBufferParameteri64v(buffer, GL_BUFFER_SIZE, &size);
    REQUIRE(size >= static_cast<GLint64>(sizeof(Rule)));
    std::vector<float> weights(weight_count);
    glGetNamedBufferSubData(buffer, 0, sizeof(Rule), weights.data());
    return weights;
}

void require_rbf_rule(const std::vector<float>& rule) {
    REQUIRE(rule.size() == weight_count);
    REQUIRE(std::all_of(rule.begin(), rule.end(), [](float value) {
        return std::isfinite(value);
    }));
    for (int neuron = 0; neuron < hidden_count; ++neuron) {
        CAPTURE(neuron);
        float sum = 0.0f;
        for (int feature = 0; feature < input_count; ++feature) {
            const float weight = rule[neuron * input_count + feature];
            REQUIRE(weight >= 0.0f);
            REQUIRE(weight <= 1.0f);
            sum += weight;
        }
        CHECK(std::abs(sum - 1.0f) < 2.0e-6f);
        CHECK(rule[centers_offset + neuron] >= 0.0f);
        CHECK(rule[centers_offset + neuron] <= 1.0f);
        CHECK(rule[widths_offset + neuron] >= 0.005f);
    }
    for (int index = output_weights_offset; index < output_bias_offset; ++index)
        CHECK(std::abs(rule[index]) <= 0.2f);
    for (int index = output_bias_offset; index < weight_count; ++index)
        CHECK(std::abs(rule[index]) <= 0.05f);
}

GLSL::ValueOption<float>& float_option(auto& program, const std::string& name) {
    const auto options = program.get_options();
    const auto found = std::find_if(options.begin(), options.end(), [&](GLSL::IOption* option) {
        return option->name == name;
    });
    INFO(name);
    REQUIRE(found != options.end());
    REQUIRE((*found)->type == GLSL::IOption::Type::Float);
    REQUIRE_FALSE((*found)->compile_constant);
    return *static_cast<GLSL::ValueOption<float>*>(*found);
}

GLSL::ValueOption<uint32_t>& initialization_seed(FragmentProgram& program) {
    const auto options = program.get_options();
    const auto found = std::find_if(options.begin(), options.end(), [](GLSL::IOption* option) {
        return option->name == "seed";
    });
    REQUIRE(found != options.end());
    REQUIRE((*found)->type == GLSL::IOption::Type::UInt);
    REQUIRE_FALSE((*found)->compile_constant);
    return *static_cast<GLSL::ValueOption<uint32_t>*>(*found);
}

std::array<double, 3> rgb_sum(const std::vector<float>& pixels) {
    std::array<double, 3> sum{};
    for (size_t i = 0; i < pixels.size(); i += 4)
        for (int channel = 0; channel < 3; ++channel)
            sum[channel] += pixels[i + channel];
    return sum;
}

void require_bounded_state(const std::vector<float>& pixels) {
    REQUIRE_FALSE(pixels.empty());
    REQUIRE(std::all_of(pixels.begin(), pixels.end(), [](float value) {
        return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
    }));
    bool opaque = true;
    for (size_t i = 0; i < pixels.size(); i += 4)
        opaque = opaque && pixels[i + 3] == 1.0f;
    CHECK(opaque);
}

void require_active_state(const std::vector<float>& pixels) {
    require_bounded_state(pixels);
    std::array<float, 3> low{1.0f, 1.0f, 1.0f};
    std::array<float, 3> high{};
    for (size_t i = 0; i < pixels.size(); i += 4) {
        for (int channel = 0; channel < 3; ++channel) {
            low[channel] = std::min(low[channel], pixels[i + channel]);
            high[channel] = std::max(high[channel], pixels[i + channel]);
        }
    }
    for (int channel = 0; channel < 3; ++channel) {
        CAPTURE(channel);
        CHECK(high[channel] - low[channel] > 1.0e-4f);
    }
}
}

TEST_CASE("Neural MNCA tiled sampling matches every ring and RGB channel", "[neural][opengl]") {
    NeuralTestContext context;
    constexpr std::array<size_t, 12> ring_counts{8, 12, 16, 32, 28, 40, 40, 48, 68, 56, 72, 68};
    for (int ring = 0; ring < 12; ++ring) {
        CAPTURE(ring);
        CHECK(ring_offsets()[ring].size() == ring_counts[ring]);
    }
    auto simulation = setup_neural_mnca();
    auto* engine = dynamic_cast<ComputeShaderEngine*>(&simulation.engine());
    REQUIRE(engine != nullptr);
    REQUIRE(engine->compute_steps().size() == 1);
    auto& step = *engine->compute_steps()[0];
    REQUIRE(step.program().is_linked());
    CHECK(step.shader_name() == "neural_mnca/ca.cs");
    glBindImageTexture(1, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    struct FieldCase { int width; int height; int pattern; };
    for (const auto& [width, height, pattern] : std::array<FieldCase, 4>{
             FieldCase{16, 16, 0}, FieldCase{32, 32, 1},
             FieldCase{35, 19, 2}, FieldCase{7, 5, 2}}) {
        CAPTURE(width, height, pattern);
        // Exercise the actual setup wiring with replacement fields, not a duplicate pipeline.
        Data data;
        auto& state = *data.add_element<TextureData>("data_texture", width, height, 1);
        // Explicit ceil dispatch exercises partial tiles without changing the generic engine.
        step.set_work_groups((width + 15) / 16, (height + 15) / 16, 1);
        float_option(step.program(), "step_size").value = 1.0f;
        float_option(step.program(), "diffusion").value = 0.0f;
        TestRuleBuffer buffer;
        auto field = varied_field(width, height);
        if (pattern == 0)
            field = constant_layer(width, height, {0.17f, 0.43f, 0.79f, 0.25f});
        if (pattern == 1) {
            field = constant_layer(width, height, {0.0f, 0.0f, 0.0f, 0.25f});
            field[0] = 1.0f;
            field[(width - 1) * 4 + 1] = 0.7f;
            field[((height - 1) * width) * 4 + 2] = 0.45f;
            field[((height / 2) * width + width / 2) * 4] = 0.9f;
        }
        const auto features = ring_features(field, width, height);
        // Three independent probes per dispatch cover all 39 features, including cell RGB.
        for (int group = 0; group < 13; ++group) {
            CAPTURE(group);
            Rule rule{};
            for (int channel = 0; channel < 3; ++channel) {
                rule[channel * input_count + group * 3 + channel] = 1.0f;
                rule[centers_offset + channel] = 0.45f;
                rule[widths_offset + channel] = 0.3f;
                rule[output_weights_offset + channel * hidden_count + channel] = 0.2f;
            }
            buffer.upload(rule);
            write_texture(state, field);
            engine->step(data);
            require_close(read_texture(state), infer(features, rule, 1.0f, 0.0f));
            // A fused step must write the opposite image, never overwrite its input in-place.
            std::vector<float> original(field.size());
            glGetTextureImage(state.get_last_texture().gl_tex_num, 0, GL_RGBA, GL_FLOAT,
                              static_cast<GLsizei>(original.size() * sizeof(float)), original.data());
            CHECK(original == field);
        }
    }
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural MNCA fused forward pass matches nonuniform RGB CPU inference", "[neural][opengl]") {
    NeuralTestContext context;
    constexpr int width = 35;
    constexpr int height = 21;
    Data data;
    auto& state = *data.add_element<TextureData>("state", width, height, 1);
    ComputeShaderStep step("neural_mnca/ca.cs", {{0, "state"}}, {{2, "state"}});
    step.set_work_groups((width + 15) / 16, (height + 15) / 16, 1);
    auto& program = step.program();
    REQUIRE(program.is_linked());
    CHECK(program.local_size_x() == 16);
    CHECK(program.local_size_y() == 16);
    CHECK(program.local_size_z() == 1);
    const GLuint block = glGetProgramResourceIndex(program.get_id(), GL_SHADER_STORAGE_BLOCK, "NeuralRule");
    REQUIRE(block != GL_INVALID_INDEX);
    constexpr GLenum binding_property = GL_BUFFER_BINDING;
    GLint binding = -1;
    glGetProgramResourceiv(program.get_id(), GL_SHADER_STORAGE_BLOCK, block,
                           1, &binding_property, 1, nullptr, &binding);
    CHECK(binding == 4);
    struct ArrayLayout { const char* name; int offset; int count; };
    for (const auto& [name, offset, count] : std::array<ArrayLayout, 5>{
             ArrayLayout{"input_weights[0]", 0, centers_offset},
             ArrayLayout{"centers[0]", centers_offset, hidden_count},
             ArrayLayout{"widths[0]", widths_offset, hidden_count},
             ArrayLayout{"output_weights[0]", output_weights_offset, 3 * hidden_count},
             ArrayLayout{"output_bias[0]", output_bias_offset, 3}}) {
        CAPTURE(name);
        const GLuint index = glGetProgramResourceIndex(program.get_id(), GL_BUFFER_VARIABLE, name);
        REQUIRE(index != GL_INVALID_INDEX);
        constexpr std::array<GLenum, 3> properties{GL_OFFSET, GL_ARRAY_SIZE, GL_ARRAY_STRIDE};
        std::array<GLint, 3> values{};
        glGetProgramResourceiv(program.get_id(), GL_BUFFER_VARIABLE, index,
                               properties.size(), properties.data(), values.size(), nullptr, values.data());
        CHECK(values[0] == offset * sizeof(float));
        CHECK(values[1] == count);
        CHECK(values[2] == sizeof(float));
    }

    const auto field = varied_field(width, height);
    const auto features = ring_features(field, width, height);
    Rule rule{};
    for (int h = 0; h < hidden_count; ++h) {
        rule[centers_offset + h] = 0.1f + (h % 9) * 0.1f;
        rule[widths_offset + h] = std::array<float, 4>{0.001f, 0.025f, 0.1f, 0.3f}[h % 4];
        float total = 0.0f;
        for (int f = 0; f < input_count; ++f) {
            const float weight = static_cast<float>(1 + (h * 7 + f * 3) % 17);
            rule[h * input_count + f] = weight;
            total += weight;
        }
        for (int f = 0; f < input_count; ++f)
            rule[h * input_count + f] /= total;
        for (int c = 0; c < 3; ++c)
            rule[output_weights_offset + c * hidden_count + h] = ((c * 5 + h * 7) % 13 - 6) * 0.025f;
    }
    rule[output_bias_offset] = -0.045f;
    rule[output_bias_offset + 1] = 0.007f;
    rule[output_bias_offset + 2] = 0.045f;
    TestRuleBuffer buffer;
    auto& time_step = float_option(program, "step_size");
    auto& diffusion = float_option(program, "diffusion");
    CHECK(time_step.label == "Time step");
    CHECK(std::isfinite(time_step.value));
    CHECK(time_step.value >= time_step.min);
    CHECK(time_step.value <= time_step.max);
    CHECK(time_step.max == 1.0f);
    CHECK(diffusion.label == "Diffusion");
    CHECK(diffusion.value == 0.0f);
    CHECK(std::isfinite(diffusion.value));
    CHECK(diffusion.value >= diffusion.min);
    CHECK(diffusion.value <= diffusion.max);

    for (float timestep : {0.0f, 0.08f, 1.0f}) {
        for (float amount : {0.0f, 0.2f}) {
            CAPTURE(timestep, amount);
            write_texture(state, field);
            buffer.upload(rule);
            time_step.value = timestep;
            diffusion.value = amount;
            step.do_step(data);
            require_close(read_texture(state), infer(features, rule, timestep, amount));
        }
    }

    // A zero-output rule is inert without diffusion, even with zero response widths.
    buffer.upload(Rule{});
    time_step.value = 0.08f;
    diffusion.value = 0.0f;
    write_texture(state, field);
    step.do_step(data);
    require_close(read_texture(state), infer(features, Rule{}, 0.08f, 0.0f));

    // With every network coefficient zero, only the ring-1 diffusion term remains.
    diffusion.value = 0.2f;
    write_texture(state, field);
    step.do_step(data);
    require_close(read_texture(state), infer(features, Rule{}, 0.08f, 0.2f));

    // Both clipping directions must remain correct even at the user's maximum time step.
    rule = {};
    rule[output_bias_offset] = -10.0f;
    rule[output_bias_offset + 2] = 10.0f;
    buffer.upload(rule);
    time_step.value = 1.0f;
    diffusion.value = 0.0f;
    write_texture(state, field);
    step.do_step(data);
    require_close(read_texture(state), infer(features, rule, 1.0f, 0.0f));

    // Repeated ping-pong steps also need correct cross-dispatch image visibility.
    auto expected = field;
    time_step.value = 0.08f;
    diffusion.value = 0.1f;
    write_texture(state, field);
    for (int frame = 0; frame < 3; ++frame) {
        expected = infer(ring_features(expected, width, height), rule, 0.08f, 0.1f);
        step.do_step(data);
        require_close(read_texture(state), expected);
    }
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural RBF responses peak at preferred density and control cross-channel sharpness", "[neural][opengl]") {
    NeuralTestContext context;
    constexpr int width = 16;
    constexpr int height = 16;
    Data data;
    auto& state = *data.add_element<TextureData>("state", width, height, 1);
    ComputeShaderStep step("neural_mnca/ca.cs", {{0, "state"}}, {{2, "state"}});
    step.set_work_groups(1, 1, 1);
    REQUIRE(step.program().is_linked());
    float_option(step.program(), "step_size").value = 0.1f;
    float_option(step.program(), "diffusion").value = 0.0f;
    TestRuleBuffer buffer;
    Rule rule{};
    // Green in ring 7 excites red and inhibits blue, without modifying green itself.
    rule[3 * 7 + 1] = 1.0f;
    rule[centers_offset] = 0.45f;
    rule[output_weights_offset] = 0.2f;
    rule[output_weights_offset + 2 * hidden_count] = -0.15f;

    const auto response = [&](float density, float response_width) {
        CAPTURE(density, response_width);
        rule[widths_offset] = response_width;
        buffer.upload(rule);
        write_texture(state, constant_layer(width, height, {0.4f, density, 0.6f, 0.25f}));
        step.do_step(data);
        const auto actual = read_texture(state);
        const float distance = (density - 0.45f) / std::max(response_width, 0.005f);
        const float activation = std::exp(-distance * distance);
        require_close(actual, constant_layer(width, height,
            {0.4f + 0.1f * 0.2f * activation * 0.28f,
             density, 0.6f - 0.1f * 0.15f * activation * 0.28f, 1.0f}));
        CHECK(actual[1] == density);
        return actual[0] - 0.4f;
    };
    const float low = response(0.30f, 0.15f);
    const float peak = response(0.45f, 0.15f);
    const float high = response(0.60f, 0.15f);
    CHECK(std::abs(low - high) < 1.0e-6f);
    CHECK(peak > low + 0.003f);
    CHECK(peak > high + 0.003f);
    CHECK(std::abs(response(0.45f, 0.025f) - peak) < 1.0e-6f);
    const float broad = response(0.40f, 0.15f);
    const float narrow = response(0.40f, 0.025f);
    CHECK(broad > narrow + 0.004f);

    // Invalid/smaller raw widths use the shader's 0.005 safety floor, not a division by zero.
    const float minimum = response(0.454f, 0.005f);
    CHECK(std::abs(response(0.454f, 0.001f) - minimum) < 1.0e-6f);
    CHECK(std::abs(response(0.454f, 0.0f) - minimum) < 1.0e-6f);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural RBF outputs sum linearly and output bias supplies background decay", "[neural][opengl]") {
    NeuralTestContext context;
    constexpr int width = 16;
    constexpr int height = 16;
    Data data;
    auto& state = *data.add_element<TextureData>("state", width, height, 1);
    ComputeShaderStep step("neural_mnca/ca.cs", {{0, "state"}}, {{2, "state"}});
    step.set_work_groups(1, 1, 1);
    REQUIRE(step.program().is_linked());
    float_option(step.program(), "step_size").value = 0.1f;
    float_option(step.program(), "diffusion").value = 0.0f;
    TestRuleBuffer buffer;
    Rule rule{};
    for (int neuron = 0; neuron < hidden_count; ++neuron) {
        rule[neuron * input_count + 1] = 1.0f;
        rule[centers_offset + neuron] = 0.5f;
        rule[widths_offset + neuron] = 0.1f;
        rule[output_weights_offset + neuron] = 0.1f;
        rule[output_weights_offset + hidden_count + neuron] = -0.1f;
    }
    buffer.upload(rule);
    write_texture(state, constant_layer(width, height, {0.5f, 0.5f, 0.5f, 0.25f}));
    step.do_step(data);
    // The accumulated outputs are +/-3.2: a tanh at the output would give different values.
    require_close(read_texture(state), constant_layer(width, height, {0.58f, 0.42f, 0.5f, 1.0f}));

    rule = {};
    rule[output_bias_offset] = -0.02f;
    rule[output_bias_offset + 1] = 0.01f;
    buffer.upload(rule);
    const auto field = constant_layer(width, height, {0.2f, 0.7f, 0.4f, 0.25f});
    write_texture(state, field);
    step.do_step(data);
    const auto changed = read_texture(state);
    require_close(changed, constant_layer(width, height,
        {0.2f - 0.1f * 0.02f * 0.52f, 0.7f + 0.1f * 0.01f * 0.37f, 0.4f, 1.0f}));
    CHECK(changed[0] < field[0]);
    CHECK(changed[1] > field[1]);
    CHECK(changed[2] == field[2]);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural mutation uploads weights and honors save mutate restore shortcuts", "[neural][opengl]") {
    NeuralTestContext context;
    Data data;
    ComputeProgram program("neural_mnca/ca.cs");
    REQUIRE(program.is_linked());
    NeuralMutationControl control(program);
    control.update(data);
    const auto initial = read_weights();
    require_rbf_rule(initial);

    auto key_frame = [&](int key, bool captured = false) {
        auto& io = ImGui::GetIO();
        io.KeysDown[key] = true;
        ImGui::NewFrame();
        io.WantCaptureKeyboard = captured;
        io.WantTextInput = captured;
        control.draw();
        ImGui::EndFrame();
        io.KeysDown[key] = false;
    };
    auto update_frame = [&] {
        ImGui::NewFrame();
        control.update(data);
        control.draw();
        ImGui::EndFrame();
    };
    key_frame('V');
    CHECK(read_weights() == initial);
    update_frame();
    const auto saved = read_weights();
    REQUIRE(saved != initial);
    require_rbf_rule(saved);
    key_frame('S');
    update_frame();
    CHECK(read_weights() == saved);
    key_frame('V');
    CHECK(read_weights() == saved);
    update_frame();
    const auto mutated = read_weights();
    REQUIRE(mutated != saved);
    require_rbf_rule(mutated);
    for (int key : {'S', 'V', 'L'}) {
        CAPTURE(key);
        key_frame(key, true);
        update_frame();
        CHECK(read_weights() == mutated);
    }
    key_frame('L');
    CHECK(read_weights() == mutated);
    update_frame();
    CHECK(read_weights() == saved);
    update_frame();
    CHECK(read_weights() == saved);

    // An unchanged rule must rebind its buffer without uploading it again.
    const auto buffer = bound_rule_buffer();
    const float sentinel = saved.back() + 0.5f;
    glNamedBufferSubData(buffer, (weight_count - 1) * sizeof(float), sizeof(float), &sentinel);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
    float_option(program, "step_size").changed = true;
    update_frame();
    CHECK(program.is_linked());
    CHECK(bound_rule_buffer() == buffer);
    auto untouched = saved;
    untouched.back() = sentinel;
    CHECK(read_weights() == untouched);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural Randomize creates independent complete rules with mutation disabled", "[neural][opengl]") {
    NeuralTestContext context;
    ImGui::GetIO().DisplaySize = ImVec2(800, 900);
    Data data;
    ComputeProgram program("neural_mnca/ca.cs");
    REQUIRE(program.is_linked());
    NeuralMutationControl control(program);
    control.update(data);
    const auto baseline = read_weights();
    const auto buffer = bound_rule_buffer();

    const auto frame = [&] {
        ImGui::NewFrame();
        control.draw();
        ImGui::EndFrame();
        control.update(data);
    };
    const auto window = [](const char* name) -> ImGuiWindow& {
        auto* result = ImGui::FindWindowByName(name);
        INFO(name);
        REQUIRE(result != nullptr);
        REQUIRE(result->Active);
        REQUIRE_FALSE(result->Hidden);
        return *result;
    };
    const auto click = [&](ImVec2 position) {
        auto& io = ImGui::GetIO();
        io.MousePos = position;
        frame();
        io.MouseDown[0] = true;
        frame();
        io.MouseDown[0] = false;
        frame();
    };
    const auto shortcut = [&](int key) {
        auto& io = ImGui::GetIO();
        io.KeysDown[key] = true;
        ImGui::NewFrame();
        io.WantCaptureKeyboard = false;
        control.draw();
        ImGui::EndFrame();
        control.update(data);
        io.KeysDown[key] = false;
        frame();
    };

    // Open the actual menu and controls rather than exposing controller internals.
    frame();
    auto& bar = window("##MainMenuBar");
    click(ImVec2(bar.Pos.x + bar.DC.MenuBarOffset.x * 0.5f,
                 bar.MenuBarRect().GetCenter().y));
    frame();
    auto& menu = window("##Menu_00");
    click(ImVec2(menu.WorkRect.GetCenter().x,
                 menu.DC.CursorStartPos.y + ImGui::GetFontSize() * 0.5f));
    frame();
    window("Neural rule controls");

    const auto button = [&](int column) {
        const auto& controls = window("Neural rule controls");
        const auto& style = ImGui::GetStyle();
        const std::array<const char*, 3> labels{"Mutate", "Randomize", "Reset rule"};
        float x = controls.DC.CursorStartPos.x;
        for (int i = 0; i < column; ++i)
            x += ImGui::CalcTextSize(labels[i]).x + 2 * style.FramePadding.x + style.ItemSpacing.x;
        x += ImGui::CalcTextSize(labels[column]).x * 0.5f + style.FramePadding.x;
        return ImVec2(x, controls.DC.CursorStartPos.y + ImGui::GetFrameHeight() * 0.5f);
    };
    const auto disable_mutation_slider = [&](int row, const char* label) {
        auto& controls = window("Neural rule controls");
        // Two button rows and a horizontal separator precede these sliders.
        const float y = controls.DC.CursorStartPos.y
            + (2 + row) * ImGui::GetFrameHeightWithSpacing()
            + ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeight() * 0.5f;
        click(ImVec2(controls.DC.CursorStartPos.x + 1.0f, y));
        REQUIRE(ImGui::GetCurrentContext()->HoveredId == controls.GetID(label));
    };
    disable_mutation_slider(0, "Mutation strength");
    disable_mutation_slider(1, "Mutation rate");
    shortcut('V');
    REQUIRE(read_weights() == baseline);

    const auto require_fresh_rule = [&](const std::vector<float>& rule,
                                         const std::vector<float>& previous) {
        require_rbf_rule(rule);
        for (int neuron = 0; neuron < hidden_count; ++neuron) {
            CHECK(rule[centers_offset + neuron] >= 0.1f);
            CHECK(rule[centers_offset + neuron] <= 0.9f);
            CHECK(rule[widths_offset + neuron] >= 0.025f - 1.0e-6f);
            CHECK(rule[widths_offset + neuron] <= 0.18f + 1.0e-6f);
        }
        for (int index = output_weights_offset; index < output_bias_offset; ++index)
            CHECK(std::abs(rule[index]) <= 0.1f);
        for (int index = output_bias_offset; index < weight_count; ++index) {
            CHECK(rule[index] >= -0.025f);
            CHECK(rule[index] <= -0.005f);
        }
        // Neighborhoods, preferred densities, widths, outputs, and biases are all fresh.
        const std::array<int, 6> offsets{0, centers_offset, widths_offset,
                                        output_weights_offset, output_bias_offset, weight_count};
        for (size_t group = 0; group + 1 < offsets.size(); ++group) {
            CAPTURE(group);
            CHECK_FALSE(std::equal(rule.begin() + offsets[group], rule.begin() + offsets[group + 1],
                                   previous.begin() + offsets[group]));
        }
        CHECK(bound_rule_buffer() == buffer);
    };

    click(button(1));
    const auto first = read_weights();
    require_fresh_rule(first, baseline);
    shortcut('S');
    CHECK(read_weights() == first);
    click(button(1));
    const auto second = read_weights();
    require_fresh_rule(second, first);
    shortcut('V');
    CHECK(read_weights() == second);

    click(button(2));
    CHECK(read_weights() == baseline);
    shortcut('L');
    CHECK(read_weights() == first);
    frame();
    CHECK(read_weights() == first);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural mutation and initialization share editing-safe and nonrepeating V Q shortcuts", "[neural][opengl]") {
    NeuralTestContext context;
    Data data;
    auto& state = *data.add_element<TextureData>("state", 32, 32, 1);
    auto& init_program = data.add_element<FragmentProgramData>("init", "neural_mnca/init.fs")->get_program();
    REQUIRE(init_program.is_linked());
    initialization_seed(init_program).value = 93u;
    ComputeProgram program("neural_mnca/ca.cs");
    REQUIRE(program.is_linked());
    NeuralMutationControl mutation(program);
    InitTextureControl initialization("init", "state");

    mutation.update(data);
    initialization.update(data);
    const auto original_weights = read_weights();
    const auto marker = constant_layer(32, 32, {0.125f, 0.25f, 0.375f, 0.5f});
    write_texture(state, marker);

    auto frame = [&](bool v, bool q, bool editing = false, bool idle_capture = false) {
        auto& io = ImGui::GetIO();
        io.KeysDown['V'] = v;
        io.KeysDown['Q'] = q;
        ImGui::NewFrame();
        io.WantCaptureKeyboard = editing || idle_capture;
        io.WantTextInput = editing;
        if (idle_capture) {
            REQUIRE_FALSE(ImGui::IsAnyItemActive());
            REQUIRE_FALSE(ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel));
        }
        mutation.draw();
        initialization.draw();
        ImGui::EndFrame();
        mutation.update(data);
        initialization.update(data);
    };
    auto unchanged = [&](const std::vector<float>& weights) {
        CHECK(read_weights() == weights);
        CHECK(read_texture(state) == marker);
    };
    auto hold = [&](bool v, bool q, const std::vector<float>& weights) {
        // Keep a marker in the field so even an identical reinitialization is observable.
        write_texture(state, marker);
        const auto& io = ImGui::GetIO();
        REQUIRE(40 * io.DeltaTime > io.KeyRepeatDelay + io.KeyRepeatRate);
        for (int i = 0; i < 40; ++i) {
            CAPTURE(i, v, q);
            frame(v, q);
            unchanged(weights);
        }
    };

    frame(true, false, true);
    unchanged(original_weights);
    // Finishing text editing while V remains down must not manufacture a press.
    frame(true, false);
    unchanged(original_weights);
    frame(false, false);
    unchanged(original_weights);

    // Mere keyboard navigation capture must not prevent mutation or reinitialization.
    frame(true, false, false, true);
    const auto first_mutation = read_weights();
    REQUIRE(first_mutation != original_weights);
    require_rbf_rule(first_mutation);
    REQUIRE(read_texture(state) != marker);
    hold(true, false, first_mutation);
    frame(false, false);
    unchanged(first_mutation);

    frame(true, false);
    const auto second_mutation = read_weights();
    REQUIRE(second_mutation != first_mutation);
    require_rbf_rule(second_mutation);
    REQUIRE(read_texture(state) != marker);
    frame(false, false);
    write_texture(state, marker);

    frame(false, true, true);
    unchanged(second_mutation);
    frame(false, true);
    unchanged(second_mutation);
    frame(false, false);
    unchanged(second_mutation);

    frame(false, true, false, true);
    CHECK(read_weights() == second_mutation);
    REQUIRE(read_texture(state) != marker);
    hold(false, true, second_mutation);
    frame(false, false);
    unchanged(second_mutation);
    frame(false, true);
    CHECK(read_weights() == second_mutation);
    REQUIRE(read_texture(state) != marker);
    frame(false, false);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural MNCA setup initializes evolves clears and paints", "[neural][opengl]") {
    NeuralTestContext context;
    auto simulation = setup_neural_mnca();
    auto& data = simulation.data();
    auto* engine = dynamic_cast<ComputeShaderEngine*>(&simulation.engine());
    REQUIRE(engine != nullptr);
    REQUIRE(engine->compute_steps().size() == 1);
    for (auto* step : engine->compute_steps())
        REQUIRE(step->program().is_linked());
    CHECK(engine->compute_steps()[0]->shader_name() == "neural_mnca/ca.cs");
    float_option(engine->compute_steps()[0]->program(), "step_size").value = 0.08f;
    float_option(engine->compute_steps()[0]->program(), "diffusion").value = 0.1f;
    auto& state = data.get_element<TextureData>("data_texture");
    CHECK(state.width() == 1024);
    CHECK(state.height() == 1024);
    CHECK(state.depth() == 1);
    CHECK(state.options().internal_format == GL_RGBA32F);
    int textures = 0;
    data.for_each_element<TextureData>([&](const auto&, const auto&) { ++textures; });
    CHECK(textures == 1);
    REQUIRE(data.get_element<FragmentProgramData>("init_mnca").get_program().is_linked());
    // Keep the long-running behavior regression independent of startup randomness.
    initialization_seed(data.get_element<FragmentProgramData>("init_mnca").get_program()).value = 93u;
    auto frame = [&](bool advance) {
        ImGui::NewFrame();
        for (const auto& controller : simulation.controllers())
            controller->update(data);
        if (advance)
            engine->step(data);
        for (const auto& controller : simulation.controllers())
            controller->post_process(data);
        for (const auto& controller : simulation.controllers())
            controller->draw();
        ImGui::EndFrame();
    };
    frame(false);
    const auto initialized = read_texture(state);
    require_active_state(initialized);
    for (int i = 0; i < 16; ++i)
        frame(true);
    const auto evolved = read_texture(state);
    require_active_state(evolved);
    CHECK(evolved != initialized);

    for (int i = 16; i < 128; ++i)
        engine->step(data);
    const auto sustained = read_texture(state);
    // The starting rule must stay bounded without collapsing to a spatially uniform field.
    require_active_state(sustained);

    // Input-only frames must not accidentally advance or reinitialize the CA.
    frame(false);
    CHECK(read_texture(state) == sustained);
    auto& io = ImGui::GetIO();
    io.KeysDown['P'] = true;
    frame(false);
    io.KeysDown['P'] = false;
    frame(false);
    const std::array<double, 3> zero_rgb{};
    CHECK(rgb_sum(read_texture(state)) == zero_rgb);
    io.MousePos = ImVec2(256, 256);
    io.MouseDown[0] = true;
    frame(false);
    io.MouseDown[0] = false;
    const auto painted = read_texture(state);
    const auto painted_sum = rgb_sum(painted);
    for (double total : painted_sum)
        REQUIRE(total > 0.0);
    frame(false);
    CHECK(read_texture(state) == painted);
    io.MouseDown[1] = true;
    frame(false);
    io.MouseDown[1] = false;
    const auto erased = read_texture(state);
    const auto erased_sum = rgb_sum(erased);
    for (int channel = 0; channel < 3; ++channel)
        CHECK(erased_sum[channel] < painted_sum[channel]);
    CHECK(std::all_of(erased.begin(), erased.end(), [](float value) {
        return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
    }));
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural MNCA randomizes its startup seed", "[neural][opengl]") {
    NeuralTestContext context;
    std::array<uint32_t, 4> seeds{};
    for (auto& startup_seed : seeds) {
        auto simulation = setup_neural_mnca();
        auto& data = simulation.data();
        auto& program = data.get_element<FragmentProgramData>("init_mnca").get_program();
        auto& seed = initialization_seed(program);
        CHECK(seed.min == 0u);
        CHECK(seed.max == 255u);
        startup_seed = seed.value;
        CHECK(seed.value >= seed.min);
        CHECK(seed.value <= seed.max);
        REQUIRE(program.use());
        GLuint uploaded = 0;
        glGetUniformuiv(program.get_id(), uniform_location(program, "seed"), &uploaded);
        CHECK(uploaded == startup_seed);

    }
    CHECK(std::any_of(seeds.begin() + 1, seeds.end(), [&](uint32_t seed) {
        return seed != seeds.front();
    }));
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural MNCA optimized noise matches the original Slack initializer", "[neural][opengl]") {
    NeuralTestContext context;
    Data data;
    auto& optimized = data.add_element<FragmentProgramData>("optimized_init", "neural_mnca/init.fs")->get_program();
    auto& original = data.add_element<FragmentProgramData>("original_init", "slack_automata/init.fs")->get_program();
    const std::array<const char*, 3> channels{"r", "g", "b"};
    for (auto* program : {&optimized, &original}) {
        REQUIRE(program->is_linked());
        REQUIRE(program->get_options().size() == 10);
        const auto& seed = initialization_seed(*program);
        CHECK(seed.value == 93u);
        CHECK(seed.min == 0u);
        CHECK(seed.max == 255u);
        for (const auto* channel : channels) {
            const auto& power = float_option(*program, std::string("power_") + channel);
            const auto& scale = float_option(*program, std::string("scale_") + channel);
            const auto& amplitude = float_option(*program, std::string("amp_") + channel);
            CHECK(power.value == 1.0f);
            CHECK(power.min == 0.0f);
            CHECK(power.max == 1.0f);
            CHECK(scale.value == 1.0f);
            CHECK(scale.min == 0.05f);
            CHECK(scale.max == 10.0f);
            CHECK(amplitude.value == 0.4f);
            CHECK(amplitude.min == 0.0f);
            CHECK(amplitude.max == 1.0f);
        }
    }
    struct NoiseCase {
        int width;
        int height;
        uint32_t seed;
        uint32_t sequence;
        std::array<float, 3> scales;
        std::array<float, 3> powers;
        std::array<float, 3> amplitudes;
    };
    const std::array<NoiseCase, 7> cases{{
        {96, 64, 93u, 0u, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {0.4f, 0.4f, 0.4f}},
        // The first lump has radius zero at the minimum scale.
        {37, 23, 0u, 0u, {0.05f, 0.05f, 0.05f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.4f, 1.0f}},
        // Straddle round(6*scale) == 0.5 and round(22*scale) == 1.5.
        {35, 19, 255u, 3u, {0.08332f, 0.08335f, 0.06817f}, {0.25f, 0.7f, 1.0f}, {0.2f, 0.6f, 0.9f}},
        {19, 35, 42u, 11u, {0.06820f, 0.24999f, 0.25001f}, {1.0f, 0.35f, 0.8f}, {1.0f, 0.0f, 0.5f}},
        {47, 31, 17u, 255u, {0.15f, 0.31f, 0.73f}, {0.0f, 0.6f, 1.0f}, {0.8f, 0.3f, 0.1f}},
        {31, 17, 201u, 7u, {0.4f, 1.0f, 1.2f}, {1.0f, 0.0f, 0.5f}, {0.4f, 1.0f, 0.0f}},
        // Exercise the largest permitted radii without a large reference render.
        {7, 5, 93u, 1u, {2.0f, 5.0f, 10.0f}, {0.8f, 1.0f, 0.0f}, {0.0f, 0.7f, 1.0f}},
    }};
    for (size_t index = 0; index < cases.size(); ++index) {
        const auto& test = cases[index];
        CAPTURE(index, test.width, test.height, test.seed, test.sequence);
        const auto optimized_name = "optimized_state_" + std::to_string(index);
        const auto original_name = "original_state_" + std::to_string(index);
        auto& optimized_state = *data.add_element<TextureData>(optimized_name, test.width, test.height, 1);
        auto& original_state = *data.add_element<TextureData>(original_name, test.width, test.height, 1);
        FragmentShaderStep optimized_step("optimized_init", {}, optimized_name, 1);
        FragmentShaderStep original_step("original_init", {}, original_name, 1);
        for (auto* program : {&optimized, &original}) {
            initialization_seed(*program).value = test.seed;
            for (size_t channel = 0; channel < channels.size(); ++channel) {
                float_option(*program, std::string("power_") + channels[channel]).value = test.powers[channel];
                float_option(*program, std::string("scale_") + channels[channel]).value = test.scales[channel];
                float_option(*program, std::string("amp_") + channels[channel]).value = test.amplitudes[channel];
            }
            REQUIRE(program->use());
            program->set_uniform("rand_seed", test.sequence);
        }
        optimized_step.do_step(data);
        original_step.do_step(data);
        if (index == 0) {
            // Warm shaders once before measuring; timings are diagnostic, not a speed assertion.
            std::array<GLuint, 2> queries{};
            glGenQueries(queries.size(), queries.data());
            glBeginQuery(GL_TIME_ELAPSED, queries[0]);
            optimized_step.do_step(data);
            glEndQuery(GL_TIME_ELAPSED);
            glBeginQuery(GL_TIME_ELAPSED, queries[1]);
            original_step.do_step(data);
            glEndQuery(GL_TIME_ELAPSED);
            std::array<GLuint64, 2> nanoseconds{};
            for (size_t query = 0; query < queries.size(); ++query)
                glGetQueryObjectui64v(queries[query], GL_QUERY_RESULT, &nanoseconds[query]);
            glDeleteQueries(queries.size(), queries.data());
            INFO("Default 96x64 initialization GPU milliseconds: optimized=" << nanoseconds[0] / 1.0e6
                 << ", original=" << nanoseconds[1] / 1.0e6);
            CHECK(glGetError() == GL_NO_ERROR);
        }
        const auto actual = read_texture(optimized_state);
        require_bounded_state(actual);
        require_close(actual, read_texture(original_state));
        for (size_t pixel = 0; pixel < actual.size(); pixel += 4) {
            for (size_t channel = 0; channel < channels.size(); ++channel) {
                REQUIRE(actual[pixel + channel] <= test.powers[channel]);
                if (test.powers[channel] == 0.0f)
                    REQUIRE(actual[pixel + channel] == 0.0f);
            }
        }
    }
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural MNCA Slack-style noise repeats seeds and applies linear channel power", "[neural][opengl]") {
    NeuralTestContext context;
    Data data;
    constexpr int width = 53;
    constexpr int height = 29;
    auto& state = *data.add_element<TextureData>("state", width, height, 1);
    auto& program = data.add_element<FragmentProgramData>("init", "neural_mnca/init.fs")->get_program();
    REQUIRE(program.is_linked());
    FragmentShaderStep step("init", {}, "state", 1);
    const std::array<const char*, 3> channels{"r", "g", "b"};
    for (const auto* channel : channels)
        float_option(program, std::string("scale_") + channel).value = 0.3f;
    const auto initialize = [&](uint32_t seed, uint32_t sequence) {
        initialization_seed(program).value = seed;
        REQUIRE(program.use());
        program.set_uniform("rand_seed", sequence);
        step.do_step(data);
        auto pixels = read_texture(state);
        require_bounded_state(pixels);
        return pixels;
    };
    const auto first = initialize(93u, 0u);
    require_active_state(first);
    CHECK(initialize(93u, 0u) == first);
    CHECK(initialize(42u, 0u) != first);
    CHECK(initialize(93u, 1u) != first);
    CHECK(initialize(94u, 0u) == initialize(93u, 1u));

    const std::array<float, 3> powers{0.0f, 0.35f, 0.8f};
    for (size_t channel = 0; channel < channels.size(); ++channel)
        float_option(program, std::string("power_") + channels[channel]).value = powers[channel];
    auto expected = first;
    for (size_t pixel = 0; pixel < expected.size(); pixel += 4)
        for (size_t channel = 0; channel < channels.size(); ++channel)
            expected[pixel + channel] *= powers[channel];
    require_close(initialize(93u, 0u), expected);
    for (const auto* channel : channels)
        float_option(program, std::string("power_") + channel).value = 0.0f;
    require_close(initialize(255u, 13u), constant_layer(width, height, {0.0f, 0.0f, 0.0f, 1.0f}));
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural mutation controllers start with independent random sequences", "[neural][opengl]") {
    NeuralTestContext context;
    Data data;
    ComputeProgram program("neural_mnca/ca.cs");
    REQUIRE(program.is_linked());
    std::array<std::vector<float>, 3> mutations;
    for (auto& weights : mutations) {
        NeuralMutationControl control(program);
        control.update(data);
        auto& io = ImGui::GetIO();
        io.KeysDown['V'] = true;
        ImGui::NewFrame();
        io.WantCaptureKeyboard = false;
        control.draw();
        ImGui::EndFrame();
        io.KeysDown['V'] = false;
        ImGui::NewFrame();
        control.update(data);
        ImGui::EndFrame();
        weights = read_weights();
        require_rbf_rule(weights);
    }
    CHECK(std::any_of(mutations.begin() + 1, mutations.end(), [&](const auto& weights) {
        return weights != mutations.front();
    }));
    CHECK(glGetError() == GL_NO_ERROR);
}
