# Student controller extension

Edit `studentController/StudentController.C` and rebuild with `./Allwmake`. Select
`controller student` in oxyProperties. A shared library exposes `createOxyController`
and returns an implementation of `oxy::Controller`. Keep the solver core unchanged
while developing a controller or soft sensor.

The observation contains physical time in seconds, elapsed time since the previous
sample, a named list of oxygen concentrations in mol/m³, and the previously applied
omega/kLa command. It deliberately contains no full oxygen field, global minimum,
velocity field or ground-truth oxygen-deficiency volume. The student can estimate
hidden states using measurements and their own model.

Return `Command{omega_rad_per_s, local_kLa_per_s}`. Independent bounds and slew-rate
limits are applied by the solver. `actuators.csv` records both requested and applied
commands, so saturation and rate limiting are visible. The observation exposes the
applied command to support an appropriate student-designed anti-windup method.
The default template imposes no controller choice, gains, objective or estimator.

Stateful controllers must override `save()` and `restore()` using a vector of
numbers. Include a state-format version if useful; reject an incompatible vector.
Save integrators, filtered measurements, observer states and any reproducible
random-generator state your algorithm needs. The solver stores this vector beside
its own actuator and inventory state at written checkpoints. The template controller
is stateless and holds the existing command.

Only the MPI master calls the plug-in. It receives globally gathered measurements,
then the solver broadcasts the requested/applied action. The plug-in must not call
MPI collectives or access rank-local mesh data. C++ exceptions are converted into
an OpenFOAM fatal error. Use the same compiler/standard-library ABI as the solver.
Do not recompile or replace the shared library during a running simulation.

`constant` holds initial commands; `prescribed` interpolates `(time omega kLa)`
rows to provide repeatable open-loop actuator experiments. Both use the same
constraints and CSV outputs. Changing omega changes source-driven fluid dynamics;
changing kLa changes the oxygen source. There is no imposed direct mapping from
omega to kLa in this first model.

## Optional educational PI example

`studentController/examples/OxygenPI.C` is a separate, commented example; the
default `StudentController.C` remains the stateless template. The example holds
omega constant and controls local kLa using the arithmetic mean of the probes.
It uses error = target - measured mean, a proportional contribution Kp*error,
and an integral contribution updated by Ki*error*elapsed. The gains are illustrative.

To select it, change only the source line in `studentController/Make/files` from
`StudentController.C` to `examples/OxygenPI.C`, keeping the LIB line unchanged.
With the OpenFOAM environment active, run `./Allwmake` from the repository root
and select `controller student` in the case's oxyProperties. To return to the
empty template, restore the original source line and rebuild. Never compile both
sources into the same library: each defines the same factory function.

Start a fresh case when switching from the empty template: the PI example expects
a checkpoint vector containing its integral contribution. Continuing a PI run
restores that contribution. Do not replace the library while a simulation runs.

The example includes conditional integration for amplitude saturation. Match its
kLa bounds to the case; if solver slew-rate limits constrain the actuator, extend
anti-windup using the previously applied command. Inspect actuators.csv when tuning.

An acceptable probe mean does not guarantee acceptable oxygen everywhere. Use
offline CFD fields to identify vulnerable regions and compare probe layouts.
For example, readings 0.30 and 0.06 mol/m3 average to the 0.18 target while one
probe is oxygen-poor. A minimum-probe objective can protect sampled locations,
but cannot guarantee conditions between them. Assess the controller against the
full-field volume fraction below a chosen oxygen threshold and the duration of
deficiency, without exposing these evaluation fields to the controller. Validate
over changes in demand and stirring, not only the scenario used to place probes.
