#include "framebuffer.hpp"

#include "print.hpp"

Framebuffer::Framebuffer() {
    glGenFramebuffers(1, &_id);
    glBindFramebuffer(GL_FRAMEBUFFER, _id);
}

Framebuffer::~Framebuffer() {
    if (_id)
        glDeleteFramebuffers(1, &_id);
}

void Framebuffer::set_texture(int index, const Texture* texture) {
    if (texture->target == GL_TEXTURE_2D_ARRAY) {
        texture->bind();
        for (unsigned int i = 0; i < texture->depth; i++) {
            glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, texture->gl_tex_num, 0, i);
        }
    	glBindTexture(texture->target, 0);
    } else {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, texture->target, texture->gl_tex_num, 0);
    }
}

void Framebuffer::use() {
    glBindFramebuffer(GL_FRAMEBUFFER, _id);
}