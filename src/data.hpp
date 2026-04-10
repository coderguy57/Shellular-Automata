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
    std::string name;
    virtual operator std::string() const {
        return name;
    }
};
class TextureData : public DataElement {
public:
    TextureData(uint32_t width, uint32_t height, uint32_t depth, TextureOptions options = default_options);
    TextureData(Texture&& texture);
    void bind(GLuint layout) override;
    void bind_out(GLuint layout) override;
    GLuint memory_barrier_bits() override {
        return GL_SHADER_IMAGE_ACCESS_BARRIER_BIT;
    };
    Texture& get_texture() { 
        return tex0_last_updated_ ? texture0_ : texture1_;
    }
    Texture& get_last_texture() { 
        return tex0_last_updated_ ? texture1_ : texture0_;
    }
    void update() override {
        tex0_last_updated_ = !tex0_last_updated_;
    }
    uint32_t width() const { return texture0_.width; }
    uint32_t height() const { return texture0_.height; }
    uint32_t depth() const { return texture0_.depth; }
    TextureOptions const& options() const { return texture0_.options; }
private:
    Texture texture0_;
    Texture texture1_;
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
    template<typename T>
    T* add_element(std::string name, std::unique_ptr<T> data_element) {
        auto data_element_ptr = data_element.get();
        data_element->name = name;
        elements.insert({name, std::move(data_element)});
        return data_element_ptr;
    };
    template<typename T, typename... Args>
    T* add_element(std::string name, Args&&... args) {
        auto data_element = std::make_unique<T>(std::forward<Args>(args)...);
        data_element->name = name;
        auto data_element_ptr = data_element.get();
        if (elements.count(name))
            throw;
        elements.insert({name, std::move(data_element)});
        return data_element_ptr;
    };
    template <typename T, typename TFunc>
    void for_each_element(const TFunc& func) {
        for (auto& key_val : elements) {
            auto element = dynamic_cast<T*>(key_val.second.get());
            if (element)
                func(key_val.first, *element);
        }
    }
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