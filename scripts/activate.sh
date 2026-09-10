# Source this file AFTER your existing OpenFOAM Foundation 13 environment.
# No LAMMPS, LAMFOAM coupling or DEM executable is required.
if [[ ${WM_PROJECT_VERSION:-} != 13 ]]; then
    echo 'Source OpenFOAM Foundation 13 before oxyControlFoam.' >&2
    return 1
fi
export OXYCONTROL_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
case ":$PATH:" in *":$OXYCONTROL_ROOT/bin:"*) ;; *) export PATH="$OXYCONTROL_ROOT/bin:$PATH" ;; esac
