#pragma once

#include "automata/slack/rule_model.hpp"
#include "rule_control.hpp"

class SlackRuleControl final : public RuleControl {
public:
    explicit SlackRuleControl(slack::RuleModel& model);

private:
    bool draw_parameters() override;

    slack::RuleModel& model_;
};
