#include "rule_model.hpp"

#include <utility>

namespace neural {
RuleModel::RuleModel(std::size_t count)
    : rules_(count, baseline_rule()), saved_rules_(rules_) {}

std::size_t RuleModel::size() const
{
    return rules_.size();
}

Rule& RuleModel::rule(std::size_t index)
{
    return rules_.at(index);
}

const Rule& RuleModel::rule(std::size_t index) const
{
    return rules_.at(index);
}

void RuleModel::assign(std::size_t index, const Rule& value)
{
    auto& current = rules_.at(index);
    if (current != value) {
        current = value;
        dirty = true;
    }
}

void RuleModel::initialize(std::size_t index)
{
    assign(index, baseline_rule());
}

void RuleModel::randomize(std::size_t index)
{
    assign(index, random_rule(random_));
}

bool RuleModel::mutate(rules::MutationSettings settings, std::size_t index)
{
    const bool changed = mutate_rule(rules_.at(index), random_, settings.strength, settings.rate);
    dirty |= changed;
    return changed;
}

bool RuleModel::copy_from(const rules::RuleModel& source, std::size_t source_index,
                          std::size_t destination_index)
{
    const auto* model = dynamic_cast<const RuleModel*>(&source);
    if (!model || source_index >= model->size() || destination_index >= size())
        return false;
    assign(destination_index, model->rules_[source_index]);
    return true;
}

void RuleModel::save(std::size_t index)
{
    saved_rules_.at(index) = rule(index);
}

void RuleModel::restore(std::size_t index)
{
    assign(index, saved_rules_.at(index));
}

void RuleModel::upload()
{
    if (buffer_ && !dirty)
        return;
    if (!buffer_)
        buffer_ = std::make_unique<Buffer<float>>(size() * coefficient_count);
    std::vector<float> packed;
    packed.reserve(size() * coefficient_count);
    for (const auto& rule : rules_) {
        const auto values = pack_rule(rule);
        packed.insert(packed.end(), values.begin(), values.end());
    }
    buffer_->write(std::move(packed));
    dirty = false;
}

void RuleModel::bind(GLuint layout)
{
    upload();
    buffer_->set_layout(layout);
}
}
