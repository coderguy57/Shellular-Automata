#pragma once

#include "rule_control.hpp"

namespace neural { class RuleModel; }

class NeuralRuleControl final : public RuleControl
{
public:
    explicit NeuralRuleControl(neural::RuleModel& model);

private:
    bool draw_parameters() override;
    neural::RuleModel& model_;
    int hidden_neuron_ = 0;
};
