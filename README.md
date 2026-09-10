# oxyControlFoam

An educational OpenFOAM **Foundation 13** solver module for oxygen transport and
control experiments in a stirred liquid tank, without DEM particles.

The framework supplies a localized oxygen-transfer source, a volumetric Monod
uptake sink, configurable oxygen probes, and an interface for a student to write
an independent control strategy. The two actuator commands are rotational source
speed and local oxygen-transfer coefficient. The supplied controller simply holds
its commands; it does not implement a feedback law.

**Status:** initial implementation. Standalone numerical tests and a coarse mesh
generation check have passed. Native OpenFOAM compilation, serial/MPI integration,
restart equivalence, and turbulent wall-resolution qualification are still pending.
Do not interpret the LES dictionary or nominal Reynolds number as validation.

## Installation and activation

On Ubuntu 24.04 with Bash, from this repository:

```bash
./scripts/install-openfoam13.sh --install-deps --jobs 4 --prefix "$HOME/OpenFOAM-oxy"
source "$HOME/OpenFOAM-oxy/activate-oxyControlFoam.sh"
```

The installer builds pinned OpenFOAM-13 and ThirdParty-13 sources, this solver,
and the separate student-controller library. It also creates a Python environment
with a pinned cylinder-ogrid mesh generator. `--install-deps` explicitly enables
system package installation through apt/sudo; omit it when dependencies already
exist. Choose parallel build jobs for your available memory. Full compilation can
take substantial time and disk space.

The generated activation file loads **OpenFOAM, oxyControlFoam and the mesher**
together. Source it in each new terminal. An executed installer cannot change its
parent terminal's environment. It does not edit your shell startup files. Keep the
repository at its installation location, or rerun the installer after moving it.

If Foundation 13 is already installed:

```bash
source /your/OpenFOAM-13/etc/bashrc
./Allwmake
source scripts/activate.sh
# Install cylinder-ogrid in your chosen Python environment for tank meshing.
```

See [installation details](docs/installation.md), including dependency and build
logs. OpenCFD releases and older Foundation versions are not the target API.

## First runs

```bash
./tests/Alltest                         # No OpenFOAM required
cd cases/wellMixed
./Allrun                                # Native 512-cell chemistry/warm-up check
```

After that succeeds, test the coupled LES setup briefly on the coarse tank mesh:

```bash
cd ../stirredTank
./Allmesh --smoke                        # Integration mesh only, NOT WRLES
foamDictionary system/controlDict -entry endTime -set 0.02
./Allrun
```

Read [the tank tutorial](cases/stirredTank/README.md) before the research mesh and
longer run. `Allclean` deliberately removes generated results; `Allrun` protects
existing runs. `Allrestart` resumes a saved parallel tank checkpoint.

## Flow development before oxygen

In `constant/oxyProperties`:

```text
oxygenStartTime [0 0 1 0 0 0 0] 5;
controlStartTime [0 0 1 0 0 0 0] 5;
```

Until `oxygenStartTime`, oxygen remains at its loaded initial value: **advection,
diffusion, supply and uptake are all inactive**. Flow and LES continue evolving;
measurements and flow diagnostics are still written. The interval beginning at
5 s is the first oxygen-active interval. Set the time to zero for simultaneous
startup, or beyond `endTime` for a flow-only run.

`controlStartTime` gates student feedback independently. Constant and prescribed
commands can drive the flow throughout warm-up. Neither activation time is an
automatic development criterion. Inspect the flow, then continue from a saved
checkpoint with oxygen activation set to that time. See [restart and timing](docs/timing.md).

## What is included

- `src/oxyControlFoam`: module derived from Foundation 13 `incompressibleFluid`.
- `src/controller`: controller contract, bounded actuator updates and reaction kernel.
- `studentController`: a separate replaceable C++ controller library.
- `cases/stirredTank`: SI cylinder, dynamic Lagrangian Smagorinsky LES, graded O-grid.
- `cases/wellMixed`: a small native check with an independently computed reference.
- `scripts/assess_flow.py`: summaries of velocity histories and wall-unit diagnostics.
- [Physics and numerics](docs/model.md), [student interface](docs/controller.md),
  [qualification and tests](docs/validation.md).

All oxygen fields and measurement CSVs use **mol/m³**. For O2, multiply by 32 to
obtain mg/L. Open the generated `mesh.foam` with ParaView's OpenFOAM reader; enable
`oxygen` and the desired fluid/LES fields. In parallel, `Allrun` reconstructs the
latest written fields and retains processor directories for restart.

Licensed GPL-3.0-or-later; see [NOTICE](NOTICE) and [LICENSE](LICENSE).
