#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "automata/neural/rule.hpp"
#include "automata/neural/rule_model.hpp"
#include "automata/rule_browser.hpp"
#include "controllers/rule_browser_control.hpp"
#include "data.hpp"
#include "engines/compute_shader_engine.hpp"
#include "engines/select_step.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "setup/setups.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

bool InitOpenGLForTesting(GLFWwindow** window, std::string& error);

namespace {
constexpr int centers_offset = neural::input_count * neural::neuron_count;
constexpr int widths_offset = centers_offset + neural::neuron_count;
constexpr int outputs_offset = widths_offset + neural::neuron_count;
constexpr int biases_offset = outputs_offset + 3 * neural::neuron_count;

class BrowserTestContext {
public:
    BrowserTestContext() : previous_(ImGui::GetCurrentContext()) {
        std::string error;
        if (!InitOpenGLForTesting(&window_, error))
            SKIP("OpenGL unavailable: " << error);
        context_ = ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(800, 600);
        io.DeltaTime = 1.0f / 60.0f;
        io.Fonts->Build();
    }
    ~BrowserTestContext() {
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

GLSL::ValueOption<float>& float_option(auto& program, const std::string& name) {
    for (auto* option : program.get_options()) {
        if (option->name == name) {
            REQUIRE(option->type == GLSL::IOption::Type::Float);
            REQUIRE_FALSE(option->compile_constant);
            return *static_cast<GLSL::ValueOption<float>*>(option);
        }
    }
    FAIL("Missing option " << name);
    std::terminate();
}

void configure_initializer(FragmentProgram& program) {
    REQUIRE(program.is_linked());
    for (const auto* channel : {"r", "g", "b"})
        float_option(program, std::string("scale_") + channel).value = 0.1f;
    for (auto* option : program.get_options())
        if (option->name == "seed")
            static_cast<GLSL::ValueOption<uint32_t>*>(option)->value = 93u;
}

GLSL::ValueOption<uint32_t>& seed_option(FragmentProgram& program) {
    for (auto* option : program.get_options()) {
        if (option->name == "seed") {
            REQUIRE(option->type == GLSL::IOption::Type::UInt);
            return *static_cast<GLSL::ValueOption<uint32_t>*>(option);
        }
    }
    FAIL("Missing initialization seed");
    std::terminate();
}

std::vector<float> read_texture(const Texture& texture) {
    std::vector<float> result(texture.width * texture.height * texture.depth * 4);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    glGetTextureImage(texture.gl_tex_num, 0, GL_RGBA, GL_FLOAT,
                      static_cast<GLsizei>(result.size() * sizeof(float)), result.data());
    return result;
}

std::vector<float> read_texture(TextureData& texture) {
    return read_texture(texture.get_texture());
}

void write_texture(TextureData& texture, const std::vector<float>& pixels) {
    REQUIRE(pixels.size() == texture.width() * texture.height() * texture.depth() * 4);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    for (auto* target : {&texture.get_texture(), &texture.get_last_texture()})
        glTextureSubImage3D(target->gl_tex_num, 0, 0, 0, 0,
                            texture.width(), texture.height(), texture.depth(), GL_RGBA, GL_FLOAT, pixels.data());
}

std::vector<float> marker_field(int width, int height, float value) {
    std::vector<float> result(width * height * 4, 1.0f);
    for (int pixel = 0; pixel < width * height; ++pixel) {
        result[4 * pixel] = value;
        result[4 * pixel + 1] = value * 0.5f;
        result[4 * pixel + 2] = value * 0.25f;
    }
    return result;
}

void require_close(const std::vector<float>& actual, const std::vector<float>& expected) {
    REQUIRE(actual.size() == expected.size());
    float maximum = 0.0f;
    bool finite = true;
    for (size_t index = 0; index < actual.size(); ++index) {
        finite &= std::isfinite(actual[index]);
        maximum = std::max(maximum, std::abs(actual[index] - expected[index]));
    }
    REQUIRE(finite);
    REQUIRE(maximum < 2.0e-5f);
}

void require_all_preview_layers(TextureData& previews, const std::vector<float>& field) {
    const size_t layer_size = previews.width() * previews.height() * 4;
    REQUIRE(field.size() == layer_size);
    for (const auto* texture : {&previews.get_texture(), &previews.get_last_texture()}) {
        const auto pixels = read_texture(*texture);
        for (int layer = 0; layer < rule_browser::browser_count; ++layer) {
            CAPTURE(layer);
            REQUIRE(std::equal(field.begin(), field.end(), pixels.begin() + layer * layer_size));
        }
    }
}

void require_valid_rule(const neural::Rule& rule) {
    const auto packed = neural::pack_rule(rule);
    REQUIRE(std::all_of(packed.begin(), packed.end(), [](float value) { return std::isfinite(value); }));
    for (int neuron = 0; neuron < neural::neuron_count; ++neuron) {
        float total = 0.0f;
        for (int input = 0; input < neural::input_count; ++input) {
            const float weight = packed[neuron * neural::input_count + input];
            REQUIRE(weight > 0.0f);
            total += weight;
        }
        CHECK(std::abs(total - 1.0f) < 2.0e-6f);
        CHECK(packed[centers_offset + neuron] >= 0.0f);
        CHECK(packed[centers_offset + neuron] <= 1.0f);
        CHECK(packed[widths_offset + neuron] >= 0.005f);
    }
    for (int output = outputs_offset; output < biases_offset; ++output)
        CHECK(std::abs(packed[output]) <= neural::output_limit);
    for (int output = biases_offset; output < neural::coefficient_count; ++output)
        CHECK(std::abs(packed[output]) <= neural::bias_limit);
}

void open_browser_controls(auto&& frame) {
    frame();
    auto* controls = ImGui::FindWindowByName("Rule browser controls");
    if (controls && controls->Active && !controls->Hidden)
        return;
    const auto click = [&](ImVec2 position) {
        auto& io = ImGui::GetIO();
        io.MousePos = position;
        frame();
        io.MouseDown[0] = true;
        frame();
        io.MouseDown[0] = false;
        frame();
    };
    auto* bar = ImGui::FindWindowByName("##MainMenuBar");
    REQUIRE(bar != nullptr);
    const auto& style = ImGui::GetStyle();
    click(ImVec2(bar->Pos.x + style.DisplaySafeAreaPadding.x + style.ItemSpacing.x * 0.5f
                 + ImGui::CalcTextSize("Rule browser").x * 0.5f,
                 bar->MenuBarRect().GetCenter().y));
    frame();
    auto* menu = ImGui::FindWindowByName("##Menu_00");
    REQUIRE(menu != nullptr);
    REQUIRE(menu->Active);
    REQUIRE_FALSE(menu->Hidden);
    // Controls follows Enabled, Fresh batch, Mutate selected, Reset previews, and Use selected rule.
    click(ImVec2(menu->WorkRect.GetCenter().x,
                 menu->DC.CursorStartPos.y + 5 * ImGui::GetTextLineHeightWithSpacing()
                 + ImGui::GetFontSize() * 0.5f));
    frame();
    controls = ImGui::FindWindowByName("Rule browser controls");
    REQUIRE(controls != nullptr);
    REQUIRE(controls->Active);
    REQUIRE_FALSE(controls->Hidden);
}

std::vector<neural::Rule> snapshot(const neural::RuleModel& model) {
    std::vector<neural::Rule> result;
    for (std::size_t index = 0; index < model.size(); ++index)
        result.push_back(model.rule(index));
    return result;
}

struct BrowserHarness {
    Data data;
    rule_browser::State browser;
    neural::RuleModel main_rule;
    neural::RuleModel preview_rules{rule_browser::browser_count};
    TextureOptions options;
    TextureData& main = *data.add_element<TextureData>("data_texture", 32, 32, 1, options);
    TextureData& previews = *data.add_element<TextureData>("rule_browser_state", 16, 16, rule_browser::browser_count, options);
    TextureData& display = *data.add_element<TextureData>("rule_browser_display", 64, 64, 1);
    FragmentProgram& initializer = data.add_element<FragmentProgramData>("initializer", "neural_mnca/init.fs")->get_program();
    RuleBrowserControl control{
        browser, main_rule, preview_rules,
        main, previews, display, initializer};

    explicit BrowserHarness(GLenum format = GL_RGBA32F) : options{[format] {
        auto value = default_options;
        value.internal_format = format;
        return value;
    }()} {
        configure_initializer(initializer);
        write_texture(main, marker_field(32, 32, 0.375f));
    }
    void frame() {
        ImGui::NewFrame();
        control.draw();
        ImGui::EndFrame();
    }
    void press(int key) {
        auto& io = ImGui::GetIO();
        io.KeysDown[key] = true;
        frame();
        io.KeysDown[key] = false;
        frame();
    }
    void click(ImVec2 position) {
        auto& io = ImGui::GetIO();
        io.MousePos = position;
        frame();
        io.MouseDown[0] = true;
        frame();
        io.MouseDown[0] = false;
        frame();
    }
    ImVec2 apply_button() {
        open_browser_controls([&] { frame(); });
        auto* window = ImGui::FindWindowByName("Rule browser controls");
        REQUIRE(window != nullptr);
        REQUIRE(window->Active);
        REQUIRE_FALSE(window->Hidden);
        const auto& style = ImGui::GetStyle();
        const float x = window->DC.CursorStartPos.x
            + ImGui::CalcTextSize("Reset previews").x + 2 * style.FramePadding.x + style.ItemSpacing.x
            + ImGui::CalcTextSize("Use selected rule").x * 0.5f + style.FramePadding.x;
        const float y = window->DC.CursorStartPos.y + 2 * ImGui::GetTextLineHeightWithSpacing()
            + ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight() * 0.5f;
        return ImVec2(x, y);
    }
};
}

TEST_CASE("Neural rule randomization and mutation are reproducible without a GPU", "[neural][browser][cpu]") {
    REQUIRE(neural::baseline_rule() == neural::baseline_rule());
    std::mt19937 first(12345u);
    std::mt19937 second(12345u);
    std::mt19937 other(54321u);
    const auto randomized = neural::random_rule(first);
    REQUIRE(randomized == neural::random_rule(second));
    CHECK(first == second);
    CHECK(randomized != neural::baseline_rule());
    CHECK(randomized != neural::random_rule(other));
    CHECK(randomized != neural::random_rule(first));
    require_valid_rule(randomized);
    auto left = randomized;
    auto right = randomized;
    first.seed(9876u);
    second.seed(9876u);
    REQUIRE(neural::mutate_rule(left, first, 0.3f, 0.35f));
    REQUIRE(neural::mutate_rule(right, second, 0.3f, 0.35f));
    CHECK(left == right);
    CHECK(first == second);
    CHECK(left != randomized);
    require_valid_rule(left);

    const auto unchanged_rng = first;
    CHECK_FALSE(neural::mutate_rule(left, first, 0.0f, 1.0f));
    CHECK_FALSE(neural::mutate_rule(left, first, 1.0f, 0.0f));
    CHECK(left == right);
    CHECK(first == unchanged_rng);
    neural::RuleModel state;
    CHECK(state.rule() == neural::baseline_rule());
    CHECK(state.dirty);
}

TEST_CASE("Neural rule packing normalizes each neighborhood and preserves tensor offsets", "[neural][browser][cpu]") {
    static_assert(neural::coefficient_count == 1411);
    auto rule = neural::baseline_rule();
    require_valid_rule(rule);
    std::fill(rule.input_logits.begin(), rule.input_logits.end(), 0.0f);
    for (int input = 0; input < neural::input_count; ++input)
        rule.input_logits[7 * neural::input_count + input] = -neural::input_limit;
    rule.input_logits[7 * neural::input_count + 13] = neural::input_limit;
    const auto packed = neural::pack_rule(rule);
    for (int neuron = 0; neuron < neural::neuron_count; ++neuron) {
        for (int input = 0; input < neural::input_count; ++input) {
            const double unnormalized = neuron == 7 ? std::exp(input == 13 ? 0.0 : -24.0) : 1.0;
            const double total = neuron == 7 ? 1.0 + 38.0 * std::exp(-24.0) : 39.0;
            CHECK(std::abs(packed[neuron * neural::input_count + input] - unnormalized / total) < 1.0e-7);
        }
        CHECK(packed[centers_offset + neuron] == rule.centers[neuron]);
        CHECK(packed[widths_offset + neuron] == neural::response_width(rule.width_parameters[neuron]));
    }
    CHECK(std::equal(rule.output_weights.begin(), rule.output_weights.end(), packed.begin() + outputs_offset));
    CHECK(std::equal(rule.output_bias.begin(), rule.output_bias.end(), packed.begin() + biases_offset));
    for (float width : {0.006f, 0.025f, 0.1f, 0.2f, 0.6f})
        CHECK(std::abs(neural::response_width(neural::width_parameter(width)) - width) < 1.0e-6f);
}

TEST_CASE("Neural browser generates resets and applies rules while paused", "[neural][browser][opengl]") {
    BrowserTestContext context;
    BrowserHarness harness;
    const auto main_marker = read_texture(harness.main);
    const auto starting_rule = harness.main_rule.rule();
    harness.frame();
    REQUIRE_FALSE(harness.browser.enabled);
    REQUIRE(harness.browser.generation == 0);
    harness.press('B');
    REQUIRE(harness.browser.enabled);
    CHECK(harness.browser.generation == 1);
    CHECK(harness.browser.steps == 0);
    CHECK(harness.browser.selected == 0);
    CHECK(ImGui::FindWindowByName("Rule browser controls") == nullptr);
    REQUIRE(harness.preview_rules.rule(0) == starting_rule);
    CHECK(read_texture(harness.main) == main_marker);
    for (int candidate = 0; candidate < rule_browser::browser_count; ++candidate) {
        CAPTURE(candidate);
        require_valid_rule(harness.preview_rules.rule(candidate));
        if (candidate > 0)
            REQUIRE(harness.preview_rules.rule(candidate) != starting_rule);
        for (int earlier = 0; earlier < candidate; ++earlier)
            CHECK(harness.preview_rules.rule(candidate) != harness.preview_rules.rule(earlier));
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
    glUseProgram(0);
    harness.control.update(harness.data);
    GLint binding = -1;
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 4, &binding);
    CHECK(binding == 0);
    GLint active_program = -1;
    glGetIntegerv(GL_CURRENT_PROGRAM, &active_program);
    CHECK(active_program == 0);
    harness.preview_rules.bind(4);
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 4, &binding);
    REQUIRE(binding != 0);
    std::vector<float> uploaded(rule_browser::browser_count * neural::coefficient_count);
    glGetNamedBufferSubData(static_cast<GLuint>(binding), 0, uploaded.size() * sizeof(float), uploaded.data());
    for (int candidate = 0; candidate < rule_browser::browser_count; ++candidate) {
        const auto expected = neural::pack_rule(harness.preview_rules.rule(candidate));
        REQUIRE(std::equal(expected.begin(), expected.end(),
                            uploaded.begin() + candidate * neural::coefficient_count));
    }

    auto& reference = *harness.data.add_element<TextureData>("reference_initial", 16, 16, 1);
    REQUIRE(harness.initializer.use());
    harness.initializer.set_uniform("rand_seed", 0u);
    FragmentShaderStep initialize_reference("initializer", {}, "reference_initial", 1);
    initialize_reference.do_step(harness.data);
    const auto cached_field = read_texture(reference);
    require_all_preview_layers(harness.previews, cached_field);

    harness.browser.selected = 5;
    const auto parent = harness.preview_rules.rule(5);
    harness.browser.steps = 29;
    harness.press('V');
    CHECK(harness.browser.generation == 2);
    CHECK(harness.browser.selected == 0);
    CHECK(harness.browser.steps == 0);
    REQUIRE(harness.preview_rules.rule(0) == parent);
    for (int candidate = 1; candidate < rule_browser::browser_count; ++candidate) {
        CAPTURE(candidate);
        REQUIRE(harness.preview_rules.rule(candidate) != parent);
        for (int earlier = 1; earlier < candidate; ++earlier)
            CHECK(harness.preview_rules.rule(candidate) != harness.preview_rules.rule(earlier));
    }
    require_all_preview_layers(harness.previews, cached_field);
    const auto mutated = snapshot(harness.preview_rules);
    const auto dirty_previews = std::vector<float>(16 * 16 * rule_browser::browser_count * 4, 0.8125f);
    write_texture(harness.previews, dirty_previews);
    harness.browser.steps = 41;
    harness.press('B');
    REQUIRE_FALSE(harness.browser.enabled);
    CHECK(harness.browser.steps == 41);
    harness.press('B');
    REQUIRE(harness.browser.enabled);
    CHECK(harness.browser.generation == 2);
    CHECK(harness.browser.steps == 41);
    CHECK(snapshot(harness.preview_rules) == mutated);
    CHECK(read_texture(harness.previews) == dirty_previews);
    CHECK(read_texture(harness.previews.get_last_texture()) == dirty_previews);
    harness.press('Q');
    CHECK(snapshot(harness.preview_rules) == mutated);
    CHECK(harness.browser.generation == 2);
    CHECK(harness.browser.steps == 0);
    require_all_preview_layers(harness.previews, cached_field);
    CHECK(read_texture(harness.main) == main_marker);
    CHECK(harness.main_rule.rule() == starting_rule);

    auto& main_reference = *harness.data.add_element<TextureData>("reference_main", 32, 32, 1);
    REQUIRE(harness.initializer.use());
    harness.initializer.set_uniform("rand_seed", 0u);
    FragmentShaderStep initialize_main("initializer", {}, "reference_main", 1);
    initialize_main.do_step(harness.data);
    const auto expected_main = read_texture(main_reference);
    // Applying a preview restores the initialization settings captured by its reset.
    seed_option(harness.initializer).value = 17u;
    float_option(harness.initializer, "scale_r").value = 0.4f;
    float_option(harness.initializer, "amp_g").value = 0.9f;
    float_option(harness.initializer, "power_b").value = 0.25f;
    harness.browser.selected = 7;
    const auto selected = harness.preview_rules.rule(7);
    harness.main_rule.dirty = false;
    harness.frame();
    harness.click(harness.apply_button());
    REQUIRE_FALSE(harness.browser.enabled);
    CHECK(harness.main_rule.rule() == selected);
    CHECK(harness.main_rule.dirty);
    CHECK(seed_option(harness.initializer).value == 93u);
    CHECK(float_option(harness.initializer, "scale_r").value == 0.1f);
    CHECK(float_option(harness.initializer, "amp_g").value == 0.4f);
    CHECK(float_option(harness.initializer, "power_b").value == 1.0f);
    require_close(read_texture(harness.main), expected_main);
    CHECK(snapshot(harness.preview_rules) == mutated);
    REQUIRE_FALSE(ImGui::GetIO().MouseDown[0]);
    harness.frame();
    CHECK(read_texture(harness.main) == expected_main);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural browser grid selects all candidates and double-click promotes on release", "[neural][browser][opengl]") {
    BrowserTestContext context;
    BrowserHarness harness;
    const auto marker = read_texture(harness.main);
    harness.press('B');
    const auto rules = snapshot(harness.preview_rules);
    for (int candidate = 0; candidate < rule_browser::browser_count; ++candidate) {
        CAPTURE(candidate);
        harness.click(ImVec2((candidate % 4 + 0.5f) * 200.0f,
                             (candidate / 4 + 0.5f) * 150.0f));
        REQUIRE(harness.browser.enabled);
        CHECK(harness.browser.selected == candidate);
        CHECK(read_texture(harness.main) == marker);
    }
    harness.main_rule.dirty = false;
    auto& io = ImGui::GetIO();
    // Click the last candidate a second time within ImGui's double-click interval.
    io.MouseDown[0] = true;
    harness.frame();
    REQUIRE(ImGui::IsMouseDoubleClicked(0));
    REQUIRE(harness.browser.enabled);
    CHECK(harness.main_rule.dirty == false);
    CHECK(read_texture(harness.main) == marker);
    harness.frame();
    REQUIRE(harness.browser.enabled);
    io.MouseDown[0] = false;
    harness.frame();
    REQUIRE_FALSE(harness.browser.enabled);
    CHECK(harness.main_rule.rule() == rules[15]);
    CHECK(harness.main_rule.dirty);
    CHECK(snapshot(harness.preview_rules) == rules);
    const auto applied = read_texture(harness.main);
    REQUIRE(applied != marker);
    harness.frame();
    CHECK(read_texture(harness.main) == applied);
    CHECK_FALSE(io.MouseDown[0]);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural browser compositor uses top-origin candidates and leaves sources unchanged", "[neural][browser][opengl]") {
    const GLenum format = GENERATE(GL_RGBA32F, GL_RGBA16);
    CAPTURE(format);
    BrowserTestContext context;
    BrowserHarness harness(format);
    auto main = marker_field(32, 32, 0.5f);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            main[(y * 32 + x) * 4] = x / 31.0f;
            main[(y * 32 + x) * 4 + 1] = y / 31.0f;
        }
    write_texture(harness.main, main);
    main = read_texture(harness.main);
    std::vector<float> previews(16 * 16 * rule_browser::browser_count * 4, 1.0f);
    for (int candidate = 0; candidate < rule_browser::browser_count; ++candidate)
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const int offset = ((candidate * 16 + y) * 16 + x) * 4;
                previews[offset] = (candidate + 1) / 17.0f;
                previews[offset + 1] = x / 15.0f;
                previews[offset + 2] = y / 15.0f;
            }
    write_texture(harness.previews, previews);
    previews = read_texture(harness.previews);
    harness.browser.enabled = true;
    harness.browser.selected = 9;
    harness.browser.steps = 23;
    harness.frame();
    std::vector<float> expected(64 * 64 * 4, 1.0f);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const int candidate = (3 - y / 16) * 4 + x / 16;
            const int source = ((candidate * 16 + y % 16) * 16 + x % 16) * 4;
            std::copy_n(previews.begin() + source, 4, expected.begin() + (y * 64 + x) * 4);
        }
    if (format == GL_RGBA32F)
        CHECK(read_texture(harness.display) == expected);
    else
        require_close(read_texture(harness.display), expected);
    CHECK(harness.browser.selected == 9);
    CHECK(harness.browser.steps == 23);
    CHECK(read_texture(harness.main) == main);
    CHECK(read_texture(harness.main.get_last_texture()) == main);
    CHECK(read_texture(harness.previews) == previews);
    CHECK(read_texture(harness.previews.get_last_texture()) == previews);

    harness.press('B');
    REQUIRE_FALSE(harness.browser.enabled);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x)
            std::copy_n(main.begin() + ((y / 2) * 32 + x / 2) * 4, 4,
                         expected.begin() + (y * 64 + x) * 4);
    if (format == GL_RGBA32F)
        CHECK(read_texture(harness.display) == expected);
    else
        require_close(read_texture(harness.display), expected);
    CHECK(read_texture(harness.main) == main);
    CHECK(read_texture(harness.previews) == previews);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Neural browser SelectStep shares pending shader options with the normal branch", "[neural][browser][opengl]") {
    BrowserTestContext context;
    Data data;
    auto& main = *data.add_element<TextureData>("data_texture", 16, 16, 1);
    auto& previews = *data.add_element<TextureData>("rule_browser_state", 16, 16, rule_browser::browser_count);
    const auto uniform_field = [](int depth, float red) {
        std::vector<float> pixels(16 * 16 * depth * 4, 0.5f);
        for (size_t pixel = 0; pixel < pixels.size(); pixel += 4) {
            pixels[pixel] = red;
            pixels[pixel + 3] = 1.0f;
        }
        return pixels;
    };
    const auto initial_main = uniform_field(1, 0.5f);
    write_texture(main, initial_main);
    write_texture(previews, uniform_field(rule_browser::browser_count, 0.5f));
    neural::Rule rule;
    rule.output_bias[0] = 0.05f;
    auto& main_rules = *data.add_element<neural::RuleModel>("neural_rules");
    main_rules.rule() = rule;
    auto& preview_rules = *data.add_element<neural::RuleModel>("rule_browser_rules", rule_browser::browser_count);
    for (std::size_t index = 0; index < preview_rules.size(); ++index)
        preview_rules.rule(index) = rule;
    rule_browser::State browser;
    browser.enabled = true;
    auto& program = data.add_element<ComputeProgramData>("neural_ca", "neural_mnca/ca.cs")->get_program();
    std::vector<std::unique_ptr<Step>> options;
    auto normal_step = std::make_unique<ComputeShaderStep>(program, "normal",
        std::vector<ComputeShaderInput>{{0, "data_texture"}, {4, "neural_rules"}},
        std::vector<ComputeShaderInput>{{2, "data_texture"}});
    normal_step->set_data_work_group("data_texture");
    options.push_back(std::move(normal_step));
    auto browser_step = std::make_unique<ComputeShaderStep>(program, "browser",
        std::vector<ComputeShaderInput>{{0, "rule_browser_state"}, {4, "rule_browser_rules"}},
        std::vector<ComputeShaderInput>{{2, "rule_browser_state"}});
    browser_step->set_data_work_group("rule_browser_state");
    options.push_back(std::move(browser_step));
    SelectStep step([&] { return browser.enabled ? 1u : 0u; }, std::move(options));
    REQUIRE(program.is_linked());
    auto& time_step = float_option(program, "step_size");
    time_step.value = 0.2f;
    time_step.changed = true;
    auto& offset = float_option(program, "modulation_offset");
    offset.value = 0.75f;
    offset.changed = true;
    float_option(program, "diffusion").value = 0.0f;
    GLSL::BoolOption* modulation = nullptr;
    for (auto* option : program.get_options()) {
        if (option->name == "DO_MODULATION") {
            REQUIRE(option->type == GLSL::IOption::Type::Bool);
            REQUIRE(option->compile_constant);
            modulation = static_cast<GLSL::BoolOption*>(option);
        }
    }
    REQUIRE(modulation != nullptr);
    modulation->value = false;
    modulation->changed = true;
    // The first dispatch is a browser dispatch; no normal branch or controller has used the program.
    step.do_step(data);
    require_close(read_texture(previews), uniform_field(rule_browser::browser_count, 0.51f));
    CHECK(read_texture(main) == initial_main);
    CHECK(read_texture(main.get_last_texture()) == initial_main);

    modulation->value = true;
    modulation->changed = true;
    step.do_step(data);
    const float modulated_red = 0.51f + 0.05f * 0.2f * (3.0f * 0.01f * 0.01f + 0.75f);
    require_close(read_texture(previews), uniform_field(rule_browser::browser_count, modulated_red));
    CHECK(read_texture(main) == initial_main);

    modulation->value = false;
    modulation->changed = true;
    step.do_step(data);
    require_close(read_texture(previews), uniform_field(rule_browser::browser_count, modulated_red + 0.01f));
    const auto frozen_previews = read_texture(previews);
    const auto frozen_back = read_texture(previews.get_last_texture());
    browser.enabled = false;
    step.do_step(data);
    require_close(read_texture(main), uniform_field(1, 0.51f));
    CHECK(read_texture(main.get_last_texture()) == initial_main);
    CHECK(read_texture(previews) == frozen_previews);
    CHECK(read_texture(previews.get_last_texture()) == frozen_back);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Moved neural setup preserves borrowed state and routes controls to the active browser mode", "[neural][browser][opengl]") {
    BrowserTestContext context;
    rule_browser::State* original_state = nullptr;
    auto simulation = [&] {
        auto source = setup_neural_mnca();
        original_state = &source.data().get_element<rule_browser::State>("rule_browser");
        auto moved = std::move(source);
        REQUIRE(&moved.data().get_element<rule_browser::State>("rule_browser") == original_state);
        return moved;
    }();
    auto& data = simulation.data();
    auto& browser = data.get_element<rule_browser::State>("rule_browser");
    REQUIRE(&browser == original_state);
    auto& main_rules = data.get_element<neural::RuleModel>("neural_rules");
    auto& preview_rules = data.get_element<neural::RuleModel>("rule_browser_rules");
    auto& main = data.get_element<TextureData>("data_texture");
    auto& previews = data.get_element<TextureData>("rule_browser_state");
    auto& display = data.get_element<TextureData>("rule_browser_display");
    REQUIRE(main.width() == 1024);
    REQUIRE(main.height() == 1024);
    REQUIRE(main.depth() == 1);
    REQUIRE(previews.width() == rule_browser::preview_size);
    REQUIRE(previews.height() == rule_browser::preview_size);
    REQUIRE(previews.depth() == rule_browser::browser_count);
    REQUIRE(display.width() == main.width());
    REQUIRE(display.height() == main.height());
    REQUIRE(display.depth() == 1);
    auto* engine = dynamic_cast<ComputeShaderEngine*>(&simulation.engine());
    REQUIRE(engine != nullptr);
    REQUIRE(engine->compute_steps().empty());
    auto& program = data.get_element<ComputeProgramData>("neural_ca").get_program();
    REQUIRE(program.is_linked());
    float_option(program, "step_size").value = 0.1f;
    auto& initializer = data.get_element<FragmentProgramData>("init_mnca").get_program();
    configure_initializer(initializer);
    const auto frame = [&](bool advance) {
        ImGui::NewFrame();
        for (auto& control : simulation.controllers())
            control->update(data);
        if (advance) {
            engine->step(data);
            for (auto& control : simulation.controllers())
                control->post_process(data);
        }
        for (auto& control : simulation.controllers())
            control->draw();
        ImGui::EndFrame();
    };
    const auto press = [&](int key) {
        ImGui::GetIO().KeysDown[key] = true;
        frame(false);
        ImGui::GetIO().KeysDown[key] = false;
        frame(false);
    };
    const auto bound_buffer = [] {
        GLint value = 0;
        glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 4, &value);
        REQUIRE(value != 0);
        return static_cast<GLuint>(value);
    };
    const auto read_buffer = [](GLuint buffer, int count) {
        GLint64 size = 0;
        glGetNamedBufferParameteri64v(buffer, GL_BUFFER_SIZE, &size);
        REQUIRE(size >= static_cast<GLint64>(count * sizeof(float)));
        std::vector<float> values(count);
        glGetNamedBufferSubData(buffer, 0, count * sizeof(float), values.data());
        return values;
    };

    frame(false);
    const auto initialized_main = read_texture(main);
    main_rules.bind(4);
    const auto main_buffer = bound_buffer();
    const auto original_rule = read_buffer(main_buffer, neural::coefficient_count);
    const auto baseline = neural::pack_rule(neural::baseline_rule());
    REQUIRE(std::equal(baseline.begin(), baseline.end(), original_rule.begin()));
    const auto marker = marker_field(main.width(), main.height(), 0.375f);
    write_texture(main, marker);
    press('B');
    REQUIRE(browser.enabled);
    const auto initial_previews = read_texture(previews);
    CHECK(read_texture(main) == marker);
    frame(true);
    CHECK(browser.steps == 1);
    REQUIRE(bound_buffer() != main_buffer);
    const auto browser_buffer = bound_buffer();
    const auto browser_rules = read_buffer(browser_buffer, rule_browser::browser_count * neural::coefficient_count);
    CHECK(std::equal(original_rule.begin(), original_rule.end(), browser_rules.begin()));
    const auto evolved_previews = read_texture(previews);
    REQUIRE(evolved_previews != initial_previews);
    CHECK(read_texture(main) == marker);
    const size_t layer_size = previews.width() * previews.height() * 4;
    CHECK_FALSE(std::equal(evolved_previews.begin(), evolved_previews.begin() + layer_size,
                           evolved_previews.begin() + layer_size));

    // Browser clicks, mutation, reset, and clear must not reach normal painting or initialization.
    auto& io = ImGui::GetIO();
    io.MousePos = ImVec2(750, 550);
    frame(false);
    io.MouseDown[0] = true;
    frame(false);
    io.MouseDown[0] = false;
    frame(false);
    CHECK(read_texture(main) == marker);
    press('V');
    CHECK(browser.generation == 2);
    CHECK(read_texture(main) == marker);
    CHECK(read_buffer(main_buffer, neural::coefficient_count) == original_rule);
    press('Q');
    const auto reset_previews = read_texture(previews);
    CHECK(reset_previews == initial_previews);
    CHECK(read_texture(main) == marker);
    press('P');
    CHECK(read_texture(main) == marker);
    CHECK(read_buffer(main_buffer, neural::coefficient_count) == original_rule);
    const auto reset_back = read_texture(previews.get_last_texture());
    press('B');
    REQUIRE_FALSE(browser.enabled);
    frame(true);
    REQUIRE(bound_buffer() == main_buffer);
    CHECK(read_texture(main) != marker);
    CHECK(read_texture(previews) == reset_previews);
    CHECK(read_texture(previews.get_last_texture()) == reset_back);

    // The original Q/V behavior remains available in normal mode.
    write_texture(main, marker);
    press('Q');
    const auto reset_main = read_texture(main);
    CHECK(reset_main != marker);
    CHECK(read_texture(previews) == reset_previews);
    const auto before_mutation = read_buffer(main_buffer, neural::coefficient_count);
    press('V');
    CHECK(main_rules.dirty);
    main_rules.bind(4);
    CHECK_FALSE(main_rules.dirty);
    CHECK(read_buffer(main_buffer, neural::coefficient_count) != before_mutation);
    CHECK(read_texture(main) != reset_main);
    CHECK(read_texture(previews) == reset_previews);
    CHECK(read_texture(previews.get_last_texture()) == reset_back);
    CHECK(read_texture(display) == read_texture(main));
    write_texture(main, marker);
    io.MousePos = ImVec2(750, 550);
    frame(false);
    io.MouseDown[0] = true;
    frame(false);
    CHECK(read_texture(main) != marker);
    io.MouseDown[0] = false;
    frame(false);
    CHECK(read_texture(previews) == reset_previews);

    // Apply through the real controls while paused, then let normal controls update once.
    press('B');
    io.MousePos = ImVec2(720, 520);
    frame(false);
    io.MouseDown[0] = true;
    frame(false);
    io.MouseDown[0] = false;
    frame(false);
    preview_rules.bind(4);
    const auto candidates = read_buffer(bound_buffer(), rule_browser::browser_count * neural::coefficient_count);
    const std::vector<float> selected_rule(candidates.begin() + 15 * neural::coefficient_count,
                                           candidates.begin() + 16 * neural::coefficient_count);
    open_browser_controls([&] { frame(false); });
    auto* controls = ImGui::FindWindowByName("Rule browser controls");
    REQUIRE(controls != nullptr);
    REQUIRE(controls->Active);
    const auto& style = ImGui::GetStyle();
    io.MousePos = ImVec2(controls->DC.CursorStartPos.x + ImGui::CalcTextSize("Reset previews").x
        + 3 * style.FramePadding.x + style.ItemSpacing.x + ImGui::CalcTextSize("Use selected rule").x * 0.5f,
        controls->DC.CursorStartPos.y + 2 * ImGui::GetTextLineHeightWithSpacing()
        + ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight() * 0.5f);
    frame(false);
    io.MouseDown[0] = true;
    frame(false);
    io.MouseDown[0] = false;
    frame(false);
    REQUIRE_FALSE(browser.enabled);
    CHECK(main_rules.dirty);
    const auto applied_rule = neural::pack_rule(main_rules.rule());
    CHECK(std::equal(applied_rule.begin(), applied_rule.end(), selected_rule.begin()));
    require_close(read_texture(main), initialized_main);
    const auto applied_main = read_texture(main);
    frame(false);
    CHECK(main_rules.dirty);
    CHECK(&data.get_element<rule_browser::State>("rule_browser") == original_state);
    main_rules.bind(4);
    CHECK_FALSE(main_rules.dirty);
    CHECK(bound_buffer() == main_buffer);
    CHECK(read_buffer(main_buffer, neural::coefficient_count) == selected_rule);
    // A mouse press on Use selected rule must never turn into a normal paint stroke.
    CHECK(read_texture(main) == applied_main);
    CHECK(read_texture(display) == applied_main);
    CHECK(read_texture(previews) == reset_previews);
    CHECK_FALSE(io.MouseDown[0]);
    CHECK(glGetError() == GL_NO_ERROR);
}
