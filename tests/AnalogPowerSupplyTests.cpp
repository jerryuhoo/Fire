#include <DSP/AnalogDistortion.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

TEST_CASE("Supply sag follows the RC charge equation and recovers across sample rates",
          "[analog-physical][sag][dsp]")
{
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
    {
        CAPTURE(rate);
        fire::analog::PowerSupply supply;
        supply.prepare(rate, {.4, .08, .05, .12});
        const int count = static_cast<int>(rate * .2);
        for (int sample = 0; sample < count; ++sample) supply.advance(.75, 0);
        const auto loaded = .7 + .3 * std::exp(-static_cast<double>(count) / rate / .08);
        CHECK(supply.getVoltage() == Catch::Approx(loaded).margin(1e-10));
        CHECK(supply.getLoadCurrent() == .75);
        for (int sample = 0; sample < count; ++sample) supply.advance(0, 0);
        CHECK(supply.getVoltage() == Catch::Approx(1 - (1 - loaded) * std::exp(-static_cast<double>(count) / rate / .08)).margin(1e-10));
    }
}

TEST_CASE("Supply voltage changes clipping headroom and grid current changes the operating point",
          "[analog-physical][sag][bias][dsp]")
{
    fire::analog::PowerSupply supply;
    supply.prepare(48000, {.4, .08, .05, .12});
    for (int sample = 0; sample < 24000; ++sample) supply.advance(1, 2);
    CHECK(supply.getBias() < -.09);
    CHECK(supply.getVoltage() < .61);
    CHECK(std::abs(fire::analog::transfer(3, 4, .6f)) < std::abs(fire::analog::transfer(3, 4, 1)) * .8f);
    CHECK(std::abs(fire::analog::transfer(3, .1f, .6f, -.1f) - fire::analog::transfer(3, .1f, .6f, 0)) > .001f);
    for (int sample = 0; sample < 48000; ++sample) supply.advance(0, 0);
    CHECK(supply.getVoltage() > .999);
    CHECK(std::abs(supply.getBias()) < .0001);
    supply.reset(); CHECK(supply.getVoltage() == 1); CHECK(supply.getBias() == 0);
}

TEST_CASE("Amplifier loading depletes the rail and changes subsequent quiet-note response",
          "[analog-physical][sag][dsp][history]")
{
    fire::analog::Stage loaded, fresh;
    loaded.prepare(48000); fresh.prepare(48000);
    loaded.setProfile(15, 48000); fresh.setProfile(15, 48000); // Tweed
    for (int sample = 0; sample < 24000; ++sample)
        loaded.process(1.7f * std::sin(static_cast<float>(sample) * .08f));
    CHECK(loaded.getSupplyVoltage() < .85);
    CHECK(loaded.getOperatingBias() < -.01);
    double difference = 0;
    for (int sample = 0; sample < 4800; ++sample)
    {
        const float input = .07f * std::sin(static_cast<float>(sample) * .1f);
        const auto actual = loaded.process(input), reference = fresh.process(input);
        REQUIRE(std::isfinite(actual)); REQUIRE(std::isfinite(reference));
        if (sample > 2400) difference += std::abs(actual - reference);
    }
    CHECK(difference > .1);
    for (int sample = 0; sample < 96000; ++sample) loaded.process(0);
    CHECK(loaded.getSupplyVoltage() > .999);
    CHECK(std::abs(loaded.getOperatingBias()) < .0001);
}
