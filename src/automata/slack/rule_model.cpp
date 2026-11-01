#include "rule_model.hpp"

#include "render/shader.hpp"

#include <bit>
#include <cassert>
#include <utility>

namespace {
constexpr std::size_t shader_rule_words = 47;
constexpr std::size_t packed_rule_words = 50;
}

namespace slack {
void bind_rule(FragmentProgram& program, const Rule& rule)
{
    assert(rule.words.size() == 64);
    program.set_uniform("nb", 12, rule.words.data());
    program.set_uniform("ur", 24, rule.words.data() + 12);
    program.set_uniform("us", 2, rule.words.data() + 36);
    program.set_uniform("ch", 3, rule.words.data() + 38);
    program.set_uniform("ch2", 3, rule.words.data() + 41);
    program.set_uniform("ch3", 3, rule.words.data() + 44);
    program.set_uniform("mode", static_cast<std::uint32_t>(rule.mode));
    program.set_uniform("zoom", rule.zoom);
    program.set_uniform("scale", rule.scale);
}

RuleModel::RuleModel(std::size_t count)
    : rules_(count, baseline_rule()), saved_rules_(rules_) {}

std::size_t RuleModel::size() const
{
    return rules_.size();
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
    assign(destination_index, model->rule(source_index));
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

    std::vector<std::uint32_t> packed;
    packed.reserve(size() * packed_rule_words);
    for (const auto& rule : rules_) {
        assert(rule.words.size() == 64);
        packed.insert(packed.end(), rule.words.begin(), rule.words.begin() + shader_rule_words);
        packed.push_back(static_cast<std::uint32_t>(rule.mode));
        packed.push_back(std::bit_cast<std::uint32_t>(rule.zoom));
        packed.push_back(std::bit_cast<std::uint32_t>(rule.scale));
    }
    if (!buffer_)
        buffer_ = std::make_unique<Buffer<std::uint32_t>>(size() * packed_rule_words);
    buffer_->write(std::move(packed));
    dirty = false;
}

void RuleModel::bind(GLuint layout)
{
    upload();
    buffer_->set_layout(layout);
}

Rule& RuleModel::rule(std::size_t index)
{
    return rules_.at(index);
}

const Rule& RuleModel::rule(std::size_t index) const
{
    return rules_.at(index);
}

void RuleModel::assign(std::size_t index, const Rule& rule)
{
    auto& current = rules_.at(index);
    if (current == rule)
        return;
    current = rule;
    dirty = true;
}
}
