#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <random>

#include "gui_control.hpp"

class ComputeProgram;
template <typename T> class Buffer;

class NeuralMutationControl : public GuiControl
{
public:
    explicit NeuralMutationControl(ComputeProgram& program);
    ~NeuralMutationControl() override;
    void draw() override;
    void update(Data& data) override;
    void post_process(Data&) override {}

private:
    struct Rule {
        // Softmax logits over cell RGB and the RGB means of rings 1 through 12.
        std::array<float, 1248> input_logits{}; // neuron * 39 + input
        std::array<float, 32> centers{};
        std::array<float, 32> width_parameters{}; // softplus + minimum width
        std::array<float, 96> output_weights{}; // output * 32 + hidden
        std::array<float, 3> output_bias{};

        bool operator==(const Rule&) const = default;
    };

    static Rule baseline_rule();
    void set_rule(Rule rule);
    void mutate_rule(float probability);
    void randomize_rule();
    void draw_rule_window();

    // Non-owning: the engine's program must outlive this controller.
    ComputeProgram* program_;
    Rule rule_;
    Rule saved_rule_;
    std::unique_ptr<Buffer<float>> rule_buffer_;
    bool rule_dirty_ = true;
    float mutation_strength_ = 0.15f;
    float mutation_rate_ = 0.1f;
    std::uint32_t seed_ = std::random_device{}();
    std::mt19937 random_{seed_};
    int hidden_neuron_ = 0;
    bool show_rule_window_ = false;
};
