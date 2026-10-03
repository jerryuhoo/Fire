#pragma once

#include <algorithm>
#include <cmath>

namespace fire::analog
{
// A normalised Thevenin supply and coupling capacitor. Voltage is expressed
// relative to the unloaded rail, and current relative to the output load.
// C dV/dt = (Vs - V)/Rs - Iload. Quiescent demand is included in Vs, so the
// equilibrium rail at silence is 1. The exact RC update is sample-rate invariant.
class PowerSupply
{
public:
    struct Circuit
    {
        double resistance = 0.2;
        double rechargeSeconds = 0.08;
        double biasResistance = 0.02;
        double biasRecoverySeconds = 0.12;
    };

    void prepare(double rate, Circuit circuit) noexcept
    {
        parameters = circuit;
        const auto safeRate = std::isfinite(rate) && rate > 0 ? rate : 48000.0;
        recharge = -std::expm1(-1.0 / (safeRate * std::max(0.001, circuit.rechargeSeconds)));
        biasDecay = std::exp(-1.0 / (safeRate * std::max(0.001, circuit.biasRecoverySeconds)));
    }

    void reset() noexcept { voltage = 1; bias = current = 0; }

    void advance(double loadCurrent, double gridCurrent) noexcept
    {
        current = std::isfinite(loadCurrent) ? std::clamp(loadCurrent, 0.0, 16.0) : 0.0;
        const auto railTarget = std::clamp(1.0 - parameters.resistance * current, 0.25, 1.0);
        voltage += recharge * (railTarget - voltage);
        const auto chargingCurrent = std::isfinite(gridCurrent) ? std::clamp(gridCurrent, 0.0, 16.0) : 0.0;
        // Positive grid conduction charges the coupling capacitor and shifts
        // the operating point towards cutoff; the bias resistor discharges it.
        bias = std::clamp(bias * biasDecay
                         - parameters.biasResistance * (1.0 - biasDecay) * chargingCurrent,
                         -0.4, 0.0);
    }

    double getVoltage() const noexcept { return voltage; }
    double getBias() const noexcept { return bias; }
    double getLoadCurrent() const noexcept { return current; }

private:
    Circuit parameters;
    double recharge = 0, biasDecay = 0, voltage = 1, bias = 0, current = 0;
};
}
