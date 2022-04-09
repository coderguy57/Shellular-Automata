#pragma once

#include <vector>

#include <GL/glew.h>

class VAO
{
    GLuint id;

public:
    VAO(void)
    {
        glGenVertexArrays(1, &id);
    }
    ~VAO()
    {
        glDeleteVertexArrays(1, &id);
    }
    VAO(const VAO &) = default;
    VAO(VAO &&)
    noexcept = default;
    VAO &operator=(const VAO &) = default;

    void bind(void) const
    {
        glBindVertexArray(id);
    }
    GLuint get_id(void) const
    {
        return id;
    }
};

class VBO
{
    GLuint id;

public:
    VBO(void)
    {
        glGenBuffers(1, &id);
    }
    ~VBO()
    {
        glDeleteBuffers(1, &id);
    }
    VBO(const VBO &) = default;
    VBO(VBO &&)
    noexcept = default;
    VBO &operator=(const VBO &) = default;

    void bind(GLenum target = GL_ARRAY_BUFFER) const
    {
        glBindBuffer(target, id);
    }
    GLuint get_id(void) const
    {
        return id;
    }
};

template <typename V, typename T>
class PackedData
{
public:
    V vert;
    T tex;

    PackedData(void){};
    PackedData(V _vert, T _tex) : vert(_vert), tex(_tex){};
    ~PackedData(){};
    PackedData(const PackedData &) = default;
    PackedData(PackedData &&) noexcept = default;
    PackedData &operator=(const PackedData &) = default;
};

/**
        * Packed model - packs both vertices and texcoords into the same buffer
        */
template <typename V, typename T>
class PackedModel
{
public:
    std::vector<PackedData<V, T>> buffer;
    VAO vao;
    VBO vbo;
    GLint mode;

    PackedModel(GLint _mode) : mode(_mode){};
    ~PackedModel(){};
    PackedModel(const PackedModel &) = default;
    PackedModel(PackedModel &&) noexcept = default;
    PackedModel &operator=(const PackedModel &) = default;

    virtual void draw(void) const
    {
        vao.bind();
        glDrawArrays(mode, 0, buffer.size());
    }
};
