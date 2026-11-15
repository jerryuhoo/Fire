#include <GUI/InsertOttSpectrum.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <array>
#include <limits>

namespace
{
using Spectrum = fire::ui::InsertOttSpectrum;
using Magnitudes = std::array<float, 1024>;

float peak(const Spectrum::Frame& frame)
{
    return *std::max_element(frame.profile.begin(), frame.profile.end());
}

Spectrum::Frame settle(Spectrum& spectrum, const Magnitudes& magnitudes,
                      const Magnitudes* reference = nullptr,
                      float lowHz = 20.0f, float highHz = 20000.0f)
{
    Spectrum::Frame frame;
    for (int i = 0; i < 60; ++i)
    {
        spectrum.update(magnitudes.data(), static_cast<int>(magnitudes.size()),
            48000.0f / 2048.0f, reference != nullptr ? reference->data() : nullptr,
            lowHz, highHz);
        frame = spectrum.advance(1.0f / 60.0f);
    }
    return frame;
}
}

TEST_CASE("Inserted OTT spectrum starts silent and preserves a narrow FFT peak",
          "[ott][insert-ott-spectrum]")
{
    Spectrum spectrum;
    CHECK(peak(spectrum.advance(1.0f / 60.0f)) == 0.0f);
    Magnitudes magnitudes {};
    magnitudes[137] = 1024.0f;
    const auto frame = settle(spectrum, magnitudes);
    CHECK(peak(frame) == Catch::Approx(0.5f).margin(0.001f));
    CHECK(std::count_if(frame.profile.begin(), frame.profile.end(),
                       [](float value) { return value > 0.001f; }) == 3);
    const auto expected = std::log((137.0f * 48000.0f / 2048.0f) / 20.0f)
        / std::log(1000.0f);
    CHECK(frame.centroid == Catch::Approx(expected).margin(1.0f / 23.0f));
}

TEST_CASE("Inserted OTT spectrum restricts band colour to its frequency range",
          "[ott][insert-ott-spectrum]")
{
    Spectrum spectrum;
    Magnitudes magnitudes {};
    magnitudes[137] = 1024.0f;
    CHECK(peak(settle(spectrum, magnitudes, nullptr, 20.0f, 1000.0f)) == 0.0f);
    const auto frame = settle(spectrum, magnitudes, nullptr, 1000.0f, 5000.0f);
    CHECK(peak(frame) > 0.49f);
    CHECK(frame.centroid > std::log(1000.0f / 20.0f) / std::log(1000.0f));
    CHECK(frame.centroid < std::log(5000.0f / 20.0f) / std::log(1000.0f));
}

TEST_CASE("Inserted OTT spectrum safely combines finite reference peaks",
          "[ott][insert-ott-spectrum]")
{
    Spectrum spectrum;
    Magnitudes magnitudes {}, reference {};
    magnitudes.fill(std::numeric_limits<float>::quiet_NaN());
    reference[30] = 1024.0f;
    reference[40] = std::numeric_limits<float>::infinity();
    reference[50] = -std::numeric_limits<float>::infinity();
    const auto frame = settle(spectrum, magnitudes, &reference);
    CHECK(peak(frame) > 0.49f);
    CHECK(std::isfinite(frame.centroid));
    for (const auto value : frame.profile)
    {
        CHECK(std::isfinite(value));
        CHECK(value >= 0.0f);
        CHECK(value <= 1.0f);
    }
}

TEST_CASE("Inserted OTT spectrum expires without fresh FFT frames",
          "[ott][insert-ott-spectrum]")
{
    Spectrum spectrum;
    Magnitudes magnitudes {};
    magnitudes[137] = 1024.0f;
    const auto active = settle(spectrum, magnitudes);
    CHECK(peak(active) > 0.49f);
    Spectrum::Frame frame;
    for (int i = 0; i < 12; ++i)
        frame = spectrum.advance(1.0f / 60.0f);
    CHECK(peak(frame) > 0.49f);
    for (int i = 0; i < 12; ++i)
        frame = spectrum.advance(1.0f / 60.0f);
    CHECK(peak(frame) < peak(active) * 0.5f);
    for (int i = 0; i < 120; ++i)
        frame = spectrum.advance(1.0f / 60.0f);
    CHECK(peak(frame) == 0.0f);
}

TEST_CASE("Inserted OTT spectrum invalid input and identity reset clear old profiles",
          "[ott][insert-ott-spectrum]")
{
    Spectrum spectrum;
    Magnitudes magnitudes {};
    magnitudes[137] = std::numeric_limits<float>::max();
    auto frame = settle(spectrum, magnitudes);
    CHECK(peak(frame) > 0.49f);
    spectrum.reset();
    frame = spectrum.advance(std::numeric_limits<float>::quiet_NaN());
    CHECK(peak(frame) == 0.0f);
    CHECK(frame.centroid == Catch::Approx(0.5f));

    for (const auto binWidth : { 0.0f, -1.0f,
                                 std::numeric_limits<float>::infinity() })
    {
        settle(spectrum, magnitudes);
        spectrum.update(magnitudes.data(), static_cast<int>(magnitudes.size()), binWidth);
        CHECK(peak(spectrum.advance(0.05f)) == 0.0f);
    }
    settle(spectrum, magnitudes);
    spectrum.update(magnitudes.data(), 1024, 23.4375f, nullptr, 5000.0f, 1000.0f);
    CHECK(peak(spectrum.advance(0.05f)) == 0.0f);
    spectrum.update(nullptr, 1024, 23.4375f);
    CHECK(peak(spectrum.advance(0.05f)) == 0.0f);
}
