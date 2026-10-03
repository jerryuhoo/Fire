#pragma once

#include <algorithm>
#include <cmath>

namespace fire::analog
{
// Jiles-Atherton material law: M = c Man + (1-c) Mirr and
// He = H + alpha M. Mirr follows the anhysteretic magnetisation only when
// domain motion is admissible. The pinning equation is integrated in He,
// using its bounded exponential solution and a weighted field midpoint.
// This avoids differentiating a sampled H signal near Nyquist. The material
// parameters are normalised, not fitted to a particular commercial device.
// Reference: J. Chowdhury, DAFx-19, "Real-time Physical Modelling for Analog
// Tape Machines", section 2.2. https://www.dafx.de/paper-archive/2019/DAFx2019_paper_3.pdf
class JilesAtherton
{
public:
    struct Material { double a = .3, k = .2, c = .17, alpha = .004; };
    struct State { double field = 0, magnetisation = 0, irreversible = 0, slope = 0; };
    struct Langevin { double value, derivative; };

    static Langevin langevin(double x) noexcept
    {
        const auto magnitude = std::abs(x);
        if (magnitude < .05)
        {
            const auto square = x * x;
            return {x * (1.0 / 3 - square / 45 + 2 * square * square / 945 - square * square * square / 4725),
                    1.0 / 3 - square / 15 + 2 * square * square / 189 - square * square * square / 675};
        }
        if (magnitude > 9)
            return {std::copysign(1.0, x) - 1.0 / x, 1.0 / (x * x)};
        const auto coth = 1.0 / std::tanh(x);
        return {coth - 1.0 / x, 1.0 / (x * x) - (coth * coth - 1)};
    }

    void configure(Material material) noexcept
    {
        parameters.a = std::clamp(material.a, .02, 2.0);
        parameters.k = std::clamp(material.k, .01, 2.0);
        parameters.c = std::clamp(material.c, 0.0, 1.0);
        parameters.alpha = std::clamp(material.alpha, 0.0, .05 * std::min(parameters.a, parameters.k));
    }
    void reset() noexcept { state = {}; }
    const State& getState() const noexcept { return state; }
    void commit(const State& next) noexcept { state = next; }

    State predict(double targetField) const noexcept
    {
        targetField = std::isfinite(targetField) ? std::clamp(targetField, -64.0, 64.0) : 0.0;
        if (targetField == state.field) return state;
        // Split zero crossings to resolve changes in the anhysteretic curve.
        if (state.field * targetField < 0)
            return advance(advance(state, 0), targetField);
        return advance(state, targetField);
    }
    double process(double field) noexcept { state = predict(field); return state.magnetisation; }

private:
    State advance(const State& previous, double field) const noexcept
    {
        const auto oldEffective = previous.field + parameters.alpha * previous.magnetisation;
        double magnetisation = previous.magnetisation, irreversible = previous.irreversible, slope = 0;
        for (int iteration = 0; iteration < 3; ++iteration)
        {
            const auto effective = field + parameters.alpha * magnetisation;
            const auto delta = effective - oldEffective;
            const auto distance = std::abs(delta);
            const auto u = distance / parameters.k;
            const auto gain = -std::expm1(-u);
            const auto decay = 1 - gain;
            double meanDistance = distance * .5, meanDerivative = .5;
            if (u > 1e-3)
            {
                meanDistance = parameters.k - distance * decay / gain;
                meanDerivative = 1 + decay / gain - u * decay / (gain * gain);
            }
            else
            {
                meanDistance = parameters.k * (u * .5 - u * u / 12 + u * u * u * u / 720);
                meanDerivative = .5 + u / 6 - u * u * u / 180;
            }
            const auto direction = delta >= 0 ? 1.0 : -1.0;
            const auto mid = langevin((effective - direction * meanDistance) / parameters.a);
            const auto end = langevin(effective / parameters.a);
            const auto difference = mid.value - previous.irreversible;
            const bool moving = direction * difference > 0;
            irreversible = moving ? previous.irreversible + gain * difference : previous.irreversible;
            magnetisation = (1 - parameters.c) * irreversible + parameters.c * end.value;
            const auto irreversibleSlope = moving ? gain * mid.derivative / parameters.a * meanDerivative
                                                      + direction * decay / parameters.k * difference : 0.0;
            const auto effectiveSlope = (1 - parameters.c) * irreversibleSlope + parameters.c * end.derivative / parameters.a;
            slope = effectiveSlope / std::max(.5, 1 - parameters.alpha * effectiveSlope);
        }
        return {field, std::clamp(magnetisation, -1.0, 1.0),
                std::clamp(irreversible, -1.0, 1.0), std::max(0.0, slope)};
    }

    Material parameters;
    State state;
};

// Normalised voltage-driven transformer. The winding equation is
// dB/dt = fluxRate * (Vin - Rw H), B = linearFlux H + M(H).
// The magnetisation/current load is solved together with flux using an
// implicit midpoint winding update. Saturation therefore depends on voltage,
// frequency and previous magnetisation, not a low-pass output feedback term.
class Transformer
{
public:
    void prepare(double rate) noexcept
    {
        const auto safeRate = std::isfinite(rate) && rate > 0 ? rate : 48000.0;
        step = 2 * 3.14159265358979323846 * 18 / safeRate;
        core.configure({.22, .10, .14, .002});
    }
    void reset() noexcept { core.reset(); residual = 0; }
    double process(double input) noexcept
    {
        input = std::isfinite(input) ? std::clamp(input, -8.0, 8.0) : 0.0;
        const auto old = core.getState();
        const auto oldFlux = linearFlux * old.field + old.magnetisation;
        const auto rhs = oldFlux + step * (input - windingResistance * old.field * .5);
        auto field = std::clamp(old.field + step * (input - windingResistance * old.field)
                                              / (linearFlux + std::max(.02, old.slope) + step * windingResistance * .5),
                                -64.0, 64.0);
        auto candidate = core.predict(field);
        for (int iteration = 0; iteration < 6; ++iteration)
        {
            candidate = core.predict(field);
            residual = linearFlux * field + candidate.magnetisation + step * windingResistance * field * .5 - rhs;
            if (std::abs(residual) < 1e-11) break;
            const auto derivative = linearFlux + candidate.slope + step * windingResistance * .5;
            field = std::clamp(field - std::clamp(residual / derivative, -2.0, 2.0), -64.0, 64.0);
        }
        candidate = core.predict(field);
        residual = linearFlux * field + candidate.magnetisation + step * windingResistance * field * .5 - rhs;
        core.commit(candidate);
        return input - windingResistance * (field + old.field) * .5;
    }
    double getFlux() const noexcept { return linearFlux * core.getState().field + core.getState().magnetisation; }
    double getMagnetisation() const noexcept { return core.getState().magnetisation; }
    double getWindingResidual() const noexcept { return residual; }

private:
    static constexpr double linearFlux = .08, windingResistance = .18;
    JilesAtherton core;
    double step = 0, residual = 0;
};
}
