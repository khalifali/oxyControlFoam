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
Native checkMesh was not available in that workspace. A separate one-layer
extrusion of the full-resolution research cross-section (16,384 cells) also passed
the mesher's positive-volume, orientation and closure checks; maximum
non-orthogonality was 10.88° and the maximum interface area ratio was 1.178.
That check does not establish three-dimensional wall resolution.

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

## Recorded native result

[Runner job: successful](https://github.com/khalifali/LAMFOAM/actions/runs/34525129120)
on 2026-09-10, `lamfoam-local`, oxyControlFoam revision
`13f60c3492bcc86c4a8c9a45837469c3b3c36afe`.

- Full upstream OpenFOAM 13 Allwmake and both custom libraries compiled.
- Combined activation loaded the solver and installed cylinder-ogrid 0.2.0.
- Serial and two-rank uniform chemistry matched the independent reference;
  the warm-up field and cumulative transfers remained frozen for ten timesteps.
- Continuous and restarted two-rank uniform runs agreed within 1e-11 in all
  balance columns, including time and concentrations. A test-only controller's
  persisted call count was restored on both ranks.
- The 3,072-cell O-grid passed native checkMesh and short dynamic-Lagrangian LES
  startup, delayed oxygen activation and checkpoint continuation. Positive uptake
  and supply and the closed-vessel balance checks passed.
- The installer used an independent copy of matching cached OpenFOAM sources/build
  objects and an authorized workstation mesher checkout. Its apt dependency
  installation branch was not run.

The workflow's archived logs and CSV outputs document these checks. The uniform
restart comparison checks numerical equivalence for that controlled case; the
short LES continuation checks successful restoration/advancement, not bitwise LES
trajectory identity.

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
