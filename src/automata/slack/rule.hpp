#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace slack {
struct Rule {
    std::vector<std::uint32_t> words = std::vector<std::uint32_t>(64);
    float scale = 70.0f;
    float zoom = 0.0f;
    int mode = 0;

    bool operator==(const Rule&) const = default;
};

Rule baseline_rule();
Rule random_rule(std::mt19937& random);

bool mutate_rule(Rule& rule, std::mt19937& random, float strength, float probability);
}
