// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace oxy {
struct Command { double omega; double kla; }; // rad/s, 1/s
struct Measurement { std::string name; double concentration; }; // mol/m3
struct Observation {
    double time, elapsed; // s
    std::vector<Measurement> probes;
    Command applied;
};
// No mesh, full oxygen field, minimum concentration or stress is exposed here.
class Controller {
public:
    virtual ~Controller() = default;
    virtual Command update(const Observation&) = 0;
    virtual std::vector<double> save() const { return {}; }
    virtual void restore(const std::vector<double>& state) {
        if (!state.empty()) throw std::runtime_error("Unexpected student controller state");
    }
};
inline double limited(double requested,double old,double low,double high,double rate,double dt) {
    if (!std::isfinite(requested) || !std::isfinite(old) || !std::isfinite(low) || !std::isfinite(high) || !std::isfinite(rate) || !std::isfinite(dt) || !(low<=high) || rate<0 || dt<0)
        throw std::runtime_error("Invalid actuator command/limits");
    return std::clamp(std::clamp(requested,old-rate*dt,old+rate*dt),low,high);
}
// Activate only complete intervals starting at or after the selected time.
inline bool activeStep(double time,double dt,double start) {
    return time-dt >= start-1e-9*dt;
}
// Conservative local backward-Euler reaction solve, independently testable.
// (x-c)/dt = a*(sat-x) - q*x/(half+x), x >= 0.
inline double monodStep(double c,double dt,double a,double sat,double q,double half) {
    if(!std::isfinite(c+dt+a+sat+q+half) || c<0 || dt<=0 || a<0 || sat<0 || q<0 || half<=0)
        throw std::runtime_error("Invalid reaction data");
    const double A=1+dt*a, b=A*half-c-dt*a*sat+dt*q, rhs=(c+dt*a*sat)*half;
    const double root=std::sqrt(b*b+4*A*rhs);
    return b>=0 ? (rhs==0?0:2*rhs/(b+root)) : (-b+root)/(2*A);
}
}
