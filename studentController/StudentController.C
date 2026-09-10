// SPDX-License-Identifier: GPL-3.0-or-later
#include "Controller.hpp"

// Student extension point. Compile with ./Allwmake after changing this file.
// This template holds the previous applied commands: no PI/PID law is imposed.
// Add your own private state and implement save()/restore() if it is stateful.
class StudentController final : public oxy::Controller {
public:
    oxy::Command update(const oxy::Observation& measured) override {
        // measured.probes contains named local oxygen measurements [mol/m3].
        // measured.time and elapsed are seconds; applied is the LIMITED command.
        return measured.applied;
    }
};
extern "C" oxy::Controller* createOxyController() { return new StudentController; }
