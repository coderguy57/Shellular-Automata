#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "automata/neural/rule.hpp"
#include "automata/neural/rule_model.hpp"
#include "automata/rule_browser.hpp"
#include "automata/slack/rule.hpp"
#include "automata/slack/rule_model.hpp"
#include "controllers/neural_rule_control.hpp"
#include "controllers/slack_rule_control.hpp"
#include "controllers/init_texture_control.hpp"
#include "data.hpp"
#include "slack_fragment_reference.hpp"
#include "engines/compute_shader_engine.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "setup/setups.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

bool InitOpenGLForTesting(GLFWwindow** window, std::string& error);

namespace {
constexpr int width = 64;
constexpr int height = 32;
constexpr const char* ca_shader = "slack_automata/slack_automata.fs";
constexpr const char* ca_compute_shader = "slack_automata/slack_automata.cs";
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

std::vector<slack::Rule> snapshot(const slack::RuleModel& model) {
    std::vector<slack::Rule> result;
    for (std::size_t index = 0; index < model.size(); ++index)
        result.push_back(model.rule(index));
    return result;
}

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

std::vector<float> read_texture(const Texture& texture) {
    std::vector<float> pixels(texture.width * texture.height * texture.depth * 4);
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    glGetTextureImage(texture.gl_tex_num, 0, GL_RGBA, GL_FLOAT,
                      static_cast<GLsizei>(pixels.size() * sizeof(float)), pixels.data());
    return pixels;
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

void require_close(const std::vector<float>& actual, const std::vector<float>& expected) {
    REQUIRE(actual.size() == expected.size());
    float error = 0.0f;
    bool finite = true;
    for (size_t i = 0; i < actual.size(); ++i) {
        finite &= std::isfinite(actual[i]);
        error = std::max(error, std::abs(actual[i] - expected[i]));
    }
    REQUIRE(finite);
    REQUIRE(error <= 1.0e-6f);
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

    const auto rule = slack::baseline_rule();
    const auto prepare = [&](FragmentProgram& program) {
        slack_test::bind_rule(program, rule);
        program.set_uniform("cmd", 0u);
        program.set_uniform("frames", 1u);
    };
    FragmentShaderStep legacy_step("legacy", {}, "legacy texture", 1, prepare);
    FragmentShaderStep current_step("current", {}, "current texture", 1, prepare);
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
    slack::RuleModel rule_state;
    SlackRuleControl mutation(rule_state);
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
        REQUIRE(program.use());
        slack_test::bind_rule(program, rule_state.rule());
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

TEST_CASE("Slack editor uses model mutation and restores the complete saved rule", "[slack][rules][opengl]") {
    SlackTestContext context;
    ImGui::GetIO().Fonts->Build();
    Data data;
    slack::RuleModel state;
    state.reseed(55119u);
    state.rule().scale = 117.0f;
    state.rule().zoom = 0.4f;
    state.rule().mode = 2;
    SlackRuleControl control(state);
    const auto shortcut = [&](int key) {
        auto& io = ImGui::GetIO();
        io.KeysDown[key] = true;
        ImGui::NewFrame();
        io.WantCaptureKeyboard = false;
        io.WantTextInput = false;
        control.draw();
        ImGui::EndFrame();
        io.KeysDown[key] = false;
        ImGui::NewFrame();
        control.draw();
        ImGui::EndFrame();
    };
    shortcut('S');
    const auto saved = state.rule();
    auto expected = saved;
    std::mt19937 expected_random(55119u);
    REQUIRE(slack::mutate_rule(expected, expected_random, state.mutation.strength, state.mutation.rate));
    state.dirty = false;
    glUseProgram(0);
    shortcut('V');
    CHECK(state.rule() == expected);
    CHECK(state.dirty);
    control.update(data);
    CHECK(state.dirty);
    GLint active_program = -1;
    glGetIntegerv(GL_CURRENT_PROGRAM, &active_program);
    CHECK(active_program == 0);

    state.rule().scale = 3.0f;
    state.rule().zoom = 0.8f;
    state.rule().mode = 0;
    shortcut('L');
    CHECK(state.rule() == saved);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Rule editors share model actions without changing GPU bindings", "[neural][slack][rules][opengl]") {
    const bool use_slack = GENERATE(false, true);
    CAPTURE(use_slack);
    SlackTestContext context;
    auto& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800, 900);
    io.Fonts->Build();
    Data data;
    FragmentProgram program("basic.vs", ca_shader);
    REQUIRE(program.use());
    Buffer<float> sentinel(std::vector<float>{0.125f});
    sentinel.set_layout(4);
    GLint expected_buffer = 0;
    glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 4, &expected_buffer);
    REQUIRE(expected_buffer != 0);

    const auto exercise = [&]<typename Model, typename Control>(const char* label,
                                                              auto mutate, auto randomize) {
        Model session;
        const auto initial = session.rule();
        session.reseed(77191u);
        Control control(session);
        const auto frame = [&] {
            ImGui::NewFrame();
            control.draw();
            ImGui::EndFrame();
            control.update(data);
            control.post_process(data);
            GLint active_program = 0;
            GLint active_buffer = 0;
            glGetIntegerv(GL_CURRENT_PROGRAM, &active_program);
            glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 4, &active_buffer);
            CHECK(active_program == static_cast<GLint>(program.get_id()));
            CHECK(active_buffer == expected_buffer);
        };
        const auto click = [&](ImVec2 position) {
            io.MousePos = position;
            frame();
            io.MouseDown[0] = true;
            frame();
            io.MouseDown[0] = false;
            frame();
        };
        const auto shortcut = [&](int key) {
            io.KeysDown[key] = true;
            frame();
            io.KeysDown[key] = false;
            frame();
        };
        frame();
        auto* bar = ImGui::FindWindowByName("##MainMenuBar");
        REQUIRE(bar != nullptr);
        click(ImVec2(bar->Pos.x + bar->DC.MenuBarOffset.x * 0.5f,
                     bar->MenuBarRect().GetCenter().y));
        auto* menu = ImGui::FindWindowByName("##Menu_00");
        REQUIRE(menu != nullptr);
        REQUIRE(menu->Active);
        click(ImVec2(menu->WorkRect.GetCenter().x,
                     menu->DC.CursorStartPos.y + ImGui::GetFontSize() * 0.5f));
        const auto window_name = std::string(label) + " controls";
        const auto button = [&](int column) {
            auto* window = ImGui::FindWindowByName(window_name.c_str());
            REQUIRE(window != nullptr);
            REQUIRE(window->Active);
            REQUIRE_FALSE(window->Hidden);
            constexpr std::array<const char*, 3> labels{"Mutate", "Randomize", "Reset rule"};
            const auto& style = ImGui::GetStyle();
            float x = window->DC.CursorStartPos.x;
            for (int index = 0; index < column; ++index)
                x += ImGui::CalcTextSize(labels[index]).x + 2 * style.FramePadding.x + style.ItemSpacing.x;
            return ImVec2(x + ImGui::CalcTextSize(labels[column]).x * 0.5f + style.FramePadding.x,
                          window->DC.CursorStartPos.y + ImGui::GetFrameHeight() * 0.5f);
        };

        session.dirty = false;
        shortcut('S');
        CHECK_FALSE(session.dirty);
        auto expected = initial;
        std::mt19937 expected_random(77191u);
        REQUIRE(mutate(expected, expected_random, session.mutation.strength, session.mutation.rate));
        click(button(0));
        CHECK(session.rule() == expected);
        CHECK(session.dirty);
        shortcut('L');
        CHECK(session.rule() == initial);

        session.mutation.strength = 0.0f;
        session.dirty = false;
        shortcut('V');
        CHECK(session.rule() == initial);
        CHECK_FALSE(session.dirty);
        expected = randomize(expected_random);
        click(button(1));
        CHECK(session.rule() == expected);
        CHECK(session.dirty);
        shortcut('S');
        click(button(2));
        CHECK(session.rule() == initial);
        shortcut('L');
        CHECK(session.rule() == expected);
        CHECK(glGetError() == GL_NO_ERROR);
    };

    if (use_slack)
        exercise.operator()<slack::RuleModel, SlackRuleControl>("Slack rule",
            slack::mutate_rule, slack::random_rule);
    else
        exercise.operator()<neural::RuleModel, NeuralRuleControl>("Neural rule",
            neural::mutate_rule, neural::random_rule);
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

TEST_CASE("Slack batched compute steps match independent fragment passes", "[slack][slack-compute][browser][opengl]") {
    const int layer_count = GENERATE(3, 16);
    CAPTURE(layer_count);
    SlackTestContext context;
    Data data;
    constexpr size_t layer_size = width * height * 4;
    auto& batch = *data.add_element<TextureData>("batch", width, height, layer_count);
    auto& program = data.add_element<ComputeProgramData>("batch_program", ca_compute_shader)->get_program();
    auto& reference_program = data.add_element<FragmentProgramData>("reference_program", ca_shader)->get_program();
    REQUIRE(program.is_linked());
    REQUIRE(reference_program.is_linked());
    std::mt19937 random(77123u);
    std::vector<slack::Rule> rules;
    std::vector<float> fields(layer_size * layer_count);
    const auto base = initial_field();
    for (int layer = 0; layer < layer_count; ++layer) {
        auto rule = layer == 0 ? slack::baseline_rule() : slack::random_rule(random);
        rule.scale = 25.0f + 17.0f * layer;
        rule.mode = layer % 3;
        rule.zoom = (layer % 4) * 0.1f;
        rules.push_back(std::move(rule));
        for (size_t pixel = 0; pixel < layer_size; pixel += 4) {
            for (int channel = 0; channel < 3; ++channel)
                fields[layer * layer_size + pixel + channel] =
                    base[((pixel / 4 + layer * 137) % (width * height)) * 4 + channel];
            fields[layer * layer_size + pixel + 3] = 1.0f;
        }
    }
    write_texture(batch, fields);
    auto& gpu_rules = *data.add_element<slack::RuleModel>("rules", layer_count);
    for (std::size_t layer = 0; layer < rules.size(); ++layer)
        gpu_rules.rule(layer) = rules[layer];
    gpu_rules.upload();
    ComputeShaderStep step(program, "batch_program",
        std::vector<ComputeShaderInput>{{0, "batch"}, {4, "rules"}},
        std::vector<ComputeShaderInput>{{2, "batch"}});
    step.set_data_work_group("batch");
    for (int frame = 0; frame < 3; ++frame) {
        CAPTURE(frame);
        const auto source = read_texture(batch);
        const auto source_id = batch.get_texture().gl_tex_num;
        const auto target_id = batch.get_last_texture().gl_tex_num;
        step.do_step(data);
        CHECK(batch.get_texture().gl_tex_num == target_id);
        CHECK(batch.get_last_texture().gl_tex_num == source_id);
        CHECK(read_texture(batch.get_last_texture()) == source);
        const auto actual = read_texture(batch);
        for (int layer = 0; layer < layer_count; ++layer) {
            CAPTURE(layer);
            const auto name = "reference_" + std::to_string(frame) + "_" + std::to_string(layer);
            const std::vector<float> previous(source.begin() + layer * layer_size,
                                               source.begin() + (layer + 1) * layer_size);
            auto& single = add_texture(data, name, previous);
            FragmentShaderStep reference("reference_program", {}, name, 1,
                [&](FragmentProgram& active) {
                    slack_test::bind_rule(active, rules[layer]);
                    active.set_uniform("layer", 0);
                });
            reference.do_step(data);
            const std::vector<float> observed(actual.begin() + layer * layer_size,
                                               actual.begin() + (layer + 1) * layer_size);
            require_close(observed, read_texture(single));
            CHECK(read_texture(single.get_last_texture()) == previous);
        }
    }
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Slack rule browser isolates previews and promotes into the rule editor", "[slack][browser][opengl]") {
    SlackTestContext context;
    auto& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800, 600);
    io.Fonts->Build();
    auto simulation = setup_slack_automata();
    auto& data = simulation.data();
    auto& browser = data.get_element<rule_browser::State>("rule_browser");
    auto& main_rules = data.get_element<slack::RuleModel>("slack_rules");
    auto& preview_rules = data.get_element<slack::RuleModel>("rule_browser_rules");
    auto& main = data.get_element<TextureData>("data_texture");
    auto& previews = data.get_element<TextureData>("rule_browser_state");
    auto& display = data.get_element<TextureData>("rule_browser_display");
    REQUIRE(main.width() == 512);
    REQUIRE(main.height() == 512);
    REQUIRE(main.depth() == 1);
    REQUIRE(previews.width() == rule_browser::preview_size);
    REQUIRE(previews.height() == rule_browser::preview_size);
    REQUIRE(previews.depth() == rule_browser::browser_count);
    REQUIRE(display.width() == main.width());
    REQUIRE(display.height() == main.height());
    auto& initializer = data.get_element<FragmentProgramData>("init_mnca").get_program();
    REQUIRE(initializer.is_linked());
    for (auto* option : initializer.get_options()) {
        if (option->name == "seed") {
            REQUIRE(option->type == GLSL::IOption::Type::UInt);
            REQUIRE(static_cast<GLSL::ValueOption<uint32_t>*>(option)->value == 93u);
        }
        if (option->name == "scale_r" || option->name == "scale_g" || option->name == "scale_b") {
            REQUIRE(option->type == GLSL::IOption::Type::Float);
            static_cast<GLSL::ValueOption<float>*>(option)->value = 0.1f;
        }
    }
    const auto frame = [&](bool advance) {
        ImGui::NewFrame();
        for (auto& control : simulation.controllers())
            control->update(data);
        if (advance) {
            simulation.engine().step(data);
            for (auto& control : simulation.controllers())
                control->post_process(data);
        }
        for (auto& control : simulation.controllers())
            control->draw();
        ImGui::EndFrame();
    };
    const auto press = [&](int key) {
        io.KeysDown[key] = true;
        frame(false);
        io.KeysDown[key] = false;
        frame(false);
    };
    const auto click = [&](ImVec2 position) {
        io.MousePos = position;
        frame(false);
        io.MouseDown[0] = true;
        frame(false);
        io.MouseDown[0] = false;
        frame(false);
    };
    const auto open_menu = [&](const char* label) -> ImGuiWindow& {
        frame(false);
        auto* bar = ImGui::FindWindowByName("##MainMenuBar");
        REQUIRE(bar != nullptr);
        const auto& style = ImGui::GetStyle();
        click(ImVec2(bar->Pos.x + style.DisplaySafeAreaPadding.x + style.ItemSpacing.x * 0.5f
                     + ImGui::CalcTextSize(label).x * 0.5f,
                     bar->MenuBarRect().GetCenter().y));
        frame(false);
        auto* menu = ImGui::FindWindowByName("##Menu_00");
        REQUIRE(menu != nullptr);
        REQUIRE(menu->Active);
        REQUIRE_FALSE(menu->Hidden);
        return *menu;
    };
    const auto bound_buffer = [] {
        GLint buffer = 0;
        glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING, 4, &buffer);
        REQUIRE(buffer != 0);
        return static_cast<GLuint>(buffer);
    };
    frame(false);
    main_rules.bind(4);
    const auto main_buffer = bound_buffer();
    const auto initial_main = read_texture(main);
    const auto baseline = main_rules.rule();
    REQUIRE(baseline == slack::baseline_rule());
    std::vector<float> marker(main.width() * main.height() * 4, 0.125f);
    for (size_t pixel = 0; pixel < marker.size(); pixel += 4)
        marker[pixel + 3] = 1.0f;
    write_texture(main, marker);
    press('B');
    REQUIRE(browser.enabled);
    REQUIRE(browser.generation == 1);
    REQUIRE(preview_rules.rule(0) == baseline);
    CHECK(read_texture(main) == marker);
    const auto initialized_previews = read_texture(previews);
    const size_t layer_size = previews.width() * previews.height() * 4;
    for (int layer = 1; layer < rule_browser::browser_count; ++layer)
        REQUIRE(std::equal(initialized_previews.begin(), initialized_previews.begin() + layer_size,
                            initialized_previews.begin() + layer * layer_size));
    CHECK(read_texture(previews.get_last_texture()) == initialized_previews);
    frame(true);
    CHECK(browser.steps == 1);
    const auto preview_buffer = bound_buffer();
    CHECK(preview_buffer != main_buffer);
    preview_rules.bind(4);
    CHECK(bound_buffer() == preview_buffer);
    CHECK(read_texture(previews) != initialized_previews);
    CHECK(read_texture(main) == marker);
    CHECK(main_rules.rule() == baseline);

    click(ImVec2(700, 525));
    REQUIRE(browser.selected == 15);
    CHECK(read_texture(main) == marker);
    const auto parent = preview_rules.rule(15);
    press('V');
    REQUIRE(browser.generation == 2);
    REQUIRE(preview_rules.rule(0) == parent);
    CHECK(browser.steps == 0);
    CHECK(read_texture(main) == marker);
    CHECK(main_rules.rule() == baseline);
    const auto generated = snapshot(preview_rules);
    frame(true);
    press('Q');
    CHECK(snapshot(preview_rules) == generated);
    CHECK(browser.steps == 0);
    CHECK(read_texture(previews) == initialized_previews);
    CHECK(read_texture(previews.get_last_texture()) == initialized_previews);
    press('P');
    CHECK(read_texture(main) == marker);
    press('B');
    REQUIRE_FALSE(browser.enabled);
    frame(true);
    CHECK(bound_buffer() == main_buffer);
    CHECK(read_texture(main) != marker);
    CHECK(read_texture(previews) == initialized_previews);
    CHECK(read_texture(previews.get_last_texture()) == initialized_previews);

    // Reenter without regenerating, then promote nondefault metadata through the real UI.
    press('B');
    REQUIRE(browser.enabled);
    CHECK(browser.generation == 2);
    CHECK(snapshot(preview_rules) == generated);
    browser.selected = 3;
    preview_rules.rule(3).scale = 113.0f;
    preview_rules.rule(3).zoom = 0.3f;
    preview_rules.rule(3).mode = 2;
    preview_rules.dirty = true;
    const auto selected = preview_rules.rule(3);
    auto& menu = open_menu("Rule browser");
    click(ImVec2(menu.WorkRect.GetCenter().x,
                 menu.DC.CursorStartPos.y + 5 * ImGui::GetTextLineHeightWithSpacing()
                 + ImGui::GetFontSize() * 0.5f));
    frame(false);
    auto* controls = ImGui::FindWindowByName("Rule browser controls");
    REQUIRE(controls != nullptr);
    REQUIRE(controls->Active);
    const auto& style = ImGui::GetStyle();
    click(ImVec2(controls->DC.CursorStartPos.x + ImGui::CalcTextSize("Reset previews").x
                 + 3 * style.FramePadding.x + style.ItemSpacing.x
                 + ImGui::CalcTextSize("Use selected rule").x * 0.5f,
                 controls->DC.CursorStartPos.y + 2 * ImGui::GetTextLineHeightWithSpacing()
                 + ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight() * 0.5f));
    REQUIRE_FALSE(browser.enabled);
    REQUIRE(main_rules.rule() == selected);
    require_close(read_texture(main), initial_main);
    const auto promoted_main = read_texture(main);
    frame(false);
    CHECK(main_rules.rule() == selected);
    CHECK(read_texture(main) == promoted_main);
    CHECK(read_texture(previews) == initialized_previews);
    REQUIRE_FALSE(io.MouseDown[0]);

    auto& rule_menu = open_menu("Slack rule");
    click(ImVec2(rule_menu.WorkRect.GetCenter().x,
                 rule_menu.DC.CursorStartPos.y + ImGui::GetTextLineHeightWithSpacing()
                 + ImGui::GetFontSize() * 0.5f));
    frame(false);
    REQUIRE_FALSE(browser.enabled);
    auto* editor = ImGui::FindWindowByName("Slack rule controls");
    REQUIRE(editor != nullptr);
    REQUIRE(editor->Active);
    REQUIRE_FALSE(editor->Hidden);
    CHECK(main_rules.rule() == selected);
    // Shared actions, mutation sliders, and seed input precede the rule-specific parameters.
    click(ImVec2(editor->DC.CursorStartPos.x + 1.0f,
                 editor->DC.CursorStartPos.y + 5 * ImGui::GetFrameHeightWithSpacing()
                 + 2 * style.ItemSpacing.y + ImGui::GetFrameHeight() * 0.5f));
    REQUIRE(ImGui::GetCurrentContext()->HoveredId == editor->GetID("Scale"));
    CHECK(main_rules.rule().scale == 0.0f);
    CHECK(main_rules.rule().words == selected.words);
    CHECK(main_rules.rule().zoom == selected.zoom);
    CHECK(main_rules.rule().mode == selected.mode);
    press('S');
    const auto saved = main_rules.rule();
    for (int attempt = 0; attempt < 8 && main_rules.rule().words == saved.words; ++attempt)
        press('V');
    REQUIRE(main_rules.rule().words != saved.words);
    press('L');
    CHECK(main_rules.rule() == saved);
    CHECK(read_texture(previews) == initialized_previews);
    CHECK(read_texture(previews.get_last_texture()) == initialized_previews);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Rule browser promotion preserves the main field until an explicit reset", "[neural][slack][browser][opengl]") {
    const bool use_slack = GENERATE(false, true);
    CAPTURE(use_slack);
    SlackTestContext context;
    auto& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800, 600);
    io.Fonts->Build();
    auto simulation = use_slack ? setup_slack_automata() : setup_neural_mnca();
    auto& data = simulation.data();
    auto* browser = &data.get_element<rule_browser::State>("rule_browser");
    auto& main = data.get_element<TextureData>("data_texture");
    auto& initializer = data.get_element<FragmentProgramData>("init_mnca").get_program();
    REQUIRE(initializer.is_linked());
    for (auto* option : initializer.get_options()) {
        if (option->name == "seed")
            static_cast<GLSL::ValueOption<uint32_t>*>(option)->value = 93u;
        if (option->name == "scale_r" || option->name == "scale_g" || option->name == "scale_b")
            static_cast<GLSL::ValueOption<float>*>(option)->value = 0.1f;
    }
    // All interactions remain paused: only explicitly requested frames run controller updates.
    const auto frame = [&](bool update) {
        ImGui::NewFrame();
        if (update)
            for (auto& control : simulation.controllers())
                control->update(data);
        for (auto& control : simulation.controllers())
            control->draw();
        ImGui::EndFrame();
    };
    const auto press = [&](int key) {
        io.KeysDown[key] = true;
        frame(false);
        io.KeysDown[key] = false;
        frame(false);
    };
    const auto click = [&](ImVec2 position) {
        io.MousePos = position;
        frame(false);
        io.MouseDown[0] = true;
        frame(false);
        io.MouseDown[0] = false;
        frame(false);
    };
    frame(true);
    press('B');
    REQUIRE(browser->enabled);
    browser->selected = 7;

    auto* bar = ImGui::FindWindowByName("##MainMenuBar");
    REQUIRE(bar != nullptr);
    const auto& style = ImGui::GetStyle();
    click(ImVec2(bar->Pos.x + style.DisplaySafeAreaPadding.x + style.ItemSpacing.x * 0.5f
                 + ImGui::CalcTextSize("Rule browser").x * 0.5f,
                 bar->MenuBarRect().GetCenter().y));
    frame(false);
    auto* menu = ImGui::FindWindowByName("##Menu_00");
    REQUIRE(menu != nullptr);
    REQUIRE(menu->Active);
    click(ImVec2(menu->WorkRect.GetCenter().x,
                 menu->DC.CursorStartPos.y + 5 * ImGui::GetTextLineHeightWithSpacing()
                 + ImGui::GetFontSize() * 0.5f));
    frame(false);
    auto* controls = ImGui::FindWindowByName("Rule browser controls");
    REQUIRE(controls != nullptr);
    REQUIRE(controls->Active);
    REQUIRE_FALSE(controls->Hidden);
    click(ImVec2(controls->DC.CursorStartPos.x + ImGui::CalcTextSize("Reset previews").x
                 + 3 * style.FramePadding.x + style.ItemSpacing.x
                 + ImGui::CalcTextSize("Use selected rule").x * 0.5f,
                 controls->DC.CursorStartPos.y + 2 * ImGui::GetTextLineHeightWithSpacing()
                 + ImGui::GetFrameHeightWithSpacing() + ImGui::GetFrameHeight() * 0.5f));
    REQUIRE_FALSE(browser->enabled);
    const auto promoted = read_texture(main);
    frame(true);
    CHECK(read_texture(main) == promoted);
    frame(true);
    CHECK(read_texture(main) == promoted);

    press('Q');
    frame(true);
    const auto reset = read_texture(main);
    REQUIRE(reset != promoted);
    frame(true);
    CHECK(read_texture(main) == reset);
    CHECK(browser->steps == 0);
    CHECK(glGetError() == GL_NO_ERROR);
}
