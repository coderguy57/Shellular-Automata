#include "neural_rule_control.hpp"

#include "automata/neural/rule_model.hpp"
#include "imgui.h"

#include <array>
#include <string>

namespace {
using neural::bias_limit;
using neural::input_count;
using neural::input_limit;
using neural::neuron_count;
using neural::output_limit;
using neural::ring_count;
constexpr int output_count = 3;
}

NeuralRuleControl::NeuralRuleControl(neural::RuleModel& model)
    : RuleControl(model, "Neural rule"), model_(model) {}

bool NeuralRuleControl::draw_parameters()
{
    auto& rule = model_.rule();
    bool changed = false;
    ImGui::SliderInt("RBF neuron", &hidden_neuron_, 0, neuron_count - 1, "%d", ImGuiSliderFlags_AlwaysClamp);
    changed |= ImGui::SliderFloat("Preferred density", &rule.centers[hidden_neuron_],
        0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    float width = neural::response_width(rule.width_parameters[hidden_neuron_]);
    if (ImGui::SliderFloat("Response width", &width, neural::response_width(-7.0f), neural::response_width(0.0f),
            "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
        rule.width_parameters[hidden_neuron_] = neural::width_parameter(width);
        changed = true;
    }
    for (int group = 0; group <= ring_count; ++group) {
        const std::string label = group == 0 ? "Cell RGB logits" :
            "Ring " + std::to_string(group) + " RGB logits";
        changed |= ImGui::SliderFloat3(label.c_str(),
            rule.input_logits.data() + hidden_neuron_ * input_count + 3 * group,
            -input_limit, input_limit, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    }
    // Outgoing connections are strided by output, not contiguous by neuron.
    std::array<float, output_count> outgoing;
    for (int output = 0; output < output_count; ++output)
        outgoing[output] = rule.output_weights[output * neuron_count + hidden_neuron_];
    if (ImGui::SliderFloat3("Outgoing RGB", outgoing.data(),
            -output_limit, output_limit, "%.3f", ImGuiSliderFlags_AlwaysClamp)) {
        for (int output = 0; output < output_count; ++output)
            rule.output_weights[output * neuron_count + hidden_neuron_] = outgoing[output];
        changed = true;
    }
    ImGui::Separator();
    changed |= ImGui::SliderFloat3("Output bias RGB", rule.output_bias.data(),
        -bias_limit, bias_limit, "%.3f", ImGuiSliderFlags_AlwaysClamp);
    return changed;
}
