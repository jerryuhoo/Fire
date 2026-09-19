#include <Utility/ClapTail.h>
#include <catch2/catch_test_macros.hpp>
#include <array>

TEST_CASE("CLAP tail conversion preserves frozen tails without overflowing integer rounding", "[clouds][clap][tail]")
{
    using fire::utility::tailSecondsToClapSamples;
    constexpr auto infinite = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    const auto infinity = std::numeric_limits<double>::infinity();
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    for (double rate : {8000.0, 44100.0, 48000.0, 192000.0, 384000.0})
    {
        CAPTURE(rate);
        CHECK(tailSecondsToClapSamples(infinity, rate) == infinite);
        CHECK(tailSecondsToClapSamples(std::numeric_limits<double>::max(), rate) == infinite);
        for (double empty : {0.0, -1.0, -infinity, nan})
            CHECK(tailSecondsToClapSamples(empty, rate) == 0);
    }
    for (double invalidRate : {0.0, -1.0, infinity, nan})
        CHECK(tailSecondsToClapSamples(infinity, invalidRate) == 0);
    CHECK(tailSecondsToClapSamples(180.0, 48000.0) == 8640000);
    CHECK(tailSecondsToClapSamples(1.49, 1.0) == 1);
    CHECK(tailSecondsToClapSamples(1.5, 1.0) == 2);
    CHECK(tailSecondsToClapSamples(static_cast<double>(infinite) - 1.0, 1.0) == infinite - 1);
    CHECK(tailSecondsToClapSamples(static_cast<double>(infinite) - 0.1, 1.0) == infinite);
    CHECK(tailSecondsToClapSamples(static_cast<double>(infinite), 1.0) == infinite);
    CHECK(tailSecondsToClapSamples(static_cast<double>(infinite) + 1.0, 1.0) == infinite);
}
