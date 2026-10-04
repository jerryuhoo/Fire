#include <DSP/AnalogMagnetics.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

TEST_CASE("Magnetic material has coercivity remanence and bounded passive hysteresis loops",
          "[analog-physical][magnetics][dsp]")
{
    fire::analog::JilesAtherton core;
    core.configure({.3, .2, .17, .004});
    for (int sample = 0; sample <= 1024; ++sample) core.process(3.0 * sample / 1024);
    CHECK(core.getState().magnetisation > .85);
    for (int sample = 0; sample <= 1024; ++sample) core.process(3.0 * (1 - static_cast<double>(sample) / 1024));
    const auto remanence = core.getState().magnetisation;
    CHECK(remanence > .05);
    for (int sample = 0; sample < 1000; ++sample) CHECK(core.process(0) == remanence);
    for (int sample = 0; sample < 4096; ++sample) core.process(2 * std::sin(2 * 3.141592653589793 * sample / 1024));
    double work = 0;
    auto previous = core.getState();
    for (int sample = 0; sample < 1024; ++sample)
    {
        const auto h = 2 * std::sin(2 * 3.141592653589793 * sample / 1024);
        const auto m = core.process(h);
        REQUIRE(std::isfinite(m)); CHECK(std::abs(m) <= 1);
        work += .5 * (h + previous.field) * (m - previous.magnetisation);
        previous = core.getState();
    }
    CHECK(work > .1);
    core.reset(); CHECK(core.process(0) == 0);
}

TEST_CASE("The Langevin evaluation and derivative remain regular at zero and saturation",
          "[analog-physical][magnetics][numerics]")
{
    CHECK(fire::analog::JilesAtherton::langevin(0).value == 0);
    CHECK(fire::analog::JilesAtherton::langevin(0).derivative == Catch::Approx(1.0 / 3));
    for (double x : {1e-12, 1e-6, .049, .05, .5, 2.0, 9.0, 20.0, 100.0})
    {
        CAPTURE(x);
        const auto positive = fire::analog::JilesAtherton::langevin(x), negative = fire::analog::JilesAtherton::langevin(-x);
        REQUIRE(std::isfinite(positive.value)); REQUIRE(std::isfinite(positive.derivative));
        CHECK(positive.value == -negative.value); CHECK(positive.derivative == negative.derivative);
        CHECK(positive.derivative >= 0); CHECK(positive.value < 1);
        if (x >= .05) CHECK(positive.value == Catch::Approx(1 / std::tanh(x) - 1 / x).margin(1e-7));
    }
    for (int sample = 500; sample <= 90000; ++sample)
    {
        const double x = sample * .0001;
        const auto value = fire::analog::JilesAtherton::langevin(x);
        const auto coth = 1 / std::tanh(x);
        REQUIRE(std::abs(value.value - (coth - 1 / x)) < 1e-8);
        REQUIRE(std::abs(value.derivative - (1 / (x * x) - coth * coth + 1)) < 2e-8);
    }
}

TEST_CASE("Transformer winding voltage flux and magnetising current satisfy the circuit equation",
          "[analog-physical][transformer][dsp][circuit]")
{
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
    {
        CAPTURE(rate);
        fire::analog::Transformer transformer;
        transformer.prepare(rate);
        double largestResidual = 0;
        for (int sample = 0; sample < static_cast<int>(rate * .15); ++sample)
        {
            const auto signal = .8 * std::sin(2 * 3.141592653589793 * 45 * sample / rate)
                                + .08 * std::sin(2 * 3.141592653589793 * 3101 * sample / rate);
            const auto oldFlux = transformer.getFlux();
            const auto voltage = transformer.process(signal);
            REQUIRE(std::isfinite(voltage)); REQUIRE(std::abs(transformer.getMagnetisation()) <= 1);
            const auto fluxVoltage = (transformer.getFlux() - oldFlux) * rate / (2 * 3.141592653589793 * 18);
            largestResidual = std::max(largestResidual, std::abs(voltage - fluxVoltage));
        }
        CHECK(largestResidual < 1e-5);
        for (int sample = 0; sample < static_cast<int>(rate); ++sample) transformer.process(0);
        CHECK(std::abs(transformer.process(0)) < .001);
    }
}

TEST_CASE("Value-only magnetic integration retains the full material trajectory",
          "[analog-physical][magnetics][numerics][realtime]")
{
    fire::analog::JilesAtherton reference, fast;
    reference.configure({.18, .22, .17, .004}); fast.configure({.18, .22, .17, .004});
    double largestError = 0;
    for (int sample = 0; sample < 32000; ++sample)
    {
        const auto field = 1.25 * std::sin(sample * .45) + .8 * std::sin(sample * .006);
        largestError = std::max(largestError, std::abs(reference.process(field) - fast.processMagnetisation(field)));
    }
    CHECK(largestError < 1e-8);
}
