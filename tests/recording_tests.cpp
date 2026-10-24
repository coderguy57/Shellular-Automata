#include <catch2/catch_test_macros.hpp>

#include "controllers/recorder_control.hpp"
#include "data.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "render/texture_save.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

bool InitOpenGLForTesting(GLFWwindow** window, std::string& error);

namespace {
class RecordingTestContext {
public:
    RecordingTestContext() {
        std::string error;
        if (!InitOpenGLForTesting(&window_, error))
            SKIP("OpenGL unavailable: " << error);
    }
    ~RecordingTestContext() {
        glfwDestroyWindow(window_);
        glfwTerminate();
    }

private:
    GLFWwindow* window_ = nullptr;
};

class RecordingDirectory {
public:
    RecordingDirectory() : original_path_(std::filesystem::current_path()) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() / ("shellular-recording-" + std::to_string(stamp));
        REQUIRE(std::filesystem::create_directory(root_));
        path = root_ / "out";
        std::filesystem::create_directory(path);
        const auto work = root_ / "work";
        std::filesystem::create_directories(work / "res");
        for (const auto* shader : {"basic.vs", "basic.fs"})
            std::filesystem::copy_file(original_path_ / "res" / shader, work / "res" / shader);
        std::filesystem::current_path(work);
    }
    ~RecordingDirectory() {
        std::error_code error;
        std::filesystem::current_path(original_path_, error);
        std::filesystem::remove_all(root_, error);
    }

    std::filesystem::path path;

private:
    std::filesystem::path original_path_;
    std::filesystem::path root_;
};

class RecordingUi {
public:
    explicit RecordingUi(RecorderControl& recorder) : recorder_(recorder) {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(640, 480);
        io.DeltaTime = 1.0f / 60.0f;
        io.Fonts->Build();
    }
    ~RecordingUi() {
        ImGui::DestroyContext();
    }

    void open() {
        draw();
        auto* menu = ImGui::FindWindowByName("##MainMenuBar");
        REQUIRE(menu != nullptr);
        const auto& style = ImGui::GetStyle();
        click(ImVec2(menu->Pos.x + style.DisplaySafeAreaPadding.x + style.ItemSpacing.x * 0.5f +
                     ImGui::CalcTextSize("Record").x * 0.5f,
                     menu->Pos.y + ImGui::GetFrameHeight() * 0.5f));
        REQUIRE(GImGui->OpenPopupStack.Size == 1);
        auto* popup = GImGui->OpenPopupStack.back().Window;
        REQUIRE(popup != nullptr);
        click(ImVec2(popup->DC.CursorStartPos.x + ImGui::CalcTextSize("Save video").x * 0.5f,
                     popup->DC.CursorStartPos.y + ImGui::GetTextLineHeight() * 0.5f));
        draw();
        auto* window = ImGui::FindWindowByName("Record");
        REQUIRE(window != nullptr);
        REQUIRE(window->WasActive);
    }

    void toggle_capture() {
        auto* window = ImGui::FindWindowByName("Record");
        REQUIRE(window != nullptr);
        const auto& style = ImGui::GetStyle();
        click(ImVec2(window->DC.CursorStartPos.x + style.FramePadding.x +
                     ImGui::CalcTextSize("Capture frames").x * 0.5f,
                     window->DC.CursorStartPos.y + ImGui::GetFrameHeightWithSpacing() +
                     ImGui::GetFrameHeight() * 0.5f));
    }

    float content_height() {
        draw();
        draw();
        auto* window = ImGui::FindWindowByName("Record");
        REQUIRE(window != nullptr);
        return window->ContentSize.y;
    }

private:
    void draw() {
        ImGui::NewFrame();
        recorder_.draw();
        ImGui::EndFrame();
    }

    void click(ImVec2 position) {
        auto& io = ImGui::GetIO();
        io.MousePos = position;
        draw();
        io.MouseDown[0] = true;
        draw();
        io.MouseDown[0] = false;
        draw();
    }

    RecorderControl& recorder_;
};

// Observe real GL allocations without exposing resource handles in the recorder API.
class RecordedGpuObjects {
public:
    RecordedGpuObjects()
        : gen_buffers_(glGenBuffers), fence_sync_(glFenceSync),
          create_program_(glCreateProgram), create_shader_(glCreateShader),
          gen_framebuffers_(glGenFramebuffers), gen_vertex_arrays_(glGenVertexArrays) {
        active_ = this;
        glGenBuffers = gen_buffers;
        glFenceSync = fence_sync;
        glCreateProgram = create_program;
        glCreateShader = create_shader;
        glGenFramebuffers = gen_framebuffers;
        glGenVertexArrays = gen_vertex_arrays;
    }
    ~RecordedGpuObjects() {
        glGenBuffers = gen_buffers_;
        glFenceSync = fence_sync_;
        glCreateProgram = create_program_;
        glCreateShader = create_shader_;
        glGenFramebuffers = gen_framebuffers_;
        glGenVertexArrays = gen_vertex_arrays_;
        active_ = nullptr;
    }

    void require_released() const {
        // A deleted program and its attached shaders remain alive while it is current.
        glUseProgram(0);
        for (GLuint buffer : buffers)
            CHECK(glIsBuffer(buffer) == GL_FALSE);
        for (GLsync fence : fences)
            CHECK(glIsSync(fence) == GL_FALSE);
        for (GLuint program : programs)
            CHECK(glIsProgram(program) == GL_FALSE);
        for (GLuint shader : shaders)
            CHECK(glIsShader(shader) == GL_FALSE);
        for (GLuint framebuffer : framebuffers)
            CHECK(glIsFramebuffer(framebuffer) == GL_FALSE);
        for (GLuint vao : vertex_arrays)
            CHECK(glIsVertexArray(vao) == GL_FALSE);
    }

    auto allocation_counts() const {
        return std::array<size_t, 6>{buffers.size(), fences.size(), programs.size(),
                                   shaders.size(), framebuffers.size(), vertex_arrays.size()};
    }

    std::vector<GLuint> buffers;
    std::vector<GLsync> fences;
    std::vector<GLuint> programs;
    std::vector<GLuint> shaders;
    std::vector<GLuint> framebuffers;
    std::vector<GLuint> vertex_arrays;

private:
    static void GLAPIENTRY gen_buffers(GLsizei count, GLuint* buffers) {
        active_->gen_buffers_(count, buffers);
        active_->buffers.insert(active_->buffers.end(), buffers, buffers + count);
    }
    static GLsync GLAPIENTRY fence_sync(GLenum condition, GLbitfield flags) {
        GLsync fence = active_->fence_sync_(condition, flags);
        active_->fences.push_back(fence);
        return fence;
    }
    static GLuint GLAPIENTRY create_program() {
        const GLuint program = active_->create_program_();
        active_->programs.push_back(program);
        return program;
    }
    static GLuint GLAPIENTRY create_shader(GLenum type) {
        const GLuint shader = active_->create_shader_(type);
        active_->shaders.push_back(shader);
        return shader;
    }
    static void GLAPIENTRY gen_framebuffers(GLsizei count, GLuint* framebuffers) {
        active_->gen_framebuffers_(count, framebuffers);
        active_->framebuffers.insert(active_->framebuffers.end(), framebuffers, framebuffers + count);
    }
    static void GLAPIENTRY gen_vertex_arrays(GLsizei count, GLuint* arrays) {
        active_->gen_vertex_arrays_(count, arrays);
        active_->vertex_arrays.insert(active_->vertex_arrays.end(), arrays, arrays + count);
    }

    static inline RecordedGpuObjects* active_ = nullptr;
    PFNGLGENBUFFERSPROC gen_buffers_;
    PFNGLFENCESYNCPROC fence_sync_;
    PFNGLCREATEPROGRAMPROC create_program_;
    PFNGLCREATESHADERPROC create_shader_;
    PFNGLGENFRAMEBUFFERSPROC gen_framebuffers_;
    PFNGLGENVERTEXARRAYSPROC gen_vertex_arrays_;
};

class CaptureTargetFault {
public:
    enum class Kind { ShaderLink, Framebuffer };

    explicit CaptureTargetFault(Kind kind)
        : kind_(kind), get_program_(glGetProgramiv), check_framebuffer_(glCheckFramebufferStatus) {
        active_ = this;
        glGetProgramiv = get_program;
        glCheckFramebufferStatus = check_framebuffer;
    }
    ~CaptureTargetFault() {
        glGetProgramiv = get_program_;
        glCheckFramebufferStatus = check_framebuffer_;
        active_ = nullptr;
    }
    CaptureTargetFault(const CaptureTargetFault&) = delete;
    CaptureTargetFault& operator=(const CaptureTargetFault&) = delete;

    int failures = 0;

private:
    static void GLAPIENTRY get_program(GLuint program, GLenum parameter, GLint* value) {
        auto& self = *active_;
        self.get_program_(program, parameter, value);
        if (self.kind_ == Kind::ShaderLink && parameter == GL_LINK_STATUS) {
            ++self.failures;
            *value = GL_FALSE;
        }
    }
    static GLenum GLAPIENTRY check_framebuffer(GLenum target) {
        auto& self = *active_;
        if (self.kind_ == Kind::Framebuffer) {
            ++self.failures;
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }
        return self.check_framebuffer_(target);
    }

    static inline CaptureTargetFault* active_ = nullptr;
    Kind kind_;
    PFNGLGETPROGRAMIVPROC get_program_;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC check_framebuffer_;
};

// These overrides compose with RecordedGpuObjects and restore the GLEW dispatch table.
class ReadbackFault {
public:
    enum class Kind { Fence, TimeoutOnce, Wait, Map, Unmap };

    explicit ReadbackFault(Kind kind)
        : kind_(kind), fence_(glFenceSync), wait_(glClientWaitSync),
          map_(glMapNamedBuffer), unmap_(glUnmapNamedBuffer) {
        active_ = this;
        glFenceSync = fence;
        glClientWaitSync = wait;
        glMapNamedBuffer = map;
        glUnmapNamedBuffer = unmap;
    }
    ~ReadbackFault() {
        glFenceSync = fence_;
        glClientWaitSync = wait_;
        glMapNamedBuffer = map_;
        glUnmapNamedBuffer = unmap_;
        active_ = nullptr;
    }
    ReadbackFault(const ReadbackFault&) = delete;
    ReadbackFault& operator=(const ReadbackFault&) = delete;

    // Fence, wait, map, unmap calls, respectively.
    std::array<int, 4> calls{};

private:
    static GLsync GLAPIENTRY fence(GLenum condition, GLbitfield flags) {
        auto& self = *active_;
        ++self.calls[0];
        return self.kind_ == Kind::Fence ? nullptr : self.fence_(condition, flags);
    }
    static GLenum GLAPIENTRY wait(GLsync sync, GLbitfield flags, GLuint64 timeout) {
        auto& self = *active_;
        ++self.calls[1];
        if (self.kind_ == Kind::Wait)
            return GL_WAIT_FAILED;
        if (self.kind_ == Kind::TimeoutOnce && self.calls[1] == 1)
            return GL_TIMEOUT_EXPIRED;
        return self.wait_(sync, flags, timeout);
    }
    static void* GLAPIENTRY map(GLuint buffer, GLenum access) {
        auto& self = *active_;
        ++self.calls[2];
        return self.kind_ == Kind::Map ? nullptr : self.map_(buffer, access);
    }
    static GLboolean GLAPIENTRY unmap(GLuint buffer) {
        auto& self = *active_;
        ++self.calls[3];
        const GLboolean valid = self.unmap_(buffer);
        return self.kind_ == Kind::Unmap ? GL_FALSE : valid;
    }

    static inline ReadbackFault* active_ = nullptr;
    Kind kind_;
    PFNGLFENCESYNCPROC fence_;
    PFNGLCLIENTWAITSYNCPROC wait_;
    PFNGLMAPNAMEDBUFFERPROC map_;
    PFNGLUNMAPNAMEDBUFFERPROC unmap_;
};

void require_sticky_failure(TextureSave& saver, const ReadbackFault* fault = nullptr) {
    const auto result = saver.try_save();
    REQUIRE(result.status == TextureSave::Status::Failed);
    REQUIRE(result.error != nullptr);
    REQUIRE(std::string(result.error).size() > 0);
    REQUIRE(saver.is_done());
    const std::string message = result.error;
    const auto calls = fault ? fault->calls : std::array<int, 4>{};
    for (int retry = 0; retry < 3; ++retry) {
        const auto repeated = saver.try_save();
        CHECK(repeated.status == TextureSave::Status::Failed);
        REQUIRE(repeated.error != nullptr);
        CHECK(repeated.error == result.error);
        CHECK(std::string(repeated.error) == message);
        CHECK(saver.is_done());
        if (fault)
            CHECK(fault->calls == calls);
    }
}

std::string pam_header(int width, int height) {
    return "P7\nWIDTH " + std::to_string(width) + "\nHEIGHT " + std::to_string(height) +
        "\nDEPTH 3\nMAXVAL 255\nTUPLTYPE RGB\nENDHDR\n";
}

std::string read_recording(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.is_open());
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

std::vector<unsigned char> test_pixels(int width, int height, int seed = 0) {
    std::vector<unsigned char> pixels(width * height * 4);
    for (size_t i = 0; i < pixels.size(); i++)
        pixels[i] = static_cast<unsigned char>((i * 37 + seed * 61) % 256);
    return pixels;
}

std::string rgb_bytes(const std::vector<unsigned char>& rgba) {
    std::string rgb;
    for (size_t i = 0; i < rgba.size(); i += 4)
        rgb.append(reinterpret_cast<const char*>(rgba.data() + i), 3);
    return rgb;
}

TextureOptions recording_texture_options() {
    TextureOptions options;
    options.internal_format = GL_RGBA8;
    options.type = GL_UNSIGNED_BYTE;
    return options;
}
}

TEST_CASE("Recording saves exact RGB PAM bytes at awkward row widths", "[recording][opengl]") {
    RecordingTestContext context;
    RecordingDirectory directory;

    constexpr std::array<GLenum, 6> pack_parameters{
        GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_IMAGE_HEIGHT,
        GL_PACK_SKIP_PIXELS, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_IMAGES
    };
    constexpr std::array<GLint, 6> pack_values{8, 23, 11, 2, 3, 1};
    GLuint previous_pbo = 0;
    glGenBuffers(1, &previous_pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, previous_pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, 16, nullptr, GL_STREAM_READ);

    RecordedGpuObjects resources;
    for (int width : {1, 2, 3, 5, 7, 8}) {
        CAPTURE(width);
        constexpr int height = 3;
        const auto pixels = test_pixels(width, height);
        Texture texture(width, height, 1, recording_texture_options(), pixels.data());
        for (size_t i = 0; i < pack_parameters.size(); i++)
            glPixelStorei(pack_parameters[i], pack_values[i]);

        const std::string name = "width_" + std::to_string(width);
        {
            TextureSave saver(name, texture);
            REQUIRE_FALSE(saver.is_done());
            GLint pbo = 0;
            glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
            CHECK(GLuint(pbo) == previous_pbo);
            for (size_t i = 0; i < pack_parameters.size(); i++) {
                GLint value = 0;
                glGetIntegerv(pack_parameters[i], &value);
                CHECK(value == pack_values[i]);
            }

            glFinish();
            const auto saved = saver.try_save();
            REQUIRE(saved.status == TextureSave::Status::Saved);
            CHECK(std::string(saved.error).empty());
            REQUIRE(saver.is_done());
            const auto repeated = saver.try_save();
            CHECK(repeated.status == TextureSave::Status::Saved);
            CHECK(std::string(repeated.error).empty());
            resources.require_released();
            glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pbo);
            CHECK(GLuint(pbo) == previous_pbo);
        }
        CHECK(read_recording(directory.path / (name + ".PAM")) == pam_header(width, height) + rgb_bytes(pixels));
        CHECK(glGetError() == GL_NO_ERROR);
    }

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glDeleteBuffers(1, &previous_pbo);
    for (size_t i = 0; i < pack_parameters.size(); i++)
        glPixelStorei(pack_parameters[i], i == 0 ? 4 : 0);
}

TEST_CASE("Recording releases canceled and failed GPU readbacks", "[recording][opengl]") {
    RecordingTestContext context;
    RecordingDirectory directory;
    const auto pixels = test_pixels(5, 3);
    Texture texture(5, 3, 1, recording_texture_options(), pixels.data());
    RecordedGpuObjects resources;

    for (int frame = 0; frame < 8; frame++) {
        {
            TextureSave canceled("canceled_" + std::to_string(frame), texture);
            REQUIRE_FALSE(canceled.is_done());
        }
        resources.require_released();
    }
    CHECK(std::filesystem::is_empty(directory.path));

    std::string filename;
    SECTION("Output directory is a regular file") {
        REQUIRE(std::filesystem::remove(directory.path));
        std::ofstream(directory.path) << "not a directory";
        filename = "failure";
    }
    SECTION("Output file cannot be opened") {
        filename = "missing/frame";
    }
    {
        TextureSave failed(filename, texture);
        glFinish();
        require_sticky_failure(failed);
        resources.require_released();
    }
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Recording fence failure is nonthrowing and restores GL state", "[recording][opengl]") {
    RecordingTestContext context;
    RecordingDirectory directory;
    const auto pixels = test_pixels(5, 3);
    Texture texture(5, 3, 1, recording_texture_options(), pixels.data());
    GLuint previous_pbo = 0;
    glGenBuffers(1, &previous_pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, previous_pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, 16, nullptr, GL_STREAM_READ);
    constexpr std::array<GLenum, 6> parameters{
        GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH, GL_PACK_IMAGE_HEIGHT,
        GL_PACK_SKIP_PIXELS, GL_PACK_SKIP_ROWS, GL_PACK_SKIP_IMAGES
    };
    constexpr std::array<GLint, 6> values{8, 23, 11, 2, 3, 1};
    for (size_t i = 0; i < parameters.size(); ++i)
        glPixelStorei(parameters[i], values[i]);

    {
        RecordedGpuObjects resources;
        ReadbackFault fault(ReadbackFault::Kind::Fence);
        TextureSave failed("fence_failure", texture);
        REQUIRE(failed.is_done());
        const auto result = failed.try_save();
        CHECK(result.status == TextureSave::Status::Failed);
        REQUIRE(result.error != nullptr);
        CHECK(std::string(result.error) == "Failed to create a recording readback fence");
        require_sticky_failure(failed, &fault);
        CHECK(fault.calls == std::array<int, 4>{1, 0, 0, 0});
        REQUIRE(resources.buffers.size() == 1);
        CHECK(resources.fences.empty());
        resources.require_released();
        GLint binding = 0;
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &binding);
        CHECK(GLuint(binding) == previous_pbo);
        for (size_t i = 0; i < parameters.size(); ++i) {
            GLint value = 0;
            glGetIntegerv(parameters[i], &value);
            CHECK(value == values[i]);
        }
        CHECK(std::filesystem::is_empty(directory.path));
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glDeleteBuffers(1, &previous_pbo);
    for (size_t i = 0; i < parameters.size(); ++i)
        glPixelStorei(parameters[i], i == 0 ? 4 : 0);
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Recording readback results survive timeout and GPU failures", "[recording][opengl]") {
    RecordingTestContext context;
    RecordingDirectory directory;
    const auto pixels = test_pixels(5, 3);
    Texture texture(5, 3, 1, recording_texture_options(), pixels.data());
    auto kind = ReadbackFault::Kind::TimeoutOnce;
    SECTION("Timeout followed by ready") { kind = ReadbackFault::Kind::TimeoutOnce; }
    SECTION("Client wait failure") { kind = ReadbackFault::Kind::Wait; }
    SECTION("Map failure") { kind = ReadbackFault::Kind::Map; }
    SECTION("Corrupt unmap") { kind = ReadbackFault::Kind::Unmap; }

    RecordedGpuObjects resources;
    ReadbackFault fault(kind);
    TextureSave saver("readback", texture);
    REQUIRE_FALSE(saver.is_done());
    glFinish();
    if (kind == ReadbackFault::Kind::TimeoutOnce) {
        const auto pending = saver.try_save();
        CHECK(pending.status == TextureSave::Status::Pending);
        CHECK(std::string(pending.error).empty());
        CHECK_FALSE(saver.is_done());
        CHECK(fault.calls == std::array<int, 4>{1, 1, 0, 0});
        REQUIRE(resources.buffers.size() == 1);
        REQUIRE(resources.fences.size() == 1);
        CHECK(glIsBuffer(resources.buffers.front()) == GL_TRUE);
        CHECK(glIsSync(resources.fences.front()) == GL_TRUE);
        CHECK(std::filesystem::is_empty(directory.path));
        const auto saved = saver.try_save();
        REQUIRE(saved.status == TextureSave::Status::Saved);
        CHECK(std::string(saved.error).empty());
        CHECK(saver.is_done());
        CHECK(fault.calls == std::array<int, 4>{1, 2, 1, 1});
        const auto calls = fault.calls;
        const auto repeated = saver.try_save();
        CHECK(repeated.status == TextureSave::Status::Saved);
        CHECK(std::string(repeated.error).empty());
        CHECK(fault.calls == calls);
        CHECK(read_recording(directory.path / "readback.PAM") == pam_header(5, 3) + rgb_bytes(pixels));
    } else {
        require_sticky_failure(saver, &fault);
        const int maps = kind == ReadbackFault::Kind::Wait ? 0 : 1;
        const int unmaps = kind == ReadbackFault::Kind::Unmap ? 1 : 0;
        CHECK(fault.calls == std::array<int, 4>{1, 1, maps, unmaps});
    }
    resources.require_released();
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Recorder captures rendered frames and flushes them when stopped", "[recording][opengl]") {
    RecordingTestContext context;
    RecordingDirectory directory;
    constexpr int width = 5;
    constexpr int height = 3;
    auto options = recording_texture_options();
    options.target = GL_TEXTURE_2D_ARRAY;
    Data data;
    auto* texture_data = data.add_element<TextureData>("recorded", width, height, 1, options);
    RecordedGpuObjects resources;

    {
        RecorderControl recorder("recorded");
        RecordingUi ui(recorder);
        ui.open();
        for (int frame = 0; frame < 3; frame++) {
            CAPTURE(frame);
            const auto pixels = test_pixels(width, height, frame);
            glTextureSubImage3D(texture_data->get_texture().gl_tex_num, 0, 0, 0, 0,
                                width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            glViewport(7, 11, 13, 17);
            glEnable(GL_SCISSOR_TEST);
            glScissor(0, 0, 0, 0);
            glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ZERO, GL_ZERO);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_NEVER);
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_NEVER, 0, ~0u);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT_AND_BACK);
            glEnable(GL_RASTERIZER_DISCARD);
            glEnable(GL_COLOR_LOGIC_OP);
            glLogicOp(GL_CLEAR);
            glEnable(GL_FRAMEBUFFER_SRGB);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            ui.toggle_capture();
            recorder.post_process(data);
            ui.toggle_capture();

            const auto saved = read_recording(directory.path / ("frame_" + std::to_string(frame) + ".PAM"));
            const auto header = pam_header(width, height);
            REQUIRE(saved.substr(0, header.size()) == header);
            REQUIRE(saved.size() == header.size() + width * height * 3);
            for (int pixel = 0; pixel < width * height; pixel++) {
                for (int channel = 0; channel < 3; channel++) {
                    const float linear = pixels[pixel * 4 + channel] / 255.0f;
                    const int expected = std::lround(std::pow(linear * 0.7f, 1.0f / 2.2f) * 255.0f);
                    const int actual = static_cast<unsigned char>(saved[header.size() + pixel * 3 + channel]);
                    CHECK(std::abs(actual - expected) <= 1);
                }
            }
            CHECK(glGetError() == GL_NO_ERROR);
        }
    }
    resources.require_released();
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Recorder stops capturing after a returned readback failure", "[recording][opengl]") {
    RecordingTestContext context;
    RecordingDirectory directory;
    auto kind = ReadbackFault::Kind::Fence;
    SECTION("Fence creation fails") { kind = ReadbackFault::Kind::Fence; }
    SECTION("Waiting for the capture fails") {
        kind = ReadbackFault::Kind::Wait;
    }
    auto options = recording_texture_options();
    options.target = GL_TEXTURE_2D_ARRAY;
    Data data;
    auto* texture_data = data.add_element<TextureData>("recorded", 5, 3, 1, options);
    RecordedGpuObjects resources;
    {
        RecorderControl recorder("recorded");
        RecordingUi ui(recorder);
        ui.open();
        ui.toggle_capture();
        const float normal_height = ui.content_height();
        {
            ReadbackFault fault(kind);
            REQUIRE_NOTHROW(recorder.post_process(data));
            REQUIRE(fault.calls[0] == 1);
            CHECK(fault.calls[1] == (kind == ReadbackFault::Kind::Wait ? 1 : 0));
            CHECK(fault.calls[2] == 0);
            CHECK(fault.calls[3] == 0);
            const auto calls = fault.calls;
            // More than two default capture intervals, without another UI toggle.
            for (int frame = 0; frame < 25; ++frame)
                REQUIRE_NOTHROW(recorder.post_process(data));
            CHECK(fault.calls == calls);
        }
        const auto buffers = resources.buffers.size();
        const auto fences = resources.fences.size();
        for (int frame = 0; frame < 25; ++frame)
            REQUIRE_NOTHROW(recorder.post_process(data));
        CHECK(resources.buffers.size() == buffers);
        CHECK(resources.fences.size() == fences);
        CHECK(ui.content_height() > normal_height);
        CHECK(std::filesystem::is_empty(directory.path));
        if (kind == ReadbackFault::Kind::Fence) {
            const std::vector<unsigned char> pixels(5 * 3 * 4, 0);
            glTextureSubImage3D(texture_data->get_texture().gl_tex_num, 0, 0, 0, 0,
                                5, 3, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            ui.toggle_capture();
            REQUIRE_NOTHROW(recorder.post_process(data));
            ui.toggle_capture();
            CHECK(read_recording(directory.path / "frame_0.PAM") ==
                  pam_header(5, 3) + std::string(5 * 3 * 3, '\0'));
            CHECK_FALSE(std::filesystem::exists(directory.path / "frame_1.PAM"));
            CHECK(resources.fences.size() == fences + 1);
            CHECK(ui.content_height() == normal_height);
        }
    }
    resources.require_released();
    CHECK(glGetError() == GL_NO_ERROR);
}

TEST_CASE("Recorder releases invalid capture targets and retries after repair", "[recording][opengl]") {
    RecordingTestContext context;
    RecordingDirectory directory;
    auto kind = CaptureTargetFault::Kind::ShaderLink;
    SECTION("Output shader fails to link") { kind = CaptureTargetFault::Kind::ShaderLink; }
    SECTION("Capture framebuffer is incomplete") { kind = CaptureTargetFault::Kind::Framebuffer; }

    auto options = recording_texture_options();
    options.target = GL_TEXTURE_2D_ARRAY;
    Data data;
    auto* texture_data = data.add_element<TextureData>("recorded", 5, 3, 1, options);
    const std::vector<unsigned char> pixels(5 * 3 * 4, 0);
    glTextureSubImage3D(texture_data->get_texture().gl_tex_num, 0, 0, 0, 0,
                       5, 3, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    RecordedGpuObjects resources;
    {
        RecorderControl recorder("recorded");
        RecordingUi ui(recorder);
        ui.open();
        ui.toggle_capture();
        const float normal_height = ui.content_height();
        {
            CaptureTargetFault fault(kind);
            REQUIRE_NOTHROW(recorder.post_process(data));
            REQUIRE(fault.failures == 1);
            REQUIRE(resources.programs.size() == 1);
            REQUIRE(resources.shaders.size() == 2);
            REQUIRE(resources.framebuffers.size() == 1);
            REQUIRE(resources.vertex_arrays.size() == 1);
            REQUIRE(resources.buffers.size() == 1); // Fullscreen vertex buffer, no readback.
            CHECK(resources.fences.empty());
            resources.require_released();
            const auto allocations = resources.allocation_counts();
            for (int frame = 0; frame < 25; ++frame)
                REQUIRE_NOTHROW(recorder.post_process(data));
            CHECK(fault.failures == 1);
            CHECK(resources.allocation_counts() == allocations);
            CHECK(std::filesystem::is_empty(directory.path));
            CHECK(ui.content_height() > normal_height);
        }
        // Repair alone must not resume recording; only the next capture click does.
        const auto allocations = resources.allocation_counts();
        for (int frame = 0; frame < 25; ++frame)
            REQUIRE_NOTHROW(recorder.post_process(data));
        CHECK(resources.allocation_counts() == allocations);
        CHECK(std::filesystem::is_empty(directory.path));

        ui.toggle_capture();
        REQUIRE_NOTHROW(recorder.post_process(data));
        ui.toggle_capture();
        CHECK(read_recording(directory.path / "frame_0.PAM") ==
              pam_header(5, 3) + std::string(5 * 3 * 3, '\0'));
        CHECK_FALSE(std::filesystem::exists(directory.path / "frame_1.PAM"));
        CHECK(resources.programs.size() == 2);
        CHECK(resources.framebuffers.size() == 2);
        CHECK(resources.fences.size() == 1);
        CHECK(ui.content_height() == normal_height);
    }
    resources.require_released();
    CHECK(glGetError() == GL_NO_ERROR);
}
