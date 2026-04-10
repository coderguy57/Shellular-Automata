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

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

void InitOpenGLForTesting() {
    // Set GLFW error callback
    glfwSetErrorCallback(glfw_error_callback);

    // Initialize GLFW
    if (!glfwInit()) {
        throw "Failed to init GLFW\n";
    }

    // Configure OpenGL context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);

    // Create an invisible window
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Ensure the window is hidden
    GLFWwindow* window = glfwCreateWindow(1, 1, "Hidden Context", NULL, NULL);
    if (window == NULL) {
        throw "Failed to create hidden window\n";
    }

    // Make the context current
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        throw "Failed to init GLEW\n";
    }

    // Enable OpenGL debug output
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(GLDebugMessageCallback, 0);

    // The OpenGL context is now ready for testing!
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

TEST_CASE("Factorials are computed", "[factorial]") {
    InitOpenGLForTesting();

    auto texture_data = load_texture_data("res/image.png", 4);

    if (!texture_data) {
        FAIL("Failed to load texture data");
    }
    auto width = texture_data->width;
    auto height = texture_data->height;

    auto texture_data_r = extract_texture_channel<float>(*texture_data, 0);

    fftwf_complex *fft_result;
    do_fftw(texture_data_r, &fft_result);

    fftwf_free(fft_result);

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

    auto custom_data = save_texture_directly<float>(data_texture->get_texture(), data_texture->options());
    
    save_texture_data("res/image_out.png", *custom_data);
}