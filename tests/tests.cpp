#include <catch2/catch_test_macros.hpp>

#include "utils/image_load.hpp"
#include "fftw3.h"
#include "engines/compute_shader_engine.hpp"
#include "engines/fragment_shader_engine.hpp"
#include "render/texture.hpp"
#include "data.hpp"
#include "setup/setups.hpp"
#include "imgui.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

bool InitOpenGLForTesting(GLFWwindow** window, std::string& error) {
    // Set GLFW error callback
    glfwSetErrorCallback(glfw_error_callback);

    // Initialize GLFW
    if (!glfwInit()) {
        error = "Failed to init GLFW";
        return false;
    }

    // Configure OpenGL context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    // Create an invisible window
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Ensure the window is hidden
    *window = glfwCreateWindow(1, 1, "Hidden Context", NULL, NULL);
    if (*window == NULL) {
        glfwTerminate();
        error = "Failed to create hidden window";
        return false;
    }

    // Make the context current
    glfwMakeContextCurrent(*window);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        glfwDestroyWindow(*window);
        glfwTerminate();
        *window = nullptr;
        error = "Failed to init GLEW";
        return false;
    }
    while (glGetError() != GL_NO_ERROR) {}

    // Enable OpenGL debug output
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(GLDebugMessageCallback, 0);

    return true;
}

class OpenGLTestContext {
public:
    OpenGLTestContext() {
        std::string error;
        if (!InitOpenGLForTesting(&window_, error)) {
            SKIP("OpenGL unavailable: " << error);
        }
    }
    ~OpenGLTestContext() {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }

private:
    GLFWwindow* window_ = nullptr;
};

class ImGuiTestContext {
public:
    ImGuiTestContext() {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(512, 512);
        io.DeltaTime = 1.0f / 60.0f;
        io.Fonts->Build();
    }
    ~ImGuiTestContext() {
        ImGui::DestroyContext();
    }
};

static void require_linked_compute_steps(ComputeShaderEngine& engine) {
    for (auto* step : engine.compute_steps()) {
        INFO(step->shader_name());
        REQUIRE(step->program().is_linked());
    }
}

template<typename T>
static std::optional<TextureBuffer<T>> save_texture_directly(
    const Texture& texture, const TextureOptions& options) {
    int channels = 0;
    switch (options.format) {
        case GL_RED: channels = 1; break;
        case GL_RGB: channels = 3; break;
        case GL_RGBA: channels = 4; break;
        default: return std::nullopt;
    }

    TextureBuffer<T> buffer;
    buffer.width = texture.width;
    buffer.height = texture.height;
    buffer.depth = texture.depth;
    buffer.channels = channels;
    const auto bytes = sizeof(T) * buffer.size();
    if (bytes > static_cast<size_t>(std::numeric_limits<GLsizei>::max()))
        return std::nullopt;
    buffer.data = std::make_unique<T[]>(buffer.size());
    glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
    glGetTextureImage(texture.gl_tex_num, 0, options.format, options.type,
                      static_cast<GLsizei>(bytes), buffer.data.get());
    return buffer;
}

static TextureBuffer<float> read_texture(TextureData& texture) {
    auto options = texture.options();
    options.format = GL_RGBA;
    options.type = GL_FLOAT;
    auto buffer = save_texture_directly<float>(texture.get_texture(), options);
    REQUIRE(buffer.has_value());
    return std::move(*buffer);
}

static std::array<double, 3> sum_rgb(const TextureBuffer<float>& buffer) {
    std::array<double, 3> sum{};
    for (int pixel = 0; pixel < buffer.width * buffer.height * buffer.depth; pixel++) {
        for (int channel = 0; channel < 3; channel++) {
            sum[channel] += buffer.data[pixel * buffer.channels + channel];
        }
    }
    return sum;
}

static bool rgb_is_zero(const TextureBuffer<float>& buffer) {
    for (int pixel = 0; pixel < buffer.width * buffer.height * buffer.depth; pixel++) {
        for (int channel = 0; channel < 3; channel++) {
            if (buffer.data[pixel * buffer.channels + channel] != 0.0f) {
                return false;
            }
        }
    }
    return true;
}

static double max_rgb_change(const TextureBuffer<float>& before, const TextureBuffer<float>& after) {
    REQUIRE(before.size() == after.size());
    double change = 0.0;
    for (int pixel = 0; pixel < before.width * before.height * before.depth; pixel++) {
        for (int channel = 0; channel < 3; channel++) {
            const int index = pixel * before.channels + channel;
            change = std::max(change, std::abs(double(after.data[index]) - before.data[index]));
        }
    }
    return change;
}

static ComputeShaderStep* create_compute_shader_step(
    ComputeShaderEngine* engine, 
    std::string shader, 
    std::initializer_list<TextureData*> inputs,
    std::initializer_list<TextureData*> outputs,
    int iterations = 1) 
{
    GLuint i = 0;
    std::vector<ComputeShaderInput> step_inputs;
    for (auto& input : inputs) {
        step_inputs.push_back({i, *input});
        i++;
    }
    std::vector<ComputeShaderInput> step_outputs;
    for (auto& output : outputs) {
        step_outputs.push_back({i, *output});
        i++;
    }
    auto step = engine->add_step<ComputeShaderStep>(shader, step_inputs, step_outputs);
    REQUIRE(step->program().is_linked());
    if (inputs.size() > 0)
        step->set_data_work_group(**inputs.begin());
    else
        step->set_data_work_group(**outputs.begin());
    step->set_iterations(iterations);
    return step;
}

static void create_fft_step(ComputeShaderEngine* engine, TextureData* input, TextureData* output) {
    int size_x = input->get_texture().width;
    int size_y = input->get_texture().height;
    int size_z = input->get_texture().depth;
    create_compute_shader_step(engine, "fft2D/fft.glsl", {input}, {output})
        ->set_option("complex_in", false)
        ->set_option("horizontal", true)
        ->set_option("size", size_x)
        ->set_work_groups(1, size_y, size_z);
    create_compute_shader_step(engine, "fft2D/fft.glsl", {output}, {output})
        ->set_option("horizontal", false)
        ->set_option("size", size_y)
        ->set_work_groups(size_x, 1, size_z);
}

static void create_ifft_step(ComputeShaderEngine* engine, TextureData* input, TextureData* output) {
    int size_x = output->get_texture().width;
    int size_y = output->get_texture().height;
    int size_z = output->get_texture().depth;
    create_compute_shader_step(engine, "fft2D/fft.glsl", {input}, {input})
        ->set_option("horizontal", true)
        ->set_option("inverse", true)
        ->set_option("size", size_x)
        ->set_work_groups(1, size_y, size_z);
    create_compute_shader_step(engine, "fft2D/fft.glsl", {input}, {output})
        ->set_option("horizontal", false)
        ->set_option("inverse", true)
        ->set_option("normalization", size_x * size_y)
        ->set_option("complex_out", false)
        ->set_option("size", size_y)
        ->set_work_groups(size_x, 1, size_z);
}

void do_fftw(const TextureBuffer<float>& buffer, fftwf_complex **fft_result) {
    *fft_result = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * buffer.width * buffer.height);

    // Plan for the FFT
    fftwf_plan plan = fftwf_plan_dft_r2c_2d(buffer.height, buffer.width, buffer.data.get(), *fft_result, FFTW_ESTIMATE);
    fftwf_execute(plan);
    fftwf_destroy_plan(plan);
}

TEST_CASE("GPU FFT round trip preserves image", "[fft][opengl]") {
    OpenGLTestContext context;

    auto texture_data = load_texture_data("tests/res/image.png", 4);

    REQUIRE(texture_data.has_value());
    auto width = texture_data->width;
    auto height = texture_data->height;

    Data data = Data();

    TextureOptions options;
    options.target = GL_TEXTURE_2D_ARRAY;
    auto data_texture = data.add_element<TextureData>("texture", width, height, 1, options);
    glTextureSubImage3D(data_texture->get_texture().gl_tex_num, 0, 0, 0, 0,
                        width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, texture_data->data.get());
    REQUIRE(glGetError() == GL_NO_ERROR);
    auto output_texture = data.add_element<TextureData>("out_texture", data_texture->width(), data_texture->height(), 2, data_texture->options()); 
    std::unique_ptr<ComputeShaderEngine> engine = std::make_unique<ComputeShaderEngine>();
    create_fft_step(engine.get(), data_texture, output_texture);
    create_ifft_step(engine.get(), output_texture, data_texture);
    engine->step(data);
    glFinish();
    require_linked_compute_steps(*engine);
    CHECK(glGetError() == GL_NO_ERROR);

    auto read_options = data_texture->options();
    read_options.format = GL_RGBA;
    read_options.type = GL_FLOAT;
    auto custom_data = save_texture_directly<float>(data_texture->get_texture(), read_options);
    REQUIRE(custom_data.has_value());
    REQUIRE(custom_data->width == width);
    REQUIRE(custom_data->height == height);
    REQUIRE(custom_data->depth == 1);
    REQUIRE(custom_data->channels == 4);

    constexpr float tolerance = 2.0f / 255.0f;
    float max_abs_error = 0.0f;
    double sum_squared_error = 0.0;
    size_t bad_samples = 0;

    for (int pixel = 0; pixel < width * height; pixel++) {
        for (int channel = 0; channel < 3; channel++) {
            const auto input_index = pixel * texture_data->channels + channel;
            const auto output_index = pixel * custom_data->channels + channel;
            const float expected = static_cast<float>(texture_data->data[input_index]) / 255.0f;
            const float actual = custom_data->data[output_index];
            const float error = std::isfinite(actual)
                ? std::abs(actual - expected)
                : std::numeric_limits<float>::infinity();

            max_abs_error = std::max(max_abs_error, error);
            sum_squared_error += static_cast<double>(error) * error;
            if (error > tolerance) {
                bad_samples++;
            }
        }
    }

    const double mean_squared_error = sum_squared_error / static_cast<double>(width * height * 3);
    CAPTURE(max_abs_error, mean_squared_error, bad_samples, tolerance);
    CHECK(bad_samples == 0);
    CHECK(max_abs_error <= tolerance);
}

TEST_CASE("Gradient forces match normalized Scharr derivatives", "[fluidMNCA][opengl]") {
    OpenGLTestContext context;
    constexpr int width = 16;
    constexpr int height = 16;
    constexpr int channels = 4;
    constexpr float force_scale = 2.5f;
    bool horizontal = true;
    SECTION("X gradient") { horizontal = true; }
    SECTION("Y gradient") { horizontal = false; }

    const std::array<int, 9> kernel = horizontal
        ? std::array<int, 9>{-3, 0, 3, -10, 0, 10, -3, 0, 3}
        : std::array<int, 9>{-3, -10, -3, 0, 0, 0, 3, 10, 3};
    Data data;
    // The real setup supplies a two-layer FFT result but updates one velocity layer.
    auto* field = data.add_element<TextureData>("field", width, height, 2);
    auto* velocity = data.add_element<TextureData>("velocity", width, height, 1);
    ComputeShaderEngine engine;
    create_compute_shader_step(&engine,
        horizontal ? "fluidMNCA/gradient_x.cs" : "fluidMNCA/gradient_y.cs",
        {field, velocity}, {velocity})->set_option("Force scale", force_scale);

    for (bool constant : {true, false}) {
        CAPTURE(horizontal, constant);
        std::vector<float> input(width * height * channels * 2, 1000.0f);
        std::vector<float> initial_velocity(width * height * channels);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int channel = 0; channel < channels; ++channel) {
                    const int index = (y * width + x) * channels + channel;
                    input[index] = constant ? channel * 0.25f
                        : ((3 * x + 5 * y + x * y + 7 * channel) % 19) / 8.0f;
                    initial_velocity[index] = (x - y + channel) / 16.0f;
                }
            }
        }
        glTextureSubImage3D(field->get_texture().gl_tex_num, 0, 0, 0, 0,
                            width, height, 2, GL_RGBA, GL_FLOAT, input.data());
        glTextureSubImage3D(velocity->get_texture().gl_tex_num, 0, 0, 0, 0,
                            width, height, 1, GL_RGBA, GL_FLOAT, initial_velocity.data());
        REQUIRE(glGetError() == GL_NO_ERROR);

        engine.step(data);
        const auto actual = read_texture(*velocity);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int channel = 0; channel < channels; ++channel) {
                    float derivative = 0.0f;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            const int sx = (x + dx + width) % width;
                            const int sy = (y + dy + height) % height;
                            derivative += kernel[(dy + 1) * 3 + dx + 1]
                                * input[(sy * width + sx) * channels + channel];
                        }
                    }
                    const int index = (y * width + x) * channels + channel;
                    const float expected = initial_velocity[index] + force_scale * derivative / 32.0f;
                    CAPTURE(x, y, channel, expected, actual.data[index]);
                    CHECK(std::abs(actual.data[index] - expected) < 1.0e-5f);
                }
            }
        }
        CHECK(glGetError() == GL_NO_ERROR);
    }
}

TEST_CASE("Demand flux conserves RGB material", "[fluidMNCA][opengl]") {
    OpenGLTestContext context;

    constexpr int width = 16;
    constexpr int height = 16;
    constexpr int channels = 4;
    std::vector<float> material(width * height * channels);
    std::vector<float> demand(width * height * channels);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const int index = (y * width + x) * channels;
            material[index + 0] = 0.15f + 0.35f * static_cast<float>((x + y) % 5) / 4.0f;
            material[index + 1] = 0.10f + 0.30f * static_cast<float>((2 * x + y) % 7) / 6.0f;
            material[index + 2] = 0.05f + 0.25f * static_cast<float>((x + 3 * y) % 11) / 10.0f;
            material[index + 3] = 1.0f;

            demand[index + 0] = 0.5f - static_cast<float>((3 * x + y) % 9) / 8.0f;
            demand[index + 1] = static_cast<float>((x + 5 * y) % 13) / 12.0f - 0.5f;
            demand[index + 2] = 0.5f - static_cast<float>((7 * x + 2 * y) % 17) / 16.0f;
            demand[index + 3] = 0.0f;
        }
    }

    TextureOptions options;
    options.target = GL_TEXTURE_2D_ARRAY;
    options.internal_format = GL_RGBA32F;
    options.format = GL_RGBA;
    options.type = GL_FLOAT;

    Data data;
    auto data_texture = data.add_element<TextureData>("data_texture", width, height, 1, options);
    glTextureSubImage3D(data_texture->get_texture().gl_tex_num, 0, 0, 0, 0,
                        width, height, 1, GL_RGBA, GL_FLOAT, material.data());
    auto demand_texture = data.add_element<TextureData>("demand_texture", width, height, 1, options);
    glTextureSubImage3D(demand_texture->get_texture().gl_tex_num, 0, 0, 0, 0,
                        width, height, 1, GL_RGBA, GL_FLOAT, demand.data());
    auto total_texture = data.add_element<TextureData>("total_texture", width, height, 1, options);
    REQUIRE(glGetError() == GL_NO_ERROR);

    auto read_options = options;
    auto before = save_texture_directly<float>(data_texture->get_texture(), read_options);
    REQUIRE(before.has_value());
    auto before_sum = sum_rgb(*before);

    ComputeShaderEngine engine;
    create_compute_shader_step(&engine, "fluidMNCA/demand_flux_total.cs", {demand_texture}, {total_texture});
    create_compute_shader_step(&engine, "fluidMNCA/demand_flux.cs", {data_texture, demand_texture, total_texture}, {data_texture});
    for (int frame = 0; frame < 32; frame++) {
        engine.step(data);
    }
    glFinish();
    require_linked_compute_steps(engine);
    CHECK(glGetError() == GL_NO_ERROR);

    auto after = save_texture_directly<float>(data_texture->get_texture(), read_options);
    REQUIRE(after.has_value());
    auto after_sum = sum_rgb(*after);
    CHECK(max_rgb_change(*before, *after) > 1.0e-3);

    constexpr double tolerance = 1.0e-4;
    for (int channel = 0; channel < 3; channel++) {
        CAPTURE(channel, before_sum[channel], after_sum[channel], after_sum[channel] - before_sum[channel]);
        CHECK(std::abs(after_sum[channel] - before_sum[channel]) <= tolerance);
    }
}

TEST_CASE("NN fluid setup moves, paints, and clears material", "[fluidMNCA][opengl]") {
    OpenGLTestContext context;
    ImGuiTestContext gui;
    auto simulation = setup_conservative();
    auto& data = simulation.data();
    auto& texture = data.get_element<TextureData>("data_texture");
    auto& engine = dynamic_cast<ComputeShaderEngine&>(simulation.engine());
    auto& io = ImGui::GetIO();

    auto update_controls = [&] {
        for (auto& controller : simulation.controllers()) {
            controller->update(data);
        }
    };
    auto finish_frame = [&] {
        for (auto& controller : simulation.controllers()) {
            controller->draw();
        }
        ImGui::EndFrame();
    };
    auto frame = [&] {
        ImGui::NewFrame();
        update_controls();
        engine.step(data);
        require_linked_compute_steps(engine);
        REQUIRE(data.get_element<FragmentProgramData>("frag_shader").get_program().is_linked());
        finish_frame();
    };

    ImGui::NewFrame();
    update_controls();
    const auto before = read_texture(texture);
    const auto initial_sum = sum_rgb(before);
    finish_frame();
    for (int channel = 0; channel < 3; channel++) {
        REQUIRE(initial_sum[channel] > 0.0);
    }
    for (int i = 0; i < 4; i++) {
        frame();
    }
    const auto after = read_texture(texture);
    const auto final_sum = sum_rgb(after);
    CHECK(max_rgb_change(before, after) > 1.0e-5);
    for (int channel = 0; channel < 3; channel++) {
        CAPTURE(channel, initial_sum[channel], final_sum[channel]);
        CHECK(std::abs(final_sum[channel] - initial_sum[channel]) < initial_sum[channel] * 1.0e-6);
    }

    // The setup's action binding clears material and velocity independently of mutation.
    io.KeysDown['P'] = true;
    frame();
    io.KeysDown['P'] = false;
    frame();
    const auto cleared = read_texture(texture);
    CHECK(rgb_is_zero(cleared));
    CHECK(rgb_is_zero(read_texture(data.get_element<TextureData>("x_vel_data"))));
    CHECK(rgb_is_zero(read_texture(data.get_element<TextureData>("y_vel_data"))));

    io.MousePos = ImVec2(256, 256);
    io.MouseDown[0] = true;
    frame();
    io.MouseDown[0] = false;
    const auto painted_sum = sum_rgb(read_texture(texture));
    for (int channel = 0; channel < 3; channel++) {
        CHECK(painted_sum[channel] > 0.0);
    }

    io.MouseDown[1] = true;
    frame();
    io.MouseDown[1] = false;
    const auto erased_sum = sum_rgb(read_texture(texture));
    for (int channel = 0; channel < 3; channel++) {
        CHECK(erased_sum[channel] < painted_sum[channel]);
    }

    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Standalone NN MNCA initializes, evolves, and accepts input", "[mnca_nn][opengl]") {
    OpenGLTestContext context;
    ImGuiTestContext gui;
    auto simulation = setup_mnca_nn();
    auto& data = simulation.data();
    auto& texture = data.get_element<TextureData>("data_texture");
    auto& engine = dynamic_cast<ComputeShaderEngine&>(simulation.engine());
    auto& program = data.get_element<FragmentProgramData>("frag_shader").get_program();
    auto& io = ImGui::GetIO();

    size_t texture_count = 0;
    data.for_each_element<TextureData>([&](const std::string& name, TextureData&) {
        CHECK(name == "data_texture");
        ++texture_count;
    });
    REQUIRE(texture_count == 1);
    REQUIRE(texture.width() == 512);
    REQUIRE(texture.height() == 512);
    REQUIRE(texture.depth() == 1);
    REQUIRE(engine.compute_steps().empty());
    REQUIRE(program.is_linked());
    REQUIRE(data.get_element<FragmentProgramData>("init_mnca").get_program().is_linked());

    auto update_controls = [&] {
        for (auto& controller : simulation.controllers()) {
            controller->update(data);
        }
    };
    auto finish_frame = [&] {
        for (auto& controller : simulation.controllers()) {
            controller->draw();
        }
        ImGui::EndFrame();
    };
    bool advance_ca = true;
    auto frame = [&] {
        ImGui::NewFrame();
        update_controls();
        if (advance_ca)
            engine.step(data);
        finish_frame();
    };
    const auto normalized = [](float value) {
        return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
    };

    ImGui::NewFrame();
    update_controls();
    const auto initialized = read_texture(texture);
    finish_frame();
    for (double total : sum_rgb(initialized)) {
        REQUIRE(total > 0.0);
    }
    CHECK(std::all_of(initialized.data.get(), initialized.data.get() + initialized.size(), normalized));

    SECTION("The fragment engine evolves RGB within the normalized range") {
        frame();
        const auto evolved = read_texture(texture);
        CHECK(max_rgb_change(initialized, evolved) > 1.0e-5);
        CHECK(std::all_of(evolved.data.get(), evolved.data.get() + evolved.size(), normalized));
        CHECK(program.is_linked());
    }

    SECTION("Clear, paint, and erase work without advancing the CA") {
        advance_ca = false;
        frame();
        CHECK(max_rgb_change(initialized, read_texture(texture)) == 0.0);

        // DynamicControl queues the hotkey in draw() and clears on the next update().
        io.KeysDown['P'] = true;
        frame();
        io.KeysDown['P'] = false;
        frame();
        CHECK(rgb_is_zero(read_texture(texture)));

        io.MousePos = ImVec2(256, 256);
        io.MouseDown[0] = true;
        frame();
        io.MouseDown[0] = false;
        const auto painted = read_texture(texture);
        const auto painted_sum = sum_rgb(painted);
        for (double total : painted_sum) {
            REQUIRE(total > 0.0);
        }
        frame();
        CHECK(max_rgb_change(painted, read_texture(texture)) == 0.0);

        io.MouseDown[1] = true;
        frame();
        io.MouseDown[1] = false;
        const auto erased = read_texture(texture);
        const auto erased_sum = sum_rgb(erased);
        for (int channel = 0; channel < 3; ++channel) {
            CHECK(erased_sum[channel] < painted_sum[channel]);
        }
        CHECK(std::all_of(erased.data.get(), erased.data.get() + erased.size(), normalized));
    }

    CHECK(glGetError() == GL_NO_ERROR);
}
