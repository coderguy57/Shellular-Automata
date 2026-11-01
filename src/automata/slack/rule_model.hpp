#pragma once

#include "automata/rule_model.hpp"
#include "rule.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace slack {
void bind_rule(FragmentProgram& program, const Rule& rule);

class RuleModel final : public rules::RuleModel {
public:
    explicit RuleModel(std::size_t count = 1);

    std::size_t size() const override;
    void initialize(std::size_t index = 0) override;
    void randomize(std::size_t index = 0) override;
    bool mutate(rules::MutationSettings settings, std::size_t index = 0) override;
    bool copy_from(const rules::RuleModel& source, std::size_t source_index = 0,
                   std::size_t destination_index = 0) override;
    void save(std::size_t index = 0) override;
    void restore(std::size_t index = 0) override;
    void upload() override;
    void bind(GLuint layout) override;

    Rule& rule(std::size_t index = 0);
    const Rule& rule(std::size_t index = 0) const;

private:
    void assign(std::size_t index, const Rule& rule);

    std::vector<Rule> rules_;
    std::vector<Rule> saved_rules_;
    std::unique_ptr<Buffer<std::uint32_t>> buffer_;
};
}
