#include "rule.hpp"

#include <algorithm>
#include <cmath>

namespace neural {
namespace {
constexpr int output_count = 3;
constexpr float minimum_width = 0.005f;

void radial_logits(float* logits, float radius, float spread, const std::array<float, 3>& channels)
{
    constexpr std::array<int, 13> counts{1, 8, 12, 16, 32, 28, 40, 40, 48, 68, 56, 72, 68};
    for (int ring = 0; ring <= ring_count; ++ring) {
        const float distance = (ring - radius) / spread;
        for (int channel = 0; channel < output_count; ++channel)
            logits[3 * ring + channel] = std::clamp(
                std::log(static_cast<float>(counts[ring])) - 0.5f * distance * distance + channels[channel],
                -input_limit, input_limit);
    }
}
}

float response_width(float parameter)
{
    return std::max(parameter, 0.0f) + std::log1p(std::exp(-std::abs(parameter))) + minimum_width;
}

float width_parameter(float width)
{
    return std::log(std::expm1(width - minimum_width));
}

Rule baseline_rule()
{
    Rule rule;
    const auto configure = [&](int neuron, int channel, float radius, float spread,
                               float center, float width) {
        std::array<float, 3> channels{-6.0f, -6.0f, -6.0f};
        channels[channel] = 0.0f;
        radial_logits(rule.input_logits.data() + neuron * input_count, radius, spread, channels);
        rule.centers[neuron] = center;
        rule.width_parameters[neuron] = width_parameter(width);
    };
    for (int neuron = 0; neuron < neuron_count; ++neuron)
        configure(neuron, neuron % 3, 2.0f + 2.0f * (neuron % 6), 1.25f,
                  0.25f + 0.1f * (neuron % 5), 0.08f + 0.02f * (neuron % 3));
    for (int channel = 0; channel < output_count; ++channel) {
        // Local growth windows compete with broader inhibition and bright-cell suppression.
        const int activation = 2 * channel;
        const int decay = activation + 1;
        configure(activation, channel, 3.0f, 1.5f, 0.30f, 0.10f);
        configure(decay, channel, 7.0f, 2.5f, 0.65f, 0.20f);
        rule.output_weights[channel * neuron_count + activation] = 0.12f;
        rule.output_weights[channel * neuron_count + decay] = -0.10f;
        rule.output_bias[channel] = -0.01f;
        const int coupling = 6 + channel;
        configure(coupling, (channel + 1) % 3, 4.0f, 1.5f, 0.35f, 0.12f);
        rule.output_weights[channel * neuron_count + coupling] = 0.02f;
        const int damping = 9 + channel;
        configure(damping, channel, 0.0f, 0.5f, 0.95f, 0.20f);
        rule.output_weights[channel * neuron_count + damping] = -0.06f;
    }
    return rule;
}

Rule random_rule(std::mt19937& random)
{
    Rule rule;
    const auto sample = [&](float low, float high) {
        return std::uniform_real_distribution<float>(low, high)(random);
    };
    for (int neuron = 0; neuron < neuron_count; ++neuron) {
        std::array<float, 3> channels;
        for (float& channel : channels)
            channel = sample(-4.0f, 4.0f);
        radial_logits(rule.input_logits.data() + neuron * input_count,
                      sample(0.0f, 12.0f), sample(0.6f, 3.0f), channels);
        for (int feature = 0; feature < input_count; ++feature) {
            auto& logit = rule.input_logits[neuron * input_count + feature];
            logit = std::clamp(logit + sample(-0.4f, 0.4f), -input_limit, input_limit);
        }
        rule.centers[neuron] = sample(0.1f, 0.9f);
        rule.width_parameters[neuron] = width_parameter(sample(0.025f, 0.18f));
    }
    for (float& weight : rule.output_weights)
        weight = sample(-0.1f, 0.1f);
    for (float& bias : rule.output_bias)
        bias = sample(-0.025f, -0.005f);
    return rule;
}

bool mutate_rule(Rule& rule, std::mt19937& random, float strength, float probability)
{
    if (strength <= 0.0f || probability <= 0.0f)
        return false;

    bool changed = false;
    std::bernoulli_distribution mutate{probability};
    std::normal_distribution<float> noise{0.0f, strength};
    const auto perturb = [&](auto& coefficients, float low, float high, float scale) {
        for (float& coefficient : coefficients) {
            if (mutate(random)) {
                const float value = std::clamp(coefficient + scale * noise(random), low, high);
                changed |= value != coefficient;
                coefficient = value;
            }
        }
    };
    perturb(rule.input_logits, -input_limit, input_limit, 2.0f);
    perturb(rule.centers, 0.0f, 1.0f, 0.2f);
    perturb(rule.width_parameters, -7.0f, 0.0f, 1.0f);
    perturb(rule.output_weights, -output_limit, output_limit, 0.1f);
    perturb(rule.output_bias, -bias_limit, bias_limit, 0.02f);
    return changed;
}

std::array<float, coefficient_count> pack_rule(const Rule& rule)
{
    std::array<float, coefficient_count> coefficients;
    auto output = coefficients.begin();
    // Fold differentiable parameter transforms into upload, not every cell invocation.
    for (int neuron = 0; neuron < neuron_count; ++neuron) {
        const auto first = rule.input_logits.begin() + neuron * input_count;
        const float maximum = *std::max_element(first, first + input_count);
        float total = 0.0f;
        for (int feature = 0; feature < input_count; ++feature) {
            const float weight = std::exp(first[feature] - maximum);
            output[feature] = weight;
            total += weight;
        }
        for (int feature = 0; feature < input_count; ++feature)
            output[feature] /= total;
        output += input_count;
    }
    output = std::copy(rule.centers.begin(), rule.centers.end(), output);
    for (float parameter : rule.width_parameters)
        *output++ = response_width(parameter);
    output = std::copy(rule.output_weights.begin(), rule.output_weights.end(), output);
    std::copy(rule.output_bias.begin(), rule.output_bias.end(), output);
    return coefficients;
}
}
