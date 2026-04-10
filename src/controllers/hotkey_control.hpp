#pragma once

#include "gui_control.hpp"
#include <string>
#include <memory>
#include <functional>

class HotkeyControl : public GuiControl
{
public:
    struct HotkeyCommand {
        char key;
        std::function<void(Data& data)> command;
        HotkeyCommand(char key_, std::function<void(Data& data)> func_) : key{key_}, command{func_} {};
    };

    HotkeyControl(std::vector<HotkeyCommand>&& hotkeys);
    ~HotkeyControl(){};
    void draw() override;
    void update(Data& data) override;
    void post_process(Data& data) override{};

private:
    std::vector<HotkeyCommand> hotkeys;
};