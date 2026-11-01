#pragma once

#include "gui_control.hpp"

#include <functional>
#include <memory>
#include <vector>

class ControlGroup final : public GuiControl {
public:
    ControlGroup(std::function<bool()> enabled, std::vector<std::unique_ptr<GuiControl>> controls);
    void draw() override;
    void update(Data& data) override;
    void post_process(Data& data) override;

private:
    std::function<bool()> enabled_;
    std::vector<std::unique_ptr<GuiControl>> controls_;
};
