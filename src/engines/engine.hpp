#pragma once

#include <glm/vec3.hpp>
#include <memory>
#include <iostream>
#include <unordered_map>

class FragmentProgram;
class Data;
class Texture;

class Engine
{
public:
    virtual ~Engine() {};
    virtual void step(Data& data) = 0;
    virtual Texture* current_texture() {
        return nullptr;
    };
    virtual inline glm::ivec3 get_size() const {
        return glm::ivec3{};
    };
};