# Native oxygen verification

This deliberately small **laminar** case checks the solver independently of the
stirred-tank LES. An 8×8×8 box has zero velocity, uniform initial C=0.15 mol/m³,
uniform transfer kLa=0.02/s, and the same Monod kinetics as the lesson.

`Allrun` generates/checks the mesh, copies `0.backup`, advances 20 steps of 0.001 s
and compares every reported concentration and total molar inventory with an
independently computed implicit-reaction reference. Oxygen activates at 0.01 s:
the first ten steps must preserve C and accumulate no supply or uptake.

The test checks native module loading, dictionary/field compatibility, scalar
activation and closed-box conservation. It does not validate mixing or LES.
`Allclean` deliberately removes its generated results. Run the serial case before
the tank; `tests/native_suite.py` adds MPI, restart and short LES integration checks.
