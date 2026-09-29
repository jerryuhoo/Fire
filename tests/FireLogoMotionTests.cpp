#include <GUI/FireLogoMotion.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>

namespace
{
constexpr float frameSeconds = 1.0f / 60.0f;

float magnitude(float decibels)
{
    return std::pow(10.0f, decibels / 20.0f);
}

void feed(fire::ui::FireLogoMotion& motion, int frames, float rms, float peak)
{
    for (int frame = 0; frame < frames; ++frame)
        motion.advance(frameSeconds, true, rms, peak);
}

void checkFiniteState(const fire::ui::FireLogoMotion& motion)
{
    CHECK(std::isfinite(motion.energy()));
    CHECK(std::isfinite(motion.attack()));
    CHECK(std::isfinite(motion.phase()));
    CHECK(motion.energy() >= 0.0f);
    CHECK(motion.energy() <= 1.0f);
    CHECK(motion.attack() >= 0.0f);
    CHECK(motion.attack() <= 1.0f);
    CHECK(motion.phase() >= 0.0f);
    CHECK(motion.phase() < 6.283186f);
}
} // namespace

TEST_CASE("Fire logo rests exactly at silence and rejects the noise floor",
          "[fire-logo-motion][ui]")
{
    fire::ui::FireLogoMotion motion;
    for (float level : {0.0f, magnitude(-80.0f), 0.001f})
    {
        CAPTURE(level);
        for (int frame = 0; frame < 60; ++frame)
            CHECK_FALSE(motion.advance(frameSeconds, true, level, level));
        CHECK(motion.energy() == 0.0f);
        CHECK(motion.attack() == 0.0f);
        CHECK(motion.phase() == 0.0f);
        CHECK_FALSE(motion.isAnimating());
    }
}

TEST_CASE("Fire logo sustains distinct intensity and speed at different audio levels",
          "[fire-logo-motion][ui]")
{
    std::array<float, 3> levels {-48.0f, -30.0f, -12.0f};
    std::array<float, 3> energies {};
    std::array<float, 3> phaseSteps {};
    for (size_t index = 0; index < levels.size(); ++index)
    {
        fire::ui::FireLogoMotion motion;
        const auto level = magnitude(levels[index]);
        feed(motion, 120, level, level);
        energies[index] = motion.energy();
        CHECK(motion.attack() == 0.0f);
        const auto before = motion.phase();
        CHECK(motion.advance(frameSeconds, true, level, level));
        phaseSteps[index] = motion.phase() - before;
        if (phaseSteps[index] < 0.0f)
            phaseSteps[index] += 6.2831853f;
        CHECK(motion.isAnimating());
    }
    CHECK(energies[0] > 0.1f);
    CHECK(energies[1] > energies[0] + 0.2f);
    CHECK(energies[2] > energies[1] + 0.3f);
    CHECK(phaseSteps[1] > phaseSteps[0]);
    CHECK(phaseSteps[2] > phaseSteps[1]);
}

TEST_CASE("Fire logo responds promptly and leaves a short peak attack tail",
          "[fire-logo-motion][ui]")
{
    fire::ui::FireLogoMotion motion;
    CHECK(motion.advance(frameSeconds, true, 0.5f, 1.0f));
    CHECK(motion.energy() > 0.3f);
    CHECK(motion.attack() > 0.7f);
    feed(motion, 30, 0.5f, 1.0f);
    CHECK(motion.energy() > 0.95f);
    CHECK(motion.attack() == 0.0f);

    // A new peak produces a flash even when the RMS level is unchanged.
    feed(motion, 60, 0.01f, 0.01f);
    const auto quietEnergy = motion.energy();
    CHECK(motion.advance(frameSeconds, true, 0.01f, 1.0f));
    CHECK(motion.attack() > 0.6f);
    CHECK(motion.energy() < quietEnergy + 0.1f);
    feed(motion, 30, 0.01f, 1.0f);
    CHECK(motion.attack() == 0.0f);
    CHECK(motion.isAnimating());
}

TEST_CASE("Fire logo fades to an exact resting state after real silence",
          "[fire-logo-motion][ui]")
{
    fire::ui::FireLogoMotion motion;
    feed(motion, 60, 1.0f, 1.0f);
    auto secondsToRest = 0.0f;
    for (int frame = 0; frame < 60 && motion.isAnimating(); ++frame)
    {
        const auto previousEnergy = motion.energy();
        CHECK(motion.advance(frameSeconds, true, 0.0f, 0.0f));
        CHECK(motion.energy() < previousEnergy);
        secondsToRest += frameSeconds;
    }
    CHECK(secondsToRest > 0.3f);
    CHECK(secondsToRest < 0.6f);
    CHECK(motion.energy() == 0.0f);
    CHECK(motion.attack() == 0.0f);
    CHECK_FALSE(motion.isAnimating());
    const auto restingPhase = motion.phase();
    for (int frame = 0; frame < 120; ++frame)
        CHECK_FALSE(motion.advance(frameSeconds, true, 0.0f, 0.0f));
    CHECK(motion.phase() == restingPhase);
}

TEST_CASE("Fire logo expires held meter data when audio callbacks stop",
          "[fire-logo-motion][ui]")
{
    fire::ui::FireLogoMotion motion;
    feed(motion, 60, 1.0f, 1.0f);
    const auto runningEnergy = motion.energy();
    for (int frame = 0; frame < 9; ++frame)
        motion.advance(frameSeconds, false, 1.0f, 1.0f);
    CHECK(motion.energy() == Catch::Approx(runningEnergy).margin(0.00001f));
    for (int frame = 0; frame < 9; ++frame)
        motion.advance(frameSeconds, false, 1.0f, 1.0f);
    CHECK(motion.energy() < 0.4f);
    for (int frame = 0; frame < 30; ++frame)
        motion.advance(frameSeconds, false, 1.0f, 1.0f);
    CHECK_FALSE(motion.isAnimating());
    CHECK(motion.energy() == 0.0f);
    CHECK(motion.attack() == 0.0f);
    const auto restingPhase = motion.phase();
    CHECK_FALSE(motion.advance(frameSeconds, false, 1.0f, 1.0f));
    CHECK(motion.phase() == restingPhase);
    CHECK(motion.advance(frameSeconds, true, 1.0f, 1.0f));
    CHECK(motion.attack() > 0.7f);
}

TEST_CASE("Fire logo reset cannot replay old meter data",
          "[fire-logo-motion][ui]")
{
    fire::ui::FireLogoMotion motion;
    feed(motion, 10, 0.5f, 0.8f);
    REQUIRE(motion.isAnimating());
    motion.reset();
    CHECK(motion.energy() == 0.0f);
    CHECK(motion.attack() == 0.0f);
    CHECK(motion.phase() == 0.0f);
    CHECK_FALSE(motion.isAnimating());
    CHECK_FALSE(motion.advance(frameSeconds, false, 0.5f, 0.8f));
    CHECK(motion.advance(frameSeconds, true, 0.5f, 0.8f));
    CHECK(motion.attack() > 0.7f);
}

TEST_CASE("Fire logo safely handles malformed meter values and UI intervals",
          "[fire-logo-motion][ui]")
{
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    const auto infinity = std::numeric_limits<float>::infinity();
    for (float invalid : {nan, infinity, -infinity, -1.0f})
    {
        CAPTURE(invalid);
        fire::ui::FireLogoMotion motion;
        CHECK_FALSE(motion.advance(frameSeconds, true, invalid, invalid));
        checkFiniteState(motion);
        feed(motion, 60, 1.0f, 1.0f);
        feed(motion, 60, invalid, invalid);
        CHECK_FALSE(motion.isAnimating());
        checkFiniteState(motion);
    }

    for (float invalidTime : {nan, infinity, -infinity, -1.0f, 0.0f})
    {
        CAPTURE(invalidTime);
        fire::ui::FireLogoMotion motion;
        feed(motion, 60, 0.5f, 0.8f);
        const auto oldEnergy = motion.energy();
        const auto oldAttack = motion.attack();
        const auto oldPhase = motion.phase();
        CHECK_FALSE(motion.advance(invalidTime, true, 0.0f, 0.0f));
        CHECK(motion.energy() == oldEnergy);
        CHECK(motion.attack() == oldAttack);
        CHECK(motion.phase() == oldPhase);
        checkFiniteState(motion);
    }

    fire::ui::FireLogoMotion motion;
    feed(motion, 60, std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    CHECK(motion.energy() == Catch::Approx(1.0f).margin(0.00001f));
    checkFiniteState(motion);
    CHECK(motion.advance(std::numeric_limits<float>::max(), false, 1.0f, 1.0f));
    CHECK_FALSE(motion.isAnimating());
    checkFiniteState(motion);
    CHECK(motion.advance(std::numeric_limits<float>::max(), true, 0.5f, 0.8f));
    checkFiniteState(motion);
}

TEST_CASE("Fire logo motion is consistent across UI refresh rates",
          "[fire-logo-motion][ui]")
{
    fire::ui::FireLogoMotion slow;
    fire::ui::FireLogoMotion fast;
    struct Input { bool fresh; float rms; float peak; };
    const std::array<Input, 6> segments {{
        {true, 0.02f, 0.04f},
        {true, 0.35f, 0.9f},
        {true, 0.07f, 0.2f},
        {false, 0.07f, 0.2f},
        {false, 0.07f, 0.2f},
        {true, 0.0f, 0.0f}
    }};
    for (const auto& input : segments)
    {
        for (int frame = 0; frame < 12; ++frame)
            slow.advance(1.0f / 30.0f, input.fresh, input.rms, input.peak);
        for (int frame = 0; frame < 48; ++frame)
            fast.advance(1.0f / 120.0f, input.fresh, input.rms, input.peak);
        CHECK(slow.energy() == Catch::Approx(fast.energy()).margin(0.0001f));
        CHECK(slow.attack() == Catch::Approx(fast.attack()).margin(0.0001f));
        CHECK(slow.phase() == Catch::Approx(fast.phase()).margin(0.003f));
        CHECK(slow.isAnimating() == fast.isAnimating());
    }
    CHECK_FALSE(slow.isAnimating());
    CHECK_FALSE(fast.isAnimating());
}
