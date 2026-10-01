#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <cmath>

namespace fire::space
{
inline constexpr int count = 6;
inline constexpr std::array<const char*, count> names {"Classic", "Room", "Hall", "Plate", "Spring", "Chamber"};

class Reverb
{
    class Delay
    {
    public:
        void prepare(size_t samples) {data.assign(samples + 4, 0); reset();}
        void reset() noexcept {head = filled = 0;}
        float read(float age) const noexcept
        {
            age = juce::jlimit(1.0f, static_cast<float>(data.size() - 2), age);
            const auto whole = static_cast<size_t>(age);
            if (whole > filled) return 0;
            const auto i = head >= whole ? head - whole : data.size() - (whole - head);
            const auto j = i > 0 ? i - 1 : data.size() - 1;
            const auto next = whole < filled ? data[j] : 0;
            return juce::jmap(age - static_cast<float>(whole), data[i], next);
        }
        void write(float value) noexcept
        {data[head] = std::isfinite(value) ? value : 0; if (++head == data.size()) head = 0; filled = juce::jmin(filled + 1, data.size());}
    private:
        std::vector<float> data;
        size_t head = 0, filled = 0;
    };
public:
    void prepare(double rate)
    {
        sampleRate = std::isfinite(rate) && rate > 0 ? rate : 48000;
        for (auto& delay : tank) delay.prepare(static_cast<size_t>(std::ceil(sampleRate * .24)));
        for (auto& channel : diffusers) for (auto& delay : channel) delay.prepare(static_cast<size_t>(std::ceil(sampleRate * .021)));
        for (auto& delay : early) delay.prepare(static_cast<size_t>(std::ceil(sampleRate * .12)));
        period = juce::jmax(1, juce::roundToInt(sampleRate / 1500));
        reset();
    }
    void reset() noexcept
    {
        for (auto& delay : tank) delay.reset();
        for (auto& channel : diffusers) for (auto& delay : channel) delay.reset();
        for (auto& delay : early) delay.reset();
        for (auto& line : dispersion) line.fill(0);
        damp.fill(0); phase = 0; counter = 0; configured = false;
    }
    void process(float& left, float& right, int model, float size, float damping, float width) noexcept
    {
        model = juce::jlimit(1, count - 1, model);
        size = juce::jlimit(0.0f, 100.0f, std::isfinite(size) ? size : 50);
        damping = juce::jlimit(0.0f, 100.0f, std::isfinite(damping) ? damping : 40);
        width = juce::jlimit(0.0f, 1.0f, std::isfinite(width) ? width * .01f : 1);
        if (!configured || counter-- <= 0) {configure(model, size, damping); counter = period;}
        float input[2] {juce::jlimit(-16.0f, 16.0f, std::isfinite(left) ? left : 0), juce::jlimit(-16.0f, 16.0f, std::isfinite(right) ? right : 0)};
        for (int channel = 0; channel < 2; ++channel)
        {
            early[static_cast<size_t>(channel)].write(input[channel]);
            for (size_t stage = 0; stage < 4; ++stage)
            {
                auto& delay = diffusers[static_cast<size_t>(channel)][stage];
                const auto milliseconds = (model == 3 ? 3.7f : 2.3f) + static_cast<float>(stage) * 2.1f + static_cast<float>(channel) * .17f;
                const auto old = delay.read(static_cast<float>(sampleRate) * milliseconds * .001f);
                const auto value = input[channel] + old * .62f;
                delay.write(value); input[channel] = old - value * .62f;
            }
        }
        phase += 1.0 / sampleRate;
        if (phase >= 4096) phase -= 4096;
        std::array<float, 8> values;
        float mean = 0;
        for (size_t line = 0; line < values.size(); ++line)
        {
            const auto motion = model == 2 || model == 3 ? std::sin(phase * (0.11 + line * .027) * juce::MathConstants<double>::twoPi) * sampleRate * .00012 : 0;
            auto value = tank[line].read(delays[line] + static_cast<float>(motion));
            damp[line] += dampingPole * (value - damp[line]); value = damp[line];
            if (model == 4)
                for (size_t stage = 0; stage < dispersion[line].size(); ++stage)
                {
                    const float a = .52f + .025f * static_cast<float>(stage) + .011f * static_cast<float>(line);
                    const auto output = dispersion[line][stage] - a * value;
                    dispersion[line][stage] = value + a * output;
                    value = output;
                }
            values[line] = value; mean += value * .125f;
        }
        float wetLeft = 0, wetRight = 0;
        for (size_t line = 0; line < values.size(); ++line)
        {
            wetLeft += values[line] * (line % 2 == 0 ? .25f : -.17f);
            wetRight += values[line] * (line % 3 == 0 ? -.20f : .21f);
        }
        if (model == 3)
        {
            // An orthogonal Hadamard scatter produces fast plate density;
            // the room/hall/chamber use a Householder reflection instead.
            for (size_t stride = 1; stride < values.size(); stride *= 2)
                for (size_t start = 0; start < values.size(); start += stride * 2)
                    for (size_t offset = 0; offset < stride; ++offset)
                    {
                        const auto a = values[start + offset], b = values[start + offset + stride];
                        values[start + offset] = a + b; values[start + offset + stride] = a - b;
                    }
            for (auto& value : values) value *= .35355339f;
        }
        else for (auto& value : values) value -= mean * 2;
        for (size_t line = 0; line < values.size(); ++line)
            tank[line].write(input[line % 2] * (line % 3 == 0 ? -.22f : .22f) + feedback[line] * values[line]);
        const float earlyWeight = model == 1 ? .35f : model == 5 ? .22f : model == 4 ? .16f : .10f;
        const float earlyTime = model == 1 ? .012f : model == 4 ? .030f : .023f;
        wetLeft += earlyWeight * (early[0].read(static_cast<float>(sampleRate) * earlyTime) + .4f * early[1].read(static_cast<float>(sampleRate) * earlyTime * 1.73f));
        wetRight += earlyWeight * (early[1].read(static_cast<float>(sampleRate) * earlyTime * 1.17f) + .4f * early[0].read(static_cast<float>(sampleRate) * earlyTime * 1.91f));
        const auto middle = (wetLeft + wetRight) * .5f, side = (wetLeft - wetRight) * .5f * width;
        left = std::isfinite(middle + side) ? middle + side : 0;
        right = std::isfinite(middle - side) ? middle - side : 0;
    }
private:
    void configure(int model, float size, float damping) noexcept
    {
        constexpr std::array<std::array<float, 8>, 5> times{{
            {{.0199f,.0233f,.0311f,.0367f,.0419f,.0479f,.0533f,.0599f}},
            {{.0331f,.0413f,.0537f,.0673f,.0791f,.0893f,.1013f,.1139f}},
            {{.0127f,.0163f,.0193f,.0237f,.0293f,.0337f,.0379f,.0431f}},
            {{.0271f,.0337f,.0383f,.0419f,.0473f,.0517f,.0593f,.0671f}},
            {{.0173f,.0281f,.0347f,.0451f,.0547f,.0637f,.0797f,.0913f}}
        }};
        const auto scale = .55f + size * .012f;
        const auto decay = model == 1 ? .25f + size * .032f : model == 4 ? .6f + size * .039f : .65f + size * .085f;
        for (size_t line = 0; line < delays.size(); ++line)
        {
            const auto seconds = times[static_cast<size_t>(model - 1)][line] * scale;
            delays[line] = seconds * static_cast<float>(sampleRate);
            feedback[line] = juce::jlimit(0.0f, .994f, std::exp(-6.907755f * seconds / decay));
        }
        const auto frequency = 1600.0f + 16500.0f * std::pow(1 - damping * .01f, 2.0f);
        dampingPole = static_cast<float>(1 - std::exp(-juce::MathConstants<double>::twoPi * juce::jmin(static_cast<double>(frequency), sampleRate * .44) / sampleRate));
        configured = true;
    }
    double sampleRate = 48000, phase = 0;
    std::array<Delay, 8> tank;
    std::array<std::array<Delay, 4>, 2> diffusers;
    std::array<Delay, 2> early;
    std::array<float, 8> damp {}, delays {}, feedback {};
    std::array<std::array<float, 10>, 8> dispersion {};
    float dampingPole = .5f;
    int counter = 0, period = 32;
    bool configured = false;
};
}
