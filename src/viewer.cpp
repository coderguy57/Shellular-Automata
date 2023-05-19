#include "viewer.hpp"
#include "data.hpp"

Viewer::Viewer(uint32_t width, uint32_t height, uint32_t depth, std::string frag_path, std::string output_shader, GLuint internal_format, std::string texture_name)
    : _frag_path{frag_path} {
    program = new FragmentProgram{"basic.vs", frag_path};
    output_program = new FragmentProgram{"basic.vs", output_shader};
    _fbo = new Framebuffer();
    _surface = new RenderSurface();

    TextureOptions option{};
    option.target = GL_TEXTURE_2D_ARRAY;
    option.internal_format = internal_format;
    _tex0 = new Texture(width, height, 1, option);
    _tex1 = new Texture(width, height, 1, option);
    _texture_name = texture_name;
}

Viewer::~Viewer() {
    delete _surface;
    delete _tex0;
    delete _tex1;
    delete _fbo;
    delete program;
}

void Viewer::reload() {
    delete program;
    program = new FragmentProgram("basic.vs", _frag_path);
}

void Viewer::view(uint32_t width, uint32_t height, Data& data) {
    auto& texture_data = data.get_element<TextureData>(_texture_name);
    auto& texture = texture_data.get_texture();
    // Change the texture size if it has changed
    if (_tex0->width != texture.width || _tex0->height != texture.height) {
        delete _tex0;
        delete _tex1;
        TextureOptions option{};
        option.target = GL_TEXTURE_2D_ARRAY;
        option.internal_format = texture.options.internal_format;
        _tex0 = new Texture(width, height, 1, option);
        _tex1 = new Texture(width, height, 1, option);
    }

    {
        glViewport(0, 0, texture.width, texture.height);
        program->use();
        _fbo->use();

        _tex0_last_drawn = !_tex0_last_drawn;

        GLenum buffer = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &buffer);
        _fbo->set_texture(0, _tex0_last_drawn ? _tex1 : _tex0);
        program->set_texture(0, "prev_tex", _tex0_last_drawn ? _tex0 : _tex1);
        program->set_texture(1, "tex", &texture);
        _surface->draw();
    }

    glViewport(0, 0, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    output_program->use();
    output_program->set_texture(0, "tex", _tex0_last_drawn ? _tex1 : _tex0);
    _surface->draw();
}