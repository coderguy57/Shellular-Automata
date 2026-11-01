#include "rule.hpp"

#include <algorithm>
#include <cassert>

namespace slack {
Rule baseline_rule()
{
    Rule rule;
    rule.words = {
        1534876570u, 3102187896u, 383180533u, 3911937888u,
        1713008546u, 3183668977u, 163787620u, 1570659573u,
        4275609029u, 1409283414u, 1517509157u, 4185999792u,
        1785950172u, 1839866699u, 3464288272u, 894531137u,
        931560489u, 2107890683u, 69319769u, 2430523639u,
        769976078u, 2822674395u, 2653587597u, 1512850439u,
        1053467775u, 2833240997u, 1075074238u, 1129631768u,
        1352606717u, 3469685548u, 2802655431u, 2318177844u,
        3425762429u, 543502637u, 2736492674u, 3676755342u,
        1248651794u, 1223860922u, 943980818u, 330461460u,
        1717743274u, 2286157824u, 295261525u, 1713547946u,
        2339061161u, 2680507410u, 3285330708u, 2831848434u,
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u, 0u, 1083179006u, 1116152287u, 1174405259u,
    };
    return rule;
}

Rule random_rule(std::mt19937& random)
{
    Rule rule;
    std::uniform_int_distribution<std::uint32_t> sample;
    for (auto& word : rule.words)
        word = sample(random);
    return rule;
}

bool mutate_rule(Rule& rule, std::mt19937& random, float strength, float probability)
{
    assert(rule.words.size() == 64);
    if (strength <= 0.0f || probability <= 0.0f)
        return false;
    std::bernoulli_distribution flip(std::clamp(strength * probability, 0.0f, 1.0f));
    bool changed = false;
    for (std::size_t index = 0; index < 48; ++index) {
        for (unsigned bit = 0; bit < 32; ++bit) {
            if (flip(random)) {
                rule.words[index] ^= std::uint32_t{1} << bit;
                changed = true;
            }
        }
    }
    return changed;
}

}
