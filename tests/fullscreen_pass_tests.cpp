#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "data.hpp"
#include "engines/compute_shader_engine.hpp"
#include "engines/fragment_shader_engine.hpp"
#include "controllers/init_texture_control.hpp"
#include "controllers/paint_control.hpp"
#include "imgui.h"
#include "viewer.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

bool InitOpenGLForTesting(GLFWwindow** window, std::string& error);

namespace {
class FullscreenContext {
public:
    FullscreenContext() {
        std::string error;
        if (!InitOpenGLForTesting(&window_, error))
            SKIP("OpenGL unavailable: " << error);
    }
    ~FullscreenContext() {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
private:
    GLFWwindow* window_ = nullptr;
};

class BarrierLog {
public:
    BarrierLog() : original_(glMemoryBarrier) {
        active_ = this;
        glMemoryBarrier = record;
    }
    ~BarrierLog() {
        glMemoryBarrier = original_;
        active_ = nullptr;
    }
    std::vector<GLbitfield> calls;
private:
    static void GLAPIENTRY record(GLbitfield bits) {
        active_->calls.push_back(bits);
        active_->original_(bits);
    }
    static inline BarrierLog* active_ = nullptr;
    PFNGLMEMORYBARRIERPROC original_;
};

constexpr int width = 5;
constexpr int height = 3;

TextureData& add_texture(Data& data, const std::string& name) {
    TextureOptions options;
    options.target = GL_TEXTURE_2D_ARRAY;
    auto& texture = *data.add_element<TextureData>(name, width, height, 1, options);
    std::vector<float> pixels(width * height * 4);
    for (int i = 0; i < width * height; ++i) {
        pixels[i * 4] = 0.1f + i * 0.03f;
        pixels[i * 4 + 1] = 0.6f;
        pixels[i * 4 + 2] = 0.2f;
        pixels[i * 4 + 3] = 1.0f;
    }
    for (auto* target : {&texture.get_texture(), &texture.get_last_texture()})
        glTextureSubImage3D(target->gl_tex_num, 0, 0, 0, 0, width, height, 1,
                            GL_RGBA, GL_FLOAT, pixels.data());
    return texture;
}

std::vector<float> read_texture(TextureData& data) {
    std::vector<float> pixels(width * height * 4);
    glGetTextureImage(data.get_texture().gl_tex_num, 0, GL_RGBA, GL_FLOAT,
                      static_cast<GLsizei>(pixels.size() * sizeof(float)), pixels.data());
    return pixels;
}

// Keep each hazard independent so a missing reset cannot hide behind another one.
void poison_raster(const std::string& state) {
    if (state == "viewport") glViewport(7, 11, 1, 1);
    if (state == "scissor") { glEnable(GL_SCISSOR_TEST); glScissor(0, 0, 0, 0); }
    if (state == "mask") glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    if (state == "blend") { glEnable(GL_BLEND); glBlendFunc(GL_ZERO, GL_ZERO); }
    if (state == "depth") { glEnable(GL_DEPTH_TEST); glDepthFunc(GL_NEVER); }
    if (state == "stencil") { glEnable(GL_STENCIL_TEST); glStencilFunc(GL_NEVER, 0, ~0u); }
    if (state == "cull") { glEnable(GL_CULL_FACE); glCullFace(GL_FRONT_AND_BACK); }
    if (state == "discard") glEnable(GL_RASTERIZER_DISCARD);
    if (state == "logic") { glEnable(GL_COLOR_LOGIC_OP); glLogicOp(GL_CLEAR); }
    if (state == "srgb") glEnable(GL_FRAMEBUFFER_SRGB);
    if (state == "polygon") glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

class HostileBindings {
public:
    HostileBindings() : program_("basic.vs", "viewer.fs") {
        REQUIRE(program_.use());
        glGenVertexArrays(1, &vao_);
        glGenSamplers(1, &sampler_);
        // A mipmap sampler makes the single-level source incomplete if inherited.
        glSamplerParameteri(sampler_, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glGenTextures(1, &texture_);
    }
    void apply() {
        REQUIRE(program_.use());
        glBindVertexArray(vao_);
        for (GLuint unit : {0u, 1u}) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D_ARRAY, texture_);
            glBindSampler(unit, sampler_);
        }
        glActiveTexture(GL_TEXTURE7);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_NONE);
    }
    ~HostileBindings() {
        glDeleteSamplers(1, &sampler_);
        glDeleteTextures(1, &texture_);
        glDeleteVertexArrays(1, &vao_);
    }
private:
    FragmentProgram program_;
    GLuint vao_ = 0, sampler_ = 0, texture_ = 0;
};

void check_transform(const std::vector<float>& actual, const std::vector<float>& source) {
    REQUIRE(actual.size() == source.size());
    for (size_t i = 0; i < actual.size(); ++i) {
        CAPTURE(i);
        const float expected = i % 4 == 3 ? 1.0f : std::pow(source[i] * 0.7f, 1.0f / 2.2f);
        CHECK(std::abs(actual[i] - expected) < 0.00001f);
    }
}

std::array<unsigned char, 4> read_back() {
    std::array<unsigned char, 4> pixel{};
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    return pixel;
}
}

TEST_CASE("Fragment passes establish their own fullscreen state", "[fullscreen][opengl]") {
    const auto legacy = GENERATE(false, true);
    const std::string state = GENERATE("viewport", "scissor", "mask", "blend", "depth",
                                       "stencil", "cull", "discard", "logic", "srgb", "polygon", "bindings");
    CAPTURE(legacy, state);
    FullscreenContext context;
    Data data;
    auto& texture = add_texture(data, "image");
    auto& program = data.add_element<FragmentProgramData>("basic.fs", "basic.fs")->get_program();
    REQUIRE(program.is_linked());
    const auto source = read_texture(texture);
    FragmentShaderStep step("basic.fs", {}, "image", 1);
    FragmentShaderEngine engine("basic.fs", "image");
    HostileBindings bindings;
    if (state == "bindings") bindings.apply();
    else poison_raster(state);
    if (legacy) engine.step(data);
    else step.do_step(data);
    check_transform(read_texture(texture), source);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Fragment texture initialization ignores incoming fullscreen state", "[fullscreen][opengl]") {
    const std::string state = GENERATE("viewport", "scissor", "mask", "blend", "depth",
                                       "stencil", "cull", "discard", "logic", "srgb", "polygon", "bindings");
    CAPTURE(state);
    FullscreenContext context;
    Data data;
    auto& baseline = add_texture(data, "baseline");
    auto& actual = add_texture(data, "actual");
    const std::string shader = "fluidMNCA/init_mnca.fs";
    REQUIRE(data.add_element<FragmentProgramData>(shader, shader)->get_program().is_linked());
    InitTextureControl clean(shader, "baseline");
    InitTextureControl hostile(shader, "actual");
    clean.update(data);
    const auto expected = read_texture(baseline);
    REQUIRE(expected != read_texture(actual));
    HostileBindings bindings;
    if (state == "bindings") bindings.apply();
    else poison_raster(state);
    hostile.update(data);
    const auto pixels = read_texture(actual);
    REQUIRE(pixels.size() == expected.size());
    for (size_t i = 0; i < pixels.size(); ++i) {
        CAPTURE(i);
        CHECK(std::abs(pixels[i] - expected[i]) < 0.00001f);
    }
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Viewer output is independent of prior fullscreen work and state", "[fullscreen][opengl]") {
    const std::string state = GENERATE("viewport", "scissor", "mask", "blend", "depth",
                                       "stencil", "cull", "discard", "logic", "srgb", "polygon", "bindings", "pass");
    CAPTURE(state);
    FullscreenContext context;
    Data data;
    auto& source = add_texture(data, "image");
    // Uniform source makes the 1x1 default framebuffer independent of filtering.
    const std::array<float, 4> color{0.25f, 0.5f, 0.75f, 1.0f};
    glClearTexImage(source.get_texture().gl_tex_num, 0, GL_RGBA, GL_FLOAT, color.data());
    Viewer baseline(width, height, 1, "viewer.fs", "basic.fs", GL_RGBA32F, "image");
    Viewer actual(width, height, 1, "viewer.fs", "basic.fs", GL_RGBA32F, "image");
    REQUIRE(baseline.program->is_linked());
    REQUIRE(baseline.output_program->is_linked());
    REQUIRE(actual.program->is_linked());
    REQUIRE(actual.output_program->is_linked());
    baseline.view(1, 1, data);
    const auto expected = read_back();
    for (int i = 0; i < 3; ++i) {
        const int transformed = std::lround(std::pow(color[i] * 0.5f * 0.7f, 1.0f / 2.2f) * 255.0f);
        REQUIRE(std::abs(static_cast<int>(expected[i]) - transformed) <= 1);
    }
    auto& unrelated = add_texture(data, "unrelated");
    (void)unrelated;
    data.add_element<FragmentProgramData>("basic.fs", "basic.fs");
    FragmentShaderStep previous("basic.fs", {}, "unrelated", 1);
    HostileBindings bindings;
    if (state == "pass") previous.do_step(data);
    else if (state == "bindings") bindings.apply();
    else poison_raster(state);
    actual.view(1, 1, data);
    CHECK(read_back() == expected);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Viewer preserves spatial pixels and only applies its display transform", "[fullscreen][opengl]") {
    const float smoothing = GENERATE(0.5f, 1.0f);
    FullscreenContext context;
    glfwSetWindowSize(glfwGetCurrentContext(), width, height);
    int display_width = 0, display_height = 0;
    for (int attempt = 0; attempt < 50; ++attempt) {
        glfwGetFramebufferSize(glfwGetCurrentContext(), &display_width, &display_height);
        if (display_width >= width && display_height >= height)
            break;
        glfwWaitEventsTimeout(0.01);
    }
    REQUIRE(display_width >= width);
    REQUIRE(display_height >= height);

    Data data;
    auto& texture = add_texture(data, "image");
    auto source = read_texture(texture);
    std::vector<float> previous(source.size(), 0.0f);
    Viewer viewer(width, height, 1, "viewer.fs", "basic.fs", GL_RGBA32F, "image");
    const auto options = viewer.program->get_options();
    const auto option = std::find_if(options.begin(), options.end(), [](GLSL::IOption* value) {
        return value->name == "smoothing";
    });
    REQUIRE(option != options.end());
    REQUIRE((*option)->type == GLSL::IOption::Type::Float);
    static_cast<GLSL::ValueOption<float>*>(*option)->value = smoothing;

    for (int frame = 0; frame < 4; ++frame) {
        if (frame == 2) {
            for (size_t i = 0; i < source.size(); ++i) {
                if (i % 4 != 3)
                    source[i] = 1.0f - source[i];
            }
            glTextureSubImage3D(texture.get_texture().gl_tex_num, 0, 0, 0, 0,
                                width, height, 1, GL_RGBA, GL_FLOAT, source.data());
        }
        for (size_t i = 0; i < previous.size(); ++i)
            previous[i] = previous[i] * (1.0f - smoothing) + source[i] * smoothing;

        viewer.view(display_width, display_height, data);
        std::vector<unsigned char> pixels(display_width * display_height * 4);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, display_width, display_height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        int max_error = 0;
        for (int y = 0; y < display_height; ++y) {
            for (int x = 0; x < display_width; ++x) {
                const int sx = static_cast<int>((x + 0.5f) * width / display_width);
                const int sy = static_cast<int>((y + 0.5f) * height / display_height);
                for (int channel = 0; channel < 3; ++channel) {
                    const int expected = std::lround(std::pow(
                        previous[(sy * width + sx) * 4 + channel] * 0.7f, 1.0f / 2.2f) * 255.0f);
                    const int actual = pixels[(y * display_width + x) * 4 + channel];
                    max_error = std::max(max_error, std::abs(actual - expected));
                }
            }
        }
        CAPTURE(frame, smoothing, display_width, display_height, max_error);
        CHECK(max_error <= 1);
        CHECK(read_texture(texture) == source);
        CHECK(glGetError() == GL_NO_ERROR);
    }
}

TEST_CASE("Viewer displays compute painting without a CA pass", "[fullscreen][painting][opengl]") {
    FullscreenContext context;
    auto gui = std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)>(
        ImGui::CreateContext(), &ImGui::DestroyContext);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(512, 512);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->Build();

    constexpr int side = 16;
    Data data;
    auto* texture = data.add_element<TextureData>("image", side, side, 1);
    PaintControl paint("fluidMNCA/paint.cs", "image");
    Viewer viewer(side, side, 1, "viewer.fs", "basic.fs", GL_RGBA32F, "image");
    std::vector<float> expected(side * side, 0.0f);
    float displayed = 0.0f;

    // Repeated strokes exercise both ping-pong textures after they have been sampled.
    for (int frame = 0; frame < 8; ++frame) {
        const float cursor_x = frame % 2 ? 0.625f : 0.375f;
        const bool erase = frame >= 4;
        io.MousePos = ImVec2(cursor_x * 512.0f, 256.0f);
        io.MouseDown[0] = !erase;
        io.MouseDown[1] = erase;
        ImGui::NewFrame();
        paint.update(data);
        ImGui::EndFrame();

        const float start_x = frame == 0 ? cursor_x : (frame % 2 ? 0.375f : 0.625f);
        for (int y = 0; y < side; ++y) {
            for (int x = 0; x < side; ++x) {
                const float closest_x = std::clamp(x + 0.5f,
                    std::min(start_x, cursor_x) * side, std::max(start_x, cursor_x) * side);
                const float dx = x + 0.5f - closest_x;
                const float dy = y + 0.5f - side * 0.5f;
                const float amount = std::max(1.0f - std::sqrt(dx * dx + dy * dy) / 10.0f, 0.0f);
                float& value = expected[y * side + x];
                value = value * (1.0f - amount) + (erase ? 0.0f : amount);
            }
        }
        displayed = 0.5f * displayed + 0.5f * expected[(side / 2) * side + side / 2];
        BarrierLog barriers;
        viewer.view(1, 1, data);
        REQUIRE(std::any_of(barriers.calls.begin(), barriers.calls.end(), [](GLbitfield bits) {
            return (bits & GL_TEXTURE_FETCH_BARRIER_BIT) != 0;
        }));
        const auto pixel = read_back();
        const int target = std::lround(std::pow(displayed * 0.7f, 1.0f / 2.2f) * 255.0f);
        CAPTURE(frame, erase, target);
        for (int channel = 0; channel < 3; ++channel)
            CHECK(std::abs(static_cast<int>(pixel[channel]) - target) <= 1);
        CHECK(glGetError() == GL_NO_ERROR);
    }
}
