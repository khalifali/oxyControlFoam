# Warm-up, sampling and restart

## Meaning of activation

For fixed timestep dt, an oxygen step over `[t-dt,t]` runs only if its **start** is
at or after `oxygenStartTime`. Thus at t=5 s with activation=5 s, C is still the
initial field. At t=5+dt the first transported/reacted field is available. Oxygen
advection, diffusion, supply and uptake are all frozen beforehand. Sampling,
velocity evolution and LES dynamics continue normally.

Activation times must be nonnegative multiples of dt. `sampleInterval` must be a
positive multiple of dt. Automatic timestep adjustment is rejected in this first
version. If you reduce dt on restart, keep activation and sampling aligned.

`controlStartTime` only gates the student controller. Prior to that time, its last
applied commands are held. Constant and prescribed modes remain useful for
open-loop development; prescribed schedules use absolute simulation time. Supply
commands can be logged before oxygen activation but cannot transfer oxygen then.

At a sample, the controller receives the current oxygen measurements and the
previously applied command. New commands are limited and held for the next fluid
and oxygen steps. At t=0 the initial observation is taken before integration; its
elapsed value is the configured sample interval. No interpolation in space is
performed: each probe measures its containing cell. For a decomposition-interface
point, one owner rank is selected to prevent double counting.

## Recommended two-stage experiment

1. Set oxygenStartTime beyond the planned flow-only endTime. Keep a fixed stirrer
   command and run the fluid. Do not infer development from a preset number of
   revolutions alone.
2. Inspect post-startup velocity histories, means/fluctuations, wall units,
   spectra and spatial fields. Extend the flow-only run if statistics still drift.
3. Retain a written checkpoint on **every processor**, including U, p, phi, oxygen,
   nut, flm, fmm and `uniform/oxyControlState`. Keep all other files at the checkpoint
   too, including any history fields and function-object state.
4. In `controlDict`, set startTime to that checkpoint and endTime to the intended
   oxygen experiment end. Set oxygenStartTime to the checkpoint or a later time;
   set controlStartTime separately. Run `./Allrestart`.
5. The scalar is resumed as stored, not reset. The LES and controller states and
   cumulative oxygen inventory are preserved. CSVs use separate start-time folders.

Do not remesh, change decomposition, change controller implementation/state format,
or change probe names/positions and sampling interval within a continuous restart.
The code rejects incompatible probe/controller-mode changes. Use a fresh copied
case for a different experiment; changing the controller law is a new experiment
unless its author provides a compatible state migration.

The code uses the native fluid restart behavior. Bitwise continuation is not
promised for higher-order fluid history and function objects; native serial/MPI
restart comparisons remain part of validation. To replay a checkpoint with new
parameters, preserve a separate copy of the original case and its provenance.

Output segments are appended if the exact same start time is reused; a header may
repeat. The supplied diagnostic summarizer ignores repeated headers and lets later
restart segments replace duplicate times. Preserve separate experiment directories
instead of mixing physically different runs into those segments.
