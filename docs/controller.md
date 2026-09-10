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
No controller choice, gains, objective or estimator is supplied.

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
