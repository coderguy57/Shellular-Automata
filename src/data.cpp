#include "data.hpp"

TextureData::TextureData(uint32_t width, uint32_t height, uint32_t depth, TextureOptions options) 
    : DataElement(),
    texture0_(width, height, depth, options),
    texture1_(width, height, depth, options) {
    texture0_.options.target = GL_TEXTURE_2D_ARRAY;
    texture1_.options.target = GL_TEXTURE_2D_ARRAY;
}

TextureData::TextureData(Texture&& texture) 
    : DataElement(),
    texture0_(texture),
    texture1_(texture.width, texture.height, texture.depth, texture.options) {
    assert(texture.options.target == GL_TEXTURE_2D_ARRAY);
}

void TextureData::bind(GLuint layout) {
    auto& in_texture = tex0_last_updated_ ? texture0_ : texture1_;
    in_texture.bind();
    glBindImageTexture(layout, in_texture.gl_tex_num, 0, GL_TRUE, 0, GL_READ_WRITE, in_texture.options.internal_format);
}

void TextureData::bind_out(GLuint layout) {
    auto& out_texture = tex0_last_updated_ ? texture1_ : texture0_;
    out_texture.bind();
    glBindImageTexture(layout, out_texture.gl_tex_num, 0, GL_TRUE, 0, GL_READ_WRITE, out_texture.options.internal_format);
}