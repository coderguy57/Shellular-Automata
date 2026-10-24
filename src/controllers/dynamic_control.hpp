#pragma once

#include "gui_control.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

class DynamicControl : public GuiControl
{
public:
    struct Action {
        std::string label;
        std::function<void(Data&)> callback;
        std::optional<int> key = std::nullopt;
        bool show_button = true;
    };

    DynamicControl(std::string title, std::vector<Action> actions);

    void draw() override;
    void update(Data& data) override;
    void post_process(Data&) override {};

private:
    std::string _title;
    std::string _window_title;
    std::vector<Action> _actions;
    std::vector<bool> _pending;
    bool _show_controls = false;
};
