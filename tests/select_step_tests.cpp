#include <catch2/catch_test_macros.hpp>

#include "data.hpp"
#include "engines/compute_shader_engine.hpp"
#include "engines/select_step.hpp"

#include <array>
#include <memory>
#include <utility>
#include <vector>

namespace {
class CountingStep final : public Step {
public:
    CountingStep(int& count, Data*& received) : count_(count), received_(received) {}
    void do_step(Data& data) override { ++count_; received_ = &data; }

private:
    int& count_;
    Data*& received_;
};
}

TEST_CASE("SelectStep executes exactly one of several alternatives", "[engine][selection]") {
    Data data;
    std::array<int, 3> counts{};
    std::array<Data*, 3> received{};
    std::vector<std::unique_ptr<Step>> alternatives;
    for (int index = 0; index < 3; ++index)
        alternatives.push_back(std::make_unique<CountingStep>(counts[index], received[index]));
    std::size_t selected = 0;
    int selections = 0;
    ComputeShaderEngine engine;
    engine.add_step<SelectStep>([&] { ++selections; return selected; }, std::move(alternatives));
    CHECK(engine.compute_steps().empty());
    for (std::size_t index : {2u, 0u, 2u, 1u}) {
        selected = index;
        engine.step(data);
    }
    CHECK(counts == std::array<int, 3>{1, 1, 2});
    CHECK(selections == 4);
    for (auto* input : received)
        CHECK(input == &data);
}

TEST_CASE("SelectStep safely skips missing and invalid selections", "[engine][selection]") {
    Data data;
    int count = 0;
    Data* received = nullptr;
    std::size_t selected = 10;
    std::vector<std::unique_ptr<Step>> alternatives;
    alternatives.push_back(std::make_unique<CountingStep>(count, received));
    alternatives.push_back(nullptr);
    SelectStep step([&] { return selected; }, std::move(alternatives));
    step.do_step(data);
    selected = 1;
    step.do_step(data);
    CHECK(count == 0);
    CHECK(received == nullptr);
    selected = 0;
    step.do_step(data);
    CHECK(count == 1);

    SelectStep empty([&] { return selected; }, {});
    empty.do_step(data);
    std::vector<std::unique_ptr<Step>> unavailable;
    unavailable.push_back(std::make_unique<CountingStep>(count, received));
    SelectStep without_selector({}, std::move(unavailable));
    without_selector.do_step(data);
    CHECK(count == 1);
}

TEST_CASE("SelectStep composes with other selectors without advancing inactive children", "[engine][selection]") {
    Data data;
    std::array<int, 3> counts{};
    std::array<Data*, 3> received{};
    std::size_t outer = 0;
    std::size_t inner = 1;
    std::vector<std::unique_ptr<Step>> inner_steps;
    inner_steps.push_back(std::make_unique<CountingStep>(counts[0], received[0]));
    inner_steps.push_back(std::make_unique<CountingStep>(counts[1], received[1]));
    std::vector<std::unique_ptr<Step>> outer_steps;
    outer_steps.push_back(std::make_unique<SelectStep>([&] { return inner; }, std::move(inner_steps)));
    outer_steps.push_back(std::make_unique<CountingStep>(counts[2], received[2]));
    SelectStep step([&] { return outer; }, std::move(outer_steps));
    step.do_step(data);
    CHECK(counts == std::array<int, 3>{0, 1, 0});
    outer = 1;
    inner = 0;
    step.do_step(data);
    CHECK(counts == std::array<int, 3>{0, 1, 1});
    outer = 0;
    step.do_step(data);
    CHECK(counts == std::array<int, 3>{1, 1, 1});
}
