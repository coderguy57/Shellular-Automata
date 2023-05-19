#pragma once

class Program;
class Texture;
class Data;
class GuiControl
{
public:
    virtual ~GuiControl() = default;
    virtual void draw() = 0;
    virtual void update(Data& data) = 0;
    virtual void post_process(Data& data) = 0;
};