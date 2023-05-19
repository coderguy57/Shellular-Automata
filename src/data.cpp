#include "data.hpp"

TextureData::TextureData(uint32_t width, uint32_t height, TextureOptions options) : DataElement() {
    options.target = GL_TEXTURE_2D_ARRAY;
    texture0_ = std::make_unique<Texture>(width, height, 1, options);    
    texture1_ = std::make_unique<Texture>(width, height, 1, options);    
}
void TextureData::bind(GLuint layout) {
    auto& in_texture = tex0_last_updated_ ? texture0_ : texture1_;
    in_texture->bind();
    glBindImageTexture(layout, in_texture->gl_tex_num, 0, GL_FALSE, 0, GL_READ_WRITE, in_texture->options.internal_format);
}
void TextureData::bind_out(GLuint layout) {
    auto& out_texture = tex0_last_updated_ ? texture1_ : texture0_;
    out_texture->bind();
    glBindImageTexture(layout, out_texture->gl_tex_num, 0, GL_FALSE, 0, GL_READ_WRITE, out_texture->options.internal_format);
}