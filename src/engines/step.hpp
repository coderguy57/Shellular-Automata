#pragma once

class Data;

struct Step
{
    virtual ~Step() = default;
    virtual void do_step(Data& data) = 0;
};
