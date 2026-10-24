#include "neural_mutation_control.hpp"
#include "detail/shortcuts.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"
#include "render/shader.hpp"

namespace {
constexpr int ring_count = 12;
constexpr int input_count = 3 * (ring_count + 1);
constexpr int hidden_count = 32;
constexpr int output_count = 3;
constexpr int coefficient_count = hidden_count * input_count + 2 * hidden_count
    + output_count * hidden_count + output_count;
constexpr float input_limit = 12.0f;
constexpr float output_limit = 0.2f;
constexpr float bias_limit = 0.05f;
constexpr float minimum_width = 0.005f;

float response_width(float parameter)
{
    return std::max(parameter, 0.0f) + std::log1p(std::exp(-std::abs(parameter))) + minimum_width;
}

float width_parameter(float width)
{
    return std::log(std::expm1(width - minimum_width));
}

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

NeuralMutationControl::NeuralMutationControl(ComputeProgram& program)
    : program_{&program}, rule_{baseline_rule()}, saved_rule_{rule_},
      rule_buffer_{std::make_unique<Buffer<float>>(coefficient_count)}
{
}

NeuralMutationControl::~NeuralMutationControl() = default;

NeuralMutationControl::Rule NeuralMutationControl::baseline_rule()
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
    for (int neuron = 0; neuron < hidden_count; ++neuron)
        configure(neuron, neuron % 3, 2.0f + 2.0f * (neuron % 6), 1.25f,
                  0.25f + 0.1f * (neuron % 5), 0.08f + 0.02f * (neuron % 3));
    for (int channel = 0; channel < output_count; ++channel) {
        // Local growth windows compete with broader inhibition and bright-cell suppression.
        const int activation = 2 * channel;
        const int decay = activation + 1;
        configure(activation, channel, 3.0f, 1.5f, 0.30f, 0.10f);
        configure(decay, channel, 7.0f, 2.5f, 0.65f, 0.20f);
        rule.output_weights[channel * hidden_count + activation] = 0.12f;
        rule.output_weights[channel * hidden_count + decay] = -0.10f;
        rule.output_bias[channel] = -0.01f;
        const int coupling = 6 + channel;
        configure(coupling, (channel + 1) % 3, 4.0f, 1.5f, 0.35f, 0.12f);
        rule.output_weights[channel * hidden_count + coupling] = 0.02f;
        const int damping = 9 + channel;
        configure(damping, channel, 0.0f, 0.5f, 0.95f, 0.20f);
        rule.output_weights[channel * hidden_count + damping] = -0.06f;
    }
    return rule;
}

void NeuralMutationControl::set_rule(Rule rule)
{
    if (rule_ == rule)
        return;
    rule_ = std::move(rule);
    rule_dirty_ = true;
}

void NeuralMutationControl::mutate_rule(float probability)
{
    if (mutation_strength_ <= 0.0f || probability <= 0.0f)
        return;

    std::bernoulli_distribution mutate{probability};
    std::normal_distribution<float> noise{0.0f, mutation_strength_};
    const auto perturb = [&](auto& coefficients, float low, float high, float scale) {
        for (float& coefficient : coefficients) {
            if (mutate(random_)) {
                const float changed = std::clamp(coefficient + scale * noise(random_), low, high);
                rule_dirty_ |= changed != coefficient;
                coefficient = changed;
            }
        }
    };
    perturb(rule_.input_logits, -input_limit, input_limit, 2.0f);
    perturb(rule_.centers, 0.0f, 1.0f, 0.2f);
    perturb(rule_.width_parameters, -7.0f, 0.0f, 1.0f);
    perturb(rule_.output_weights, -output_limit, output_limit, 0.1f);
    perturb(rule_.output_bias, -bias_limit, bias_limit, 0.02f);
}

void NeuralMutationControl::randomize_rule()
{
    Rule rule;
    const auto sample = [&](float low, float high) {
        return std::uniform_real_distribution<float>(low, high)(random_);
    };
    for (int neuron = 0; neuron < hidden_count; ++neuron) {
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
    set_rule(std::move(rule));
}

void NeuralMutationControl::draw_rule_window()
{
    ImGui::SetNextWindowSizeConstraints(ImVec2(360, 0), ImVec2(480, 600));
    if (ImGui::Begin("Neural rule controls", &show_rule_window_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushItemWidth(240);
        if (ImGui::Button("Mutate"))
            mutate_rule(mutation_rate_);
        ImGui::SameLine();
        if (ImGui::Button("Randomize"))
            randomize_rule();
        ImGui::SameLine();
        if (ImGui::Button("Reset rule"))
            set_rule(baseline_rule());
        if (ImGui::Button("Save"))
            saved_rule_ = rule_;
        ImGui::SameLine();
        if (ImGui::Button("Restore"))
            set_rule(saved_rule_);

        ImGui::Separator();
        ImGui::SliderFloat("Mutation strength", &mutation_strength_, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Mutation rate", &mutation_rate_, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        const std::uint32_t seed_step = 1u;
        if (ImGui::InputScalar("Seed", ImGuiDataType_U32, &seed_, &seed_step))
            random_.seed(seed_);

        ImGui::Separator();
        ImGui::SliderInt("RBF neuron", &hidden_neuron_, 0, hidden_count - 1, "%d", ImGuiSliderFlags_AlwaysClamp);
        rule_dirty_ |= ImGui::SliderFloat("Preferred density", &rule_.centers[hidden_neuron_],
            0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        float width = response_width(rule_.width_parameters[hidden_neuron_]);
        if (ImGui::SliderFloat("Response width", &width, response_width(-7.0f), response_width(0.0f),
                "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
            rule_.width_parameters[hidden_neuron_] = width_parameter(width);
            rule_dirty_ = true;
        }
        for (int group = 0; group <= ring_count; ++group) {
            const std::string label = group == 0 ? "Cell RGB logits" :
                "Ring " + std::to_string(group) + " RGB logits";
            rule_dirty_ |= ImGui::SliderFloat3(label.c_str(),
                rule_.input_logits.data() + hidden_neuron_ * input_count + 3 * group,
                -input_limit, input_limit, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        }
        // Outgoing connections are strided by output, not contiguous by neuron.
        std::array<float, output_count> outgoing;
        for (int output = 0; output < output_count; ++output)
            outgoing[output] = rule_.output_weights[output * hidden_count + hidden_neuron_];
        if (ImGui::SliderFloat3("Outgoing RGB", outgoing.data(),
                -output_limit, output_limit, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
            for (int output = 0; output < output_count; ++output)
                rule_.output_weights[output * hidden_count + hidden_neuron_] = outgoing[output];
            rule_dirty_ = true;
        }
        ImGui::Separator();
        rule_dirty_ |= ImGui::SliderFloat3("Output bias RGB", rule_.output_bias.data(),
            -bias_limit, bias_limit, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::PopItemWidth();
    }
    ImGui::End();
}

void NeuralMutationControl::draw()
{
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Neural rule")) {
            ImGui::MenuItem("Controls", nullptr, &show_rule_window_);
            ImGui::Separator();
            if (ImGui::MenuItem("Mutate", "V"))
                mutate_rule(mutation_rate_);
            if (ImGui::MenuItem("Randomize"))
                randomize_rule();
            if (ImGui::MenuItem("Reset rule"))
                set_rule(baseline_rule());
            ImGui::Separator();
            if (ImGui::MenuItem("Save", "S"))
                saved_rule_ = rule_;
            if (ImGui::MenuItem("Restore", "L"))
                set_rule(saved_rule_);
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::EndMainMenuBar();
    }

    if (show_rule_window_)
        draw_rule_window();

    if (controller_detail::shortcuts_allowed()) {
        if (ImGui::IsKeyPressed('S', false))
            saved_rule_ = rule_;
        if (ImGui::IsKeyPressed('L', false))
            set_rule(saved_rule_);
        if (ImGui::IsKeyPressed('V', false))
            mutate_rule(mutation_rate_);
    }
}

void NeuralMutationControl::update(Data&)
{
    if (!program_->use())
        return;

    if (rule_dirty_) {
        // Explicit packing matches the shader's std430 scalar arrays without struct padding.
        std::vector<float> coefficients;
        coefficients.reserve(coefficient_count);
        const auto append = [&](const auto& values) {
            coefficients.insert(coefficients.end(), values.begin(), values.end());
        };
        // Fold differentiable parameter transforms into upload, not every cell invocation.
        for (int neuron = 0; neuron < hidden_count; ++neuron) {
            const auto first = rule_.input_logits.begin() + neuron * input_count;
            const float maximum = *std::max_element(first, first + input_count);
            float total = 0.0f;
            const auto offset = coefficients.size();
            for (int feature = 0; feature < input_count; ++feature) {
                const float weight = std::exp(first[feature] - maximum);
                coefficients.push_back(weight);
                total += weight;
            }
            for (int feature = 0; feature < input_count; ++feature)
                coefficients[offset + feature] /= total;
        }
        append(rule_.centers);
        for (float parameter : rule_.width_parameters)
            coefficients.push_back(response_width(parameter));
        append(rule_.output_weights);
        append(rule_.output_bias);
        rule_buffer_->write(std::move(coefficients));
        rule_dirty_ = false;
    }
    rule_buffer_->set_layout(4);
    glUseProgram(0);
}
