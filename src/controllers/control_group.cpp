#include "control_group.hpp"

#include <utility>

ControlGroup::ControlGroup(std::function<bool()> enabled, std::vector<std::unique_ptr<GuiControl>> controls)
    : enabled_(std::move(enabled)), controls_(std::move(controls)) {}

void ControlGroup::draw()
{
    if (enabled_())
        for (auto& control : controls_) control->draw();
}

void ControlGroup::update(Data& data)
{
    if (enabled_())
        for (auto& control : controls_) control->update(data);
}

void ControlGroup::post_process(Data& data)
{
    if (enabled_())
        for (auto& control : controls_) control->post_process(data);
}
