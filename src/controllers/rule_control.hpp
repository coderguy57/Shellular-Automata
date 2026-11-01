#pragma once

#include "gui_control.hpp"

#include <string>

namespace rules { class RuleModel; }

class RuleControl : public GuiControl {
public:
    void draw() final;
    void update(Data&) final {}
    void post_process(Data&) final {}

protected:
    RuleControl(rules::RuleModel& model, std::string label);
    virtual bool draw_parameters() = 0;

private:
    void draw_window();

    rules::RuleModel& model_;
    std::string label_;
    std::string window_name_;
    bool show_window_ = false;
};
