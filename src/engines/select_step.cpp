#include "select_step.hpp"

#include <utility>

SelectStep::SelectStep(std::function<std::size_t()> select, std::vector<std::unique_ptr<Step>> options)
    : select_(std::move(select)), options_(std::move(options)) {}

void SelectStep::do_step(Data& data)
{
    if (!select_)
        return;
    const auto index = select_();
    if (index < options_.size() && options_[index])
        options_[index]->do_step(data);
}
