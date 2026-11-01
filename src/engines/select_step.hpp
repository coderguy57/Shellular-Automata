#pragma once

#include "step.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

class SelectStep final : public Step {
public:
    SelectStep(std::function<std::size_t()> select, std::vector<std::unique_ptr<Step>> options);
    void do_step(Data& data) override;

private:
    std::function<std::size_t()> select_;
    std::vector<std::unique_ptr<Step>> options_;
};
