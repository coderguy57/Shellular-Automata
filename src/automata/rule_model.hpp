#pragma once

#include "data.hpp"

#include <cstddef>
#include <cstdint>
#include <random>

namespace rules {
struct MutationSettings {
    float strength = 0.15f;
    float rate = 0.1f;
};

class RuleModel : public DataElement {
public:
    virtual std::size_t size() const = 0;
    virtual void initialize(std::size_t index = 0) = 0;
    virtual void randomize(std::size_t index = 0) = 0;
    virtual bool mutate(MutationSettings settings, std::size_t index = 0) = 0;
    // Copying incompatible models or invalid indices fails without changing the destination.
    virtual bool copy_from(const RuleModel& source, std::size_t source_index = 0,
                           std::size_t destination_index = 0) = 0;
    virtual void save(std::size_t index = 0) = 0;
    virtual void restore(std::size_t index = 0) = 0;
    virtual void upload() = 0;
    void bind(GLuint layout) override = 0;

    void reseed(std::uint32_t value)
    {
        seed = value;
        random_.seed(seed);
    }

    MutationSettings mutation;
    std::uint32_t seed = std::random_device{}();
    bool dirty = true;

protected:
    std::mt19937 random_{seed};
};
}
