#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "automata/neural/rule_model.hpp"
#include "automata/rule_model.hpp"
#include "automata/slack/rule_model.hpp"

#include <cstdint>
#include <random>

TEST_CASE("Concrete rule models initialize and restore through the base interface without a GPU", "[rules][cpu]") {
    const bool use_slack = GENERATE(false, true);
    const auto exercise = [](auto& model, const auto& baseline) {
        rules::RuleModel& base = model;
        REQUIRE(base.size() == 3);
        for (std::size_t index = 0; index < base.size(); ++index)
            CHECK(model.rule(index) == baseline);
        CHECK(base.dirty);
        CHECK(base.mutation.strength == 0.15f);
        CHECK(base.mutation.rate == 0.1f);

        base.dirty = false;
        base.initialize(1);
        CHECK_FALSE(base.dirty);
        base.save(1);
        base.restore(1);
        CHECK_FALSE(base.dirty);

        base.reseed(99871u);
        base.randomize(1);
        const auto saved = model.rule(1);
        REQUIRE(saved != baseline);
        CHECK(base.dirty);
        base.dirty = false;
        base.save(1);
        CHECK_FALSE(base.dirty);
        base.initialize(1);
        CHECK(base.dirty);
        CHECK(model.rule(1) == baseline);
        base.restore(1);
        CHECK(model.rule(1) == saved);
        CHECK(model.rule(0) == baseline);
        CHECK(model.rule(2) == baseline);
        base.dirty = false;
        base.restore(1);
        CHECK_FALSE(base.dirty);

        base.randomize(0);
        base.restore(0);
        CHECK(model.rule(0) == baseline);
        CHECK(model.rule(1) == saved);
    };
    if (use_slack) {
        slack::RuleModel model(3);
        exercise(model, slack::baseline_rule());
    } else {
        neural::RuleModel model(3);
        exercise(model, neural::baseline_rule());
    }
}

TEST_CASE("Rule model randomization and mutation match the CPU algorithms and private random stream", "[rules][cpu]") {
    const bool use_slack = GENERATE(false, true);
    const auto exercise = [](auto& model, auto randomize, auto mutate) {
        rules::RuleModel& base = model;
        base.reseed(44519u);
        std::mt19937 reference(44519u);
        auto expected = randomize(reference);
        base.randomize(1);
        CHECK(model.rule(1) == expected);
        base.dirty = false;
        CHECK_FALSE(base.mutate({0.0f, 1.0f}, 1));
        CHECK_FALSE(base.mutate({1.0f, 0.0f}, 1));
        CHECK_FALSE(base.dirty);
        CHECK(model.rule(1) == expected);
        for (int iteration = 0; iteration < 4; ++iteration) {
            const bool changed = mutate(expected, reference, 0.4f, 0.3f);
            CHECK(base.mutate({0.4f, 0.3f}, 1) == changed);
            CHECK(model.rule(1) == expected);
            CHECK(base.dirty == changed);
            base.dirty = false;
        }
        expected = randomize(reference);
        base.randomize(1);
        CHECK(model.rule(1) == expected);
        base.save(1);
        base.mutation = {0.8f, 0.7f};
        base.reseed(751u);
        base.randomize(1);
        base.restore(1);
        CHECK(model.rule(1) == expected);
        CHECK(base.mutation.strength == 0.8f);
        CHECK(base.mutation.rate == 0.7f);
        CHECK(base.seed == 751u);
        reference.seed(751u);
        randomize(reference);
        base.randomize(1);
        CHECK(model.rule(1) == randomize(reference));
    };
    if (use_slack) {
        slack::RuleModel model(2);
        exercise(model, slack::random_rule, slack::mutate_rule);
    } else {
        neural::RuleModel model(2);
        exercise(model, neural::random_rule, neural::mutate_rule);
    }
}

TEST_CASE("Rule models own copied rules and saved snapshots independently", "[rules][cpu]") {
    const bool use_slack = GENERATE(false, true);
    const auto exercise = [](auto& source, auto& destination) {
        rules::RuleModel& parent = source;
        rules::RuleModel& child = destination;
        const auto baseline = destination.rule(0);
        parent.reseed(5819u);
        parent.randomize(1);
        const auto original = source.rule(1);
        child.dirty = false;
        REQUIRE(child.copy_from(parent, 1, 2));
        CHECK(child.dirty);
        CHECK(destination.rule(2) == original);
        child.dirty = false;
        REQUIRE(child.copy_from(parent, 1, 2));
        CHECK_FALSE(child.dirty);
        child.save(2);
        CHECK_FALSE(child.dirty);
        parent.randomize(1);
        REQUIRE(source.rule(1) != original);
        CHECK(destination.rule(2) == original);

        REQUIRE(child.copy_from(child, 2, 0));
        REQUIRE(child.copy_from(child, 2, 2));
        child.initialize(2);
        CHECK(destination.rule(2) == baseline);
        CHECK(destination.rule(0) == original);
        child.restore(2);
        CHECK(destination.rule(2) == original);
        child.restore(0);
        CHECK(destination.rule(0) == baseline);
        CHECK(destination.rule(2) == original);
        parent.initialize(1);
        CHECK(destination.rule(2) == original);
    };
    if (use_slack) {
        slack::RuleModel source(2);
        slack::RuleModel destination(3);
        exercise(source, destination);
    } else {
        neural::RuleModel source(2);
        neural::RuleModel destination(3);
        exercise(source, destination);
    }
}

TEST_CASE("Rule model copies reject incompatible models and invalid indices without changing the destination", "[rules][cpu]") {
    neural::RuleModel neural(2);
    slack::RuleModel slack(2);
    rules::RuleModel& neural_base = neural;
    rules::RuleModel& slack_base = slack;
    const auto neural_original = neural.rule(1);
    const auto slack_original = slack.rule(1);
    neural.dirty = slack.dirty = false;
    CHECK_FALSE(neural_base.copy_from(slack_base, 0, 1));
    CHECK_FALSE(slack_base.copy_from(neural_base, 0, 1));
    CHECK_FALSE(neural_base.copy_from(neural_base, 2, 1));
    CHECK_FALSE(neural_base.copy_from(neural_base, 0, 2));
    CHECK_FALSE(slack_base.copy_from(slack_base, 2, 1));
    CHECK_FALSE(slack_base.copy_from(slack_base, 0, 2));
    CHECK(neural.rule(1) == neural_original);
    CHECK(slack.rule(1) == slack_original);
    CHECK_FALSE(neural.dirty);
    CHECK_FALSE(slack.dirty);
}

TEST_CASE("Reseeding a rule model changes only its own private random sequence", "[rules][cpu]") {
    const bool use_slack = GENERATE(false, true);
    const auto exercise = [](auto& first, auto& second) {
        rules::RuleModel& left = first;
        rules::RuleModel& right = second;
        left.reseed(12477u);
        right.reseed(12477u);
        left.randomize();
        right.randomize();
        CHECK(first.rule() == second.rule());
        const auto saved = first.rule();
        left.save();
        left.dirty = false;
        left.reseed(12477u);
        CHECK_FALSE(left.dirty);
        CHECK(first.rule() == saved);
        left.randomize();
        CHECK(first.rule() == saved);
        right.randomize();
        const auto second_draw = second.rule();
        REQUIRE(second_draw != saved);
        left.randomize();
        CHECK(first.rule() == second_draw);
        left.restore();
        CHECK(first.rule() == saved);
        CHECK(second.rule() == second_draw);
    };
    if (use_slack) {
        slack::RuleModel first;
        slack::RuleModel second;
        exercise(first, second);
    } else {
        neural::RuleModel first;
        neural::RuleModel second;
        exercise(first, second);
    }
}

TEST_CASE("Slack model mutation changes only active words and snapshots preserve all metadata", "[slack][rules][cpu]") {
    slack::RuleModel model;
    rules::RuleModel& base = model;
    base.reseed(4419u);
    base.randomize();
    model.rule().scale = 113.0f;
    model.rule().zoom = 0.3f;
    model.rule().mode = 2;
    const auto original = model.rule();
    base.save();
    REQUIRE(base.mutate({1.0f, 1.0f}));
    REQUIRE(model.rule().words.size() == original.words.size());
    for (std::size_t word = 0; word < original.words.size(); ++word) {
        CAPTURE(word);
        CHECK(model.rule().words[word] == (word < 48 ? ~original.words[word] : original.words[word]));
    }
    CHECK(model.rule().scale == original.scale);
    CHECK(model.rule().zoom == original.zoom);
    CHECK(model.rule().mode == original.mode);
    model.rule().scale = 1.0f;
    model.rule().zoom = 0.7f;
    model.rule().mode = 0;
    base.restore();
    CHECK(model.rule() == original);
}

TEST_CASE("Slack mutation independently flips bits using strength times probability", "[slack][rules][cpu]") {
    std::mt19937 actual_random(7531u);
    std::mt19937 reference_random(7531u);
    auto actual = slack::baseline_rule();
    auto expected = actual;
    std::bernoulli_distribution flip(0.3f * 0.4f);
    for (std::size_t word = 0; word < 48; ++word)
        for (unsigned bit = 0; bit < 32; ++bit)
            if (flip(reference_random))
                expected.words[word] ^= std::uint32_t{1} << bit;
    REQUIRE(slack::mutate_rule(actual, actual_random, 0.3f, 0.4f));
    CHECK(actual == expected);
    CHECK(actual_random == reference_random);
}
