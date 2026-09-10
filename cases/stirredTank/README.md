# Stirred liquid tank: educational workflow

Read `constant/oxyProperties` first: its numbered sections define physical inputs,
source geometry, warm-up, actuator modes/constraints and measurement locations.
`0.backup` contains commented initial/boundary fields. Geometry is in metres,
time in seconds and oxygen in mol/m³. This is a closed liquid tank with a top lid.

## Tasks

1. Build/activate the solver and mesher as described in the repository README.
2. Run the native `wellMixed` check. Use `./Allmesh --smoke` here for a small
   integration mesh, then shorten endTime before `./Allrun`.
3. For the LES candidate, work in a clean copy, use `./Allmesh` and inspect
   `log.checkMesh`, `quality.json` and `mesh.foam`. The default configuration has
   **3,145,728 hex cells** and 192 axial layers. The smoke mesh has 3,072 cells and
   must never be used as evidence of wall resolution.
4. Choose a flow-only endTime and set oxygenStartTime beyond it. `./Allrun` performs
   explicit checking, initial-field copy, decomposition, simulation and latest-time
   reconstruction. The eight-rank setting is in `system/decomposeParDict`.
5. Inspect diagnostics and choose a developed-flow checkpoint. Edit startTime and
   endTime in controlDict and set oxygenStartTime to that checkpoint, then use
   `./Allrestart`. Keep the saved processor directories intact.
6. Compare oxygen transients and the molar balance before developing feedback.
   For the student experiment, select the student plug-in and implement its law
   separately; see `docs/controller.md`.

Example diagnostic command from this directory:

```bash
python3 ../../scripts/assess_flow.py . --after 2
```

The cutoff is your analysis choice, not a development assertion. Refine it after
examining the histories. The default 5 s oxygen activation and 10 s endTime are
provisional; uptake and control studies generally require much longer runs.

## Mesh and LES qualification

`mesh.json` uses the smooth-core O-grid with core_fraction=0.65, a graded outer
annulus, and axial sections with matched interface cell thicknesses. The grading
is intended to refine all physical walls. Geometry quality is checked separately
from flow-dependent wall units. Increasing total cell count alone does not ensure
wall resolution.

`wallResolution.csv` reports maximum y+ and local along-/cross-shear face extents
in wall units for each wall. Use y+ around or below 1 as an initial resolved-wall
target; tangential extents of order 50 along the shear and 20 across it are useful
conservative starting targets, not universal guarantees for a stirred tank.
Refine both tangential directions, including the end caps, wherever needed.

Built-in yPlus and wallShearStress fields aid spatial inspection. The CSV computes
viscous friction velocity from the wall-normal tangential velocity gradient;
face projections follow instantaneous shear. At zero shear, reported tangential
wall units are zero and do not indicate that a mesh is sufficient for later flow.

Inspect sustained fluctuations after the startup perturbation ends, stability of
statistics over independent windows, spectra, resolved/SGS activity and mesh/time
sensitivity. A smooth axisymmetric source can produce a strongly swirling flow
without adequate turbulent mixing. If that happens, revisit source geometry or
forcing physics rather than labelling the result turbulent because LES is on.

## Output and disk usage

`postProcessing/oxyControl/<startTime>/` contains:

- `measurements.csv`: physical time and each named oxygen measurement.
- `actuators.csv`: requested and applied omega/kLa.
- `oxygenBalance.csv`: per-timestep total molar inventory, transfers and residual.
- `flowMeasurements.csv`: velocity histories at the same points, for qualification
  only; these are not passed to the student controller.
- `wallResolution.csv`: sampled wall-unit maxima by patch.

The default ASCII field output can become large on the research mesh. Decide how
many restart checkpoints and analysis snapshots you need before long runs; retain
complete selected checkpoints rather than isolated U fields. Allclean deletes
numeric time directories, processor directories, the generated mesh and outputs;
it preserves the commented inputs. It never runs automatically.
