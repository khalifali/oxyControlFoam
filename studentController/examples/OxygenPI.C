// SPDX-License-Identifier: GPL-3.0-or-later
#include "Controller.hpp"

// Optional teaching example. Not compiled by default; see docs/controller.md.
// Regulate the arithmetic mean of the oxygen probes by changing local kLa.
// Keep stirring fixed. These gains are illustrative, not tuned for the tank.
class StudentController final : public oxy::Controller {
    const double target_ = 0.18;  // mol/m3
    const double kp_ = 5.0;       // m3/(mol s)
    const double ki_ = 0.2;       // m3/(mol s2)
    const double klaBase_ = 1.0;  // 1/s
    // Match these to the case actuator bounds.
    const double klaMin_ = 0.0, klaMax_ = 3.0; // 1/s
    double integral_ = 0.0;      // Integral contribution, 1/s

public:
    oxy::Command update(const oxy::Observation& measured) override {
        if (measured.probes.empty())
            throw std::runtime_error("PI controller needs an oxygen probe");
        if (!std::isfinite(measured.elapsed) || measured.elapsed < 0.0)
            throw std::runtime_error("Invalid PI sample interval");

        // 1. Average sensor readings (not a vessel volume average).
        double concentration = 0.0;
        for (const auto& probe : measured.probes) {
            if (!std::isfinite(probe.concentration))
                throw std::runtime_error("Invalid PI oxygen measurement");
            concentration += probe.concentration;
        }
        concentration /= measured.probes.size();

        // 2. Positive error calls for more oxygen supply.
        const double error = target_ - concentration;
        const double trialIntegral = integral_ + ki_ * error * measured.elapsed;
        const double trialCommand = klaBase_ + kp_ * error + trialIntegral;

        // 3. Conditional integration prevents amplitude-limit windup.
        // Integration remains allowed when it helps leave saturation.
        // This does not compensate for additional solver slew-rate limits.
        const bool pushesAbove = trialCommand > klaMax_ && error > 0.0;
        const bool pushesBelow = trialCommand < klaMin_ && error < 0.0;
        if (!pushesAbove && !pushesBelow) integral_ = trialIntegral;

        // 4. Return requested supply and unchanged applied stirring speed.
        const double kla = std::clamp(
            klaBase_ + kp_ * error + integral_, klaMin_, klaMax_);
        return {measured.applied.omega, kla};
    }

    // Preserve integral memory across checkpoints; reject other state formats.
    std::vector<double> save() const override { return {integral_}; }
    void restore(const std::vector<double>& state) override {
        if (state.size() != 1 || !std::isfinite(state[0]))
            throw std::runtime_error("Invalid PI controller restart state");
        integral_ = state[0];
    }
};
extern "C" oxy::Controller* createOxyController() { return new StudentController; }
