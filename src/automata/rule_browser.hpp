#pragma once

#include "rule_model.hpp"

#include <cstdint>

namespace rule_browser {
inline constexpr int browser_columns = 4;
inline constexpr int browser_count = browser_columns * browser_columns;
inline constexpr int preview_size = 256;

struct State final : DataElement {
    bool enabled = false;
    int selected = 0;
    std::uint64_t generation = 0;
    std::uint64_t steps = 0;
    rules::MutationSettings mutation;

    void bind(GLuint) override {}
};
}
