#pragma once

#include <array>
#include <random>

namespace neural {
inline constexpr int ring_count = 12;
inline constexpr int input_count = 39;
inline constexpr int neuron_count = 32;
inline constexpr int coefficient_count = 1411;
inline constexpr float input_limit = 12.0f;
inline constexpr float output_limit = 0.2f;
inline constexpr float bias_limit = 0.05f;

struct Rule {
    // Softmax logits over cell RGB and the RGB means of rings 1 through 12.
    std::array<float, input_count * neuron_count> input_logits{};
    std::array<float, neuron_count> centers{};
    std::array<float, neuron_count> width_parameters{};
    std::array<float, 3 * neuron_count> output_weights{};
    std::array<float, 3> output_bias{};

    bool operator==(const Rule&) const = default;
};

Rule baseline_rule();
Rule random_rule(std::mt19937& random);
bool mutate_rule(Rule& rule, std::mt19937& random, float strength, float probability);
std::array<float, coefficient_count> pack_rule(const Rule& rule);
float response_width(float parameter);
float width_parameter(float width);

}
