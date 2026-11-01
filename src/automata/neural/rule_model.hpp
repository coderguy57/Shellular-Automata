#pragma once

#include "rule.hpp"
#include "automata/rule_model.hpp"

#include <memory>
#include <vector>

namespace neural {
class RuleModel final : public rules::RuleModel {
public:
    explicit RuleModel(std::size_t count = 1);

    std::size_t size() const override;
    Rule& rule(std::size_t index = 0);
    const Rule& rule(std::size_t index = 0) const;

    void initialize(std::size_t index = 0) override;
    void randomize(std::size_t index = 0) override;
    bool mutate(rules::MutationSettings settings, std::size_t index = 0) override;
    bool copy_from(const rules::RuleModel& source, std::size_t source_index = 0,
                   std::size_t destination_index = 0) override;
    void save(std::size_t index = 0) override;
    void restore(std::size_t index = 0) override;
    void upload() override;
    void bind(GLuint layout) override;

private:
    void assign(std::size_t index, const Rule& value);

    std::vector<Rule> rules_;
    std::vector<Rule> saved_rules_;
    std::unique_ptr<Buffer<float>> buffer_;
};
}
