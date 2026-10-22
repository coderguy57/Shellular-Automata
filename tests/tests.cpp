#include <catch2/catch_test_macros.hpp>

#include "utils/image_load.hpp"
#include "utils/image_load.hpp"
#include "fftw3.h"
#include "engines/compute_shader_engine.hpp"
#include "engines/fragment_shader_engine.hpp"
#include "render/texture_save.hpp"
#include "data.hpp"

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
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

    // Enable OpenGL debug output
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(GLDebugMessageCallback, 0);

    return true;
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
    std::string gl_error;
    GLFWwindow* window = nullptr;
    if (!InitOpenGLForTesting(&window, gl_error)) {
        WARN("OpenGL unavailable; skipping GPU FFT image comparison: " << gl_error);
        SUCCEED("GPU FFT image comparison requires an OpenGL context");
        return;
    }
    std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)> cleanup(window, [](GLFWwindow* window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    });

    auto texture_data = load_texture_data("res/image.png", 4);

    REQUIRE(texture_data.has_value());
    auto width = texture_data->width;
    auto height = texture_data->height;

    Data data = Data();

    TextureOptions options;
    options.target = GL_TEXTURE_2D_ARRAY;
    auto temp_texture_data = TextureData(load_texture(*texture_data, options));
    auto data_texture = data.add_element("texture", std::make_unique<TextureData>(std::move(temp_texture_data))); 
    auto output_texture = data.add_element<TextureData>("out_texture", data_texture->width(), data_texture->height(), 2, data_texture->options()); 
    std::unique_ptr<ComputeShaderEngine> engine = std::make_unique<ComputeShaderEngine>();
    create_fft_step(engine.get(), data_texture, output_texture);
    create_ifft_step(engine.get(), output_texture, data_texture);
    engine->step(data);
    glFinish();

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

TEST_CASE("Demand flux conserves RGB material", "[fluidMNCA][opengl]") {
    std::string gl_error;
    GLFWwindow* window = nullptr;
    if (!InitOpenGLForTesting(&window, gl_error)) {
        WARN("OpenGL unavailable; skipping demand flux conservation test: " << gl_error);
        SUCCEED("Demand flux conservation requires an OpenGL context");
        return;
    }
    std::unique_ptr<GLFWwindow, void(*)(GLFWwindow*)> cleanup(window, [](GLFWwindow* window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    });

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

    auto sum_rgb = [](const TextureBuffer<float>& buffer) {
        std::array<double, 3> sum{0.0, 0.0, 0.0};
        for (int pixel = 0; pixel < buffer.width * buffer.height * buffer.depth; pixel++) {
            for (int channel = 0; channel < 3; channel++) {
                sum[channel] += buffer.data[pixel * buffer.channels + channel];
            }
        }
        return sum;
    };

    TextureOptions options;
    options.target = GL_TEXTURE_2D_ARRAY;
    options.internal_format = GL_RGBA32F;
    options.format = GL_RGBA;
    options.type = GL_FLOAT;

    Data data;
    auto data_texture = data.add_element("data_texture", std::make_unique<TextureData>(
        Texture(width, height, 1, options, material.data())));
    auto demand_texture = data.add_element("demand_texture", std::make_unique<TextureData>(
        Texture(width, height, 1, options, demand.data())));
    auto total_texture = data.add_element<TextureData>("total_texture", width, height, 1, options);

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

    auto after = save_texture_directly<float>(data_texture->get_texture(), read_options);
    REQUIRE(after.has_value());
    auto after_sum = sum_rgb(*after);

    constexpr double tolerance = 1.0e-4;
    for (int channel = 0; channel < 3; channel++) {
        CAPTURE(channel, before_sum[channel], after_sum[channel], after_sum[channel] - before_sum[channel]);
        CHECK(std::abs(after_sum[channel] - before_sum[channel]) <= tolerance);
    }
}
