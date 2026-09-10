# Verification and physical qualification

## Implemented fast checks

`tests/Alltest` compiles the standalone reaction/actuator kernel and the student
plug-in with GCC C++17 and strict warnings. It checks frozen warm-up, activation
boundaries, positive conservative implicit chemistry over stiff parameter ranges,
first-order convergence against the exact pure-transfer transient, finite command
validation and slew limits. It also checks shell and Python syntax.

These checks passed in the development workspace. Generation of the 3,072-cell
smoke O-grid passed the mesher's checks: maximum non-orthogonality about 5.80°,
maximum core-interface cell-size ratio 1.048, smoothing objective reached.
Native checkMesh was not available in that workspace.

## Native gates

`cases/wellMixed/Allrun` validates actual module loading and uniform chemistry,
including frozen oxygen and inventory during warm-up. `tests/native_suite.py`
runs the uniform case serially and on two ranks, compares a restarted calculation
with its continuous reference, then exercises the coarse O-grid LES case with
oxygen activation and student-controller loading. Logs are preserved for failures.

The self-hosted workflow runs only trusted same-repository changes from the owner.
Native compilation/runtime results must be read from the associated workflow run;
they are not inferred from standalone checks. Full source-installation testing is
separate from using an existing Foundation 13 environment.

## Before research claims

The full turbulent tank has not been physically qualified by a short CI job.
Demonstrate mesh and timestep sensitivity, acceptable wall units over the intended
omega range, sustained turbulent fluctuations after finite startup perturbations,
and statistically developed mixing before selecting oxygenStartTime. Validate
stirrer forcing and oxygen supply/uptake against suitable reference data.

Check closed-vessel oxygen conservation and positivity over the full transient.
Assess upwind scalar numerical diffusion and first-order splitting sensitivity.
Study measurement-location and containing-cell sensitivity, sampling interval and
control delay. Recheck mesh adequacy when control raises the rotational command.
An accepted checkMesh, high nominal Reynolds number, or nonzero fluctuation RMS
alone is not sufficient for a wall-resolved turbulent LES claim.
