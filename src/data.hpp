#pragma once

#include "render/texture.hpp"
#include "render/shader.hpp"
#include <memory>
#include <string>
#include <unordered_map>

class DataElement {
public:
    DataElement() {};
    virtual ~DataElement() = default;
    virtual void bind(GLuint layout) = 0;
    virtual void bind_out(GLuint layout) {};
    virtual void update() {};
    virtual GLuint memory_barrier_bits() {
        return 0;
    };
};
class TextureData : public DataElement {
public:
    TextureData(uint32_t width, uint32_t height, TextureOptions options = default_options);
    void bind(GLuint layout) override;
    void bind_out(GLuint layout) override;
    GLuint memory_barrier_bits() override {
        return GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
    };
    Texture& get_texture() { 
        return tex0_last_updated_ ? *texture0_ : *texture1_;
    }
    Texture& get_last_texture() { 
        return tex0_last_updated_ ? *texture1_ : *texture0_;
    }
    void update() override {
        tex0_last_updated_ = !tex0_last_updated_;
    }
private:
    std::unique_ptr<Texture> texture0_;
    std::unique_ptr<Texture> texture1_;
    bool tex0_last_updated_ = false;
};

class FragmentProgramData : public DataElement {
public:
    FragmentProgramData(std::string fragment_path) {
        program_ = std::make_unique<FragmentProgram>("basic.vs", fragment_path);
    }
    void bind(GLuint layout) override {}
    FragmentProgram& get_program() { 
        return *program_;
    }
private:
    std::unique_ptr<FragmentProgram> program_;
};

class Data {
public:
    void add_element(std::string name, std::unique_ptr<DataElement> data_element) {
        elements.insert({name, std::move(data_element)});
    };
    template <typename T>
    T& get_element(std::string name) {
        auto it = elements.find(name);
        if (it == elements.end()) {
            throw;
        }
        auto derived_ptr = dynamic_cast<T*>(it->second.get());
        if (!derived_ptr) {
            throw;
        }
        return *derived_ptr;   
    }
    DataElement& get_element(std::string name) {
        return *elements.at(name);
    }
private:
    std::unordered_map<std::string, std::unique_ptr<DataElement>> elements;
};