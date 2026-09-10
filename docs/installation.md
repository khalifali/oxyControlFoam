# Installation notes

The installer targets Linux/Bash, GCC, system OpenMPI, and OpenFOAM Foundation 13.
Ubuntu 24.04 is the intended dependency baseline. Other Debian-derived systems
may need package adjustments; macOS and Windows-native builds are not covered.
WSL2 with Ubuntu is a possible environment, but has not been tested here.

## Explicit tasks performed

1. Parse the destination and compile-job count; reject paths with whitespace
   because upstream build scripts use unquoted source paths.
2. Optionally install build-essential, Git, Flex, Bison, CMake, zlib development
   headers, OpenMPI, readline/ncurses/Xt development headers, and Python/venv/pip.
3. Clone OpenFOAM-13 and ThirdParty-13 and check out the exact commits recorded in
   `scripts/upstream-revisions.sh`. Existing mismatched or edited checkouts cause
   a stop rather than losing local work.
4. Source the upstream environment and compile the bundled ThirdParty components,
   libraries and applications using upstream `Allwmake`.
5. Build `liboxyControlFoam.so` and `liboxyStudentController.so` into
   `$FOAM_USER_LIBBIN` using `wmake`.
6. Install the pinned O-grid package in `<prefix>/mesh-venv`.
7. Write `<prefix>/activate-oxyControlFoam.sh`, which activates all three components.

OpenFOAM source and third-party build logs live at `<prefix>/log.OpenFOAM13`;
the solver build log is `<prefix>/log.oxyControlFoam`. Rerunning on the same
unmodified revisions resumes incremental compilation. The installer does not run
the expensive tank simulation. Run `cases/wellMixed/Allrun` after activation.

A normal run needs no root privileges. Only `--install-deps` installs system
packages. On a cluster, have the administrator provide the equivalent dependencies
or load the matching modules, then omit that flag. The script currently selects
system OpenMPI explicitly; adapt this deliberately for a site's MPI environment.

The full source-build installer has not been executed in the development
workspace. Shell checks have been performed; the first local build remains a
required integration gate. No system-wide shell configuration is modified.

Upstream references: [OpenFOAM-13](https://github.com/OpenFOAM/OpenFOAM-13),
[ThirdParty-13](https://github.com/OpenFOAM/ThirdParty-13), and the
[upstream build entry point](https://github.com/OpenFOAM/OpenFOAM-13/blob/master/Allwmake).

## Native CI runner

A self-hosted runner registered only to LAMFOAM cannot accept jobs for a new
repository automatically. To enable this repository's native workflow, register
a Linux/X64 runner for oxyControlFoam and set the Actions repository variable
`OXY_NATIVE_RUNNER_ENABLED` to `true`. Otherwise the native job is skipped and
the ordinary kernel/syntax workflow still runs. Owner-authored same-repository
changes are the only pull requests eligible for native execution.

The initial implementation is being validated through a pinned, CI-only draft
workflow in LAMFOAM using the owner's existing `lamfoam-local` runner. It builds
in an independent directory and does not change the LAMFOAM solver.
