#pragma once

class Program;
class Texture;
class GuiControl {
   public:
    virtual void draw() = 0;
    virtual void update(Program* program) = 0;
    virtual void post_process(Texture* texture) = 0;
};