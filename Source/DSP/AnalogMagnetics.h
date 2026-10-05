#pragma once

#include <algorithm>
#include <array>
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

    static Langevin langevin(double x) noexcept { return evaluateLangevin<true>(x); }

    template <bool needDerivative>
    static Langevin evaluateLangevin(double x) noexcept
    {
        const auto magnitude = std::abs(x);
        if (magnitude < .05)
        {
            const auto square = x * x;
            return {x * (1.0 / 3 - square / 45 + 2 * square * square / 945 - square * square * square / 4725),
                    needDerivative ? 1.0 / 3 - square / 15 + 2 * square * square / 189 - square * square * square / 675 : 0};
        }
        if (magnitude > 9)
            return {std::copysign(1.0, x) - 1.0 / x, needDerivative ? 1.0 / (x * x) : 0};
        // The winding Jacobian uses the derivative of the same interpolant as
        // M, keeping Newton's voltage/flux solve coherent. The immutable table
        // is shared by the value-only and derivative paths, with no RT setup.
        const auto& table = langevinTable();
        constexpr double spacing = 9.0 / 512;
        const auto position = magnitude * (512.0 / 9);
        const auto index = std::min(511, static_cast<int>(position));
        const auto t = position - index;
        const auto& cell = table[static_cast<size_t>(index)];
        const auto value = cell.value + t * (cell.first + t * (cell.second + t * cell.third));
        const auto derivative = needDerivative ? (cell.first + t * (2 * cell.second + 3 * t * cell.third)) / spacing : 0;
        return {std::copysign(value, x), derivative};
    }

    void configure(Material material) noexcept
    {
        parameters.a = std::clamp(material.a, .02, 2.0);
        parameters.k = std::clamp(material.k, .01, 2.0);
        parameters.c = std::clamp(material.c, 0.0, 1.0);
        parameters.alpha = std::clamp(material.alpha, 0.0, .05 * std::min(parameters.a, parameters.k));
        inverseA = 1 / parameters.a;
        inverseK = 1 / parameters.k;
    }
    void reset() noexcept { state = {}; }
    const State& getState() const noexcept { return state; }
    void commit(const State& next) noexcept { state = next; }

    State predict(double targetField) const noexcept { return predictImpl<true>(targetField); }
    template <bool needDerivative>
    State predictImpl(double targetField) const noexcept
    {
        targetField = std::isfinite(targetField) ? std::clamp(targetField, -64.0, 64.0) : 0.0;
        if (targetField == state.field) return state;
        // Split zero crossings to resolve changes in the anhysteretic curve.
        if (state.field * targetField < 0)
            return advance<needDerivative>(advance<needDerivative>(state, 0), targetField);
        return advance<needDerivative>(state, targetField);
    }
    double process(double field) noexcept { state = predict(field); return state.magnetisation; }
    // Same magnetisation trajectory, without the winding solver's Jacobian.
    double processMagnetisation(double field) noexcept { state = predictImpl<false>(field); return state.magnetisation; }

private:
    struct LangevinCell { double value, first, second, third; };

    static const std::array<LangevinCell, 512>& langevinTable() noexcept
    {
        // Thirteen-term Langevin continued fraction, evaluated at compile time
        // as positive-coefficient rational polynomials. Hermite interpolation
        // preserves its value and slope accuracy without per-sample division
        // through the full polynomial or cancellation in coth(x)-1/x.
        static constexpr auto table = []
        {
            constexpr std::array p{.33333333333333331, .027160493827160494, .00063492063492063492,
                                   5.7255322955806051e-6, 2.0654878411185444e-8, 2.5086896855690823e-11, 4.6847613175893221e-15};
            constexpr std::array q{1.0, .14814814814814814, .0054320987654320986,
                                   7.3613986657464919e-5, 4.0896659254147175e-7, 8.6967909099728185e-10, 4.9189993834687888e-13};
            std::array<Langevin, 513> values{};
            for (size_t sample = 0; sample < values.size(); ++sample)
            {
                const auto field = static_cast<double>(sample) * 9 / 512;
                const auto squared = field * field;
                double n = p.back(), d = q.back(), dn = 0, dd = 0;
                for (int index = 5; index >= 0; --index)
                {
                    dn = dn * squared + n; dd = dd * squared + d;
                    n = n * squared + p[static_cast<size_t>(index)];
                    d = d * squared + q[static_cast<size_t>(index)];
                }
                const auto ratio = n / d;
                values[sample] = {field * ratio, ratio + 2 * squared * (dn - ratio * dd) / d};
            }
            std::array<LangevinCell, 512> cells{};
            for (size_t index = 0; index < cells.size(); ++index)
            {
                const auto d0 = values[index].derivative * (9.0 / 512);
                const auto d1 = values[index + 1].derivative * (9.0 / 512);
                const auto difference = values[index + 1].value - values[index].value;
                cells[index] = {values[index].value, d0, 3 * difference - 2 * d0 - d1,
                                -2 * difference + d0 + d1};
            }
            return cells;
        }();
        return table;
    }

    template <bool needDerivative>
    State advance(const State& previous, double field) const noexcept
    {
        const bool uncoupled = parameters.alpha == 0;
        Langevin uncoupledEnd{};
        if (uncoupled)
        {
            uncoupledEnd = evaluateLangevin<needDerivative>(field * inverseA);
            const auto direction = field >= previous.field ? 1.0 : -1.0;
            // The weighted midpoint lies between the old and new fields. If
            // even the new endpoint cannot depin the domains, neither can the
            // midpoint. Retain reversible magnetisation while skipping the
            // exponential and midpoint calculation for this pinned RF step.
            if (direction * (uncoupledEnd.value - previous.irreversible) <= 0)
                return {field, std::clamp((1 - parameters.c) * previous.irreversible
                                         + parameters.c * uncoupledEnd.value, -1.0, 1.0),
                        previous.irreversible,
                        needDerivative ? parameters.c * uncoupledEnd.derivative * inverseA : 0};
        }
        const auto oldEffective = previous.field + parameters.alpha * previous.magnetisation;
        const auto initialU = std::abs(field - previous.field) * inverseK;
        double initialGain = 0;
        if constexpr (! needDerivative) initialGain = -std::expm1(-initialU);
        double magnetisation = previous.magnetisation, irreversible = previous.irreversible, slope = 0;
        // Solve M = phi(H + alpha M) with a Newton-assisted mean-field
        // correction instead of three fixed-point evaluations. The material
        // law and winding clock stay unchanged; the second evaluation refines
        // the corrected operating point whenever its displacement is large.
        const int iterations = uncoupled ? 1 : 2;
        for (int iteration = 0; iteration < iterations; ++iteration)
        {
            const auto effective = field + parameters.alpha * magnetisation;
            const auto delta = effective - oldEffective;
            const auto distance = std::abs(delta);
            const auto u = distance * inverseK;
            double gain;
            if constexpr (needDerivative) gain = -std::expm1(-u);
            else
            {
                // Mean-field corrections are small (alpha is bounded above).
                // Reuse the exponential between the three implicit iterations;
                // the fifth-order correction retains the material trajectory.
                const auto correction = u - initialU;
                if (std::abs(correction) > .05) gain = -std::expm1(-u);
                else gain = initialGain + (1 - initialGain) * correction
                           * (1 - correction * (.5 - correction * (1.0 / 6 - correction * (1.0 / 24 - correction / 120))));
            }
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
            const bool needSlope = needDerivative || (!uncoupled && iteration == 0);
            const auto mid = needSlope ? evaluateLangevin<true>((effective - direction * meanDistance) * inverseA)
                                      : evaluateLangevin<false>((effective - direction * meanDistance) * inverseA);
            const auto end = uncoupled ? uncoupledEnd : needSlope ? evaluateLangevin<true>(effective * inverseA)
                                                               : evaluateLangevin<false>(effective * inverseA);
            const auto difference = mid.value - previous.irreversible;
            const bool moving = direction * difference > 0;
            irreversible = moving ? previous.irreversible + gain * difference : previous.irreversible;
            magnetisation = (1 - parameters.c) * irreversible + parameters.c * end.value;
            if (needSlope)
            {
                const auto irreversibleSlope = moving ? gain * mid.derivative * inverseA * meanDerivative
                                                          + direction * decay * inverseK * difference : 0.0;
                const auto effectiveSlope = (1 - parameters.c) * irreversibleSlope + parameters.c * end.derivative * inverseA;
                slope = effectiveSlope / std::max(.5, 1 - parameters.alpha * effectiveSlope);
                if (!uncoupled && iteration == 0)
                {
                    magnetisation += parameters.alpha * slope * (magnetisation - previous.magnetisation);
                    const auto correction = parameters.alpha * (magnetisation - previous.magnetisation);
                    const auto slopeOfDifference = mid.derivative * inverseA * meanDerivative;
                    // Below one millionth of a normalised field unit, retain
                    // the first-order correction of both M and Mirr. Stay away
                    // from reversals/pinning boundaries where the active branch
                    // can change. Larger moves use the full second evaluation.
                    if (std::abs(correction) < 1e-6 && std::abs(delta) > 2 * std::abs(correction)
                        && std::abs(difference) > 2 * std::abs(slopeOfDifference * correction))
                        return {field, std::clamp(magnetisation, -1.0, 1.0),
                                std::clamp(irreversible + irreversibleSlope * correction, -1.0, 1.0),
                                needDerivative ? slope : 0};
                }
            }
        }
        return {field, std::clamp(magnetisation, -1.0, 1.0),
                std::clamp(irreversible, -1.0, 1.0), std::max(0.0, slope)};
    }

    Material parameters;
    double inverseA = 1 / .3, inverseK = 5;
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
        JilesAtherton::State candidate;
        for (int iteration = 0; iteration < 6; ++iteration)
        {
            candidate = core.predict(field);
            residual = linearFlux * field + candidate.magnetisation + step * windingResistance * field * .5 - rhs;
            if (std::abs(residual) < 1e-11)
            {
                // predict() does not mutate the core. Reuse the converged
                // candidate instead of evaluating the identical field again.
                core.commit(candidate);
                return input - windingResistance * (field + old.field) * .5;
            }
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
