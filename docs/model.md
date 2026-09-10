# Model, units and numerical choices

## Liquid and stirring

This is an isothermal, incompressible, single-liquid model in a closed cylindrical
tank of radius 0.05 m and height 0.15 m. All three boundaries are stationary,
no-slip walls, including the top lid. There is no free surface, resolved blade,
bubble phase, gas holdup, or liquid inlet/outlet.

The kinematic momentum equation receives acceleration

`a_stir = w_stir/tau * (omega e_z × (x - centre) - U)`.

The damping in U is implicit in the momentum matrix. `tau` sets forcing relaxation;
`omega` is the rotational target in rad/s, not a measured blade speed. The compact
support uses `w = (1-r²/R²)² (1-z²/H²)²` inside its cylinder and zero outside,
where H is the half-height. It and its first derivative vanish at the support edge.
A finite 0.2 s perturbation breaks the initially exact symmetry; it does not supply
ongoing turbulence forcing. The source has no explicit blade-induced axial pumping
model. Establish its mixing behavior rather than assuming a real impeller's flow.

The example uses water-like `nu=1e-6 m²/s`; `rho=998 kg/m³` is documented in
oxyProperties for interpreting dimensional forces but is not needed in the
kinematic momentum equation. The source radius is 0.03 m and omega is 30 rad/s,
giving nominal `Re_omega=omega*R_source²/nu=27000`. This scaling is not the usual
impeller `N*D²/nu` definition and is not evidence of developed turbulence.

## Oxygen equation

When enabled:

`dC/dt + div(U C) = div((D + nut/Sc_t) grad(C)) + w_supply*kLa*(Cstar-C) - qmaxV*C/(KO+C)`.

| Quantity | Example | SI unit |
|---|---:|---|
| C initial | 0.15 | mol/m³ |
| Cstar | 0.25 | mol/m³ |
| KO | 0.01 | mol/m³ |
| qmaxV | 0.002 | mol/(m³ s) |
| D | 2e-9 | m²/s |
| Sc_t | 0.7 | dimensionless |
| Local kLa | 1 | 1/s |

These are illustrative liquid-phase and biological parameters, not measurements
for a particular organism or tank. Identify uptake and oxygen-transfer parameters
experimentally for a thesis case. Uniform biomass is absorbed into `qmaxV`; a
prescribed demand disturbance multiplies it at an absolute physical time.

The supply term models dissolved-oxygen transfer from an implicit reservoir,
localized in the lower tank. It does not inject liquid. It becomes negative above
saturation, allowing desorption. A controller therefore changes the *effective
local transfer coefficient*, not a calibrated gas flow rate. Connecting it to
physical gas flow needs an empirical mass-transfer relation and applicable bounds.

For a spatially uniform concentration, whole-vessel effective kLa is
`kLa_local * integral(w_supply dV)/V`. In the default cylinder this ratio is about
0.0384, so local kLa=1/s corresponds to about 0.0384/s under that uniform-C
approximation. For nonuniform C use the actual volume integral; the CSV budget
records it. At `qmaxV=0.002`, the homogeneous balance predicts a concentration of
roughly 0.20 mol/m³ for the initial illustrative settings.

## Discretization and accounting

The fluid uses the inherited Foundation 13 PIMPLE pressure/velocity algorithm.
The LES model is native `dynamicLagrangian`, with dynamic Smagorinsky coefficient
averaging and cube-root-volume filter width. No wall-function boundary conditions
are used. See the [upstream model implementation](https://github.com/OpenFOAM/OpenFOAM-13/blob/master/src/MomentumTransportModels/momentumTransportModels/LES/dynamicLagrangian/dynamicLagrangian.C).

Oxygen advances once per complete fluid timestep, after the pressure-corrected
flux is available. Euler transport is followed by a local backward-Euler supply
and Monod reaction solve. The nonnegative quadratic root solves the reaction
without clipping and permits conservative inventory accounting. This splitting
is first order in time. The default oxygen upwind discretization is dissipative;
spatial and temporal refinement are required before interpreting oxygen gradients
or sensor-response delays. Momentum uses backward time stepping and central
convection. Monitor Courant number; fixed deltaT is not an automatic stability guarantee.

`oxygenBalance.csv` reports total moles, cumulative net supply, cumulative uptake,
minimum/maximum C, and `inventory - initialInventory - supply + uptake`.
The budget is valid for the supplied closed, zero-flux scalar boundaries. Open
boundaries would require adding their advective/diffusive transfers to the budget.
Significant negative transported oxygen fails explicitly instead of silently
changing the inventory. Warm-up freezes the scalar and both cumulative transfers.

This is wall-resolved **velocity LES intent**, not DNS of the high-Schmidt-number
oxygen boundary layers. No reactive solid-wall boundary flux is prescribed.
