#!/usr/bin/env bash
# Reproducible source build on Ubuntu 24.04 (or a compatible Debian system).
# Execute this file; afterward source the printed activate-oxyControlFoam.sh.
# Existing repositories are checked, never reset or silently updated.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
prefix="$HOME/OpenFOAM-oxy"
jobs=4
install_deps=false
mesher_path=
# CI must fail clearly if authentication is unavailable, never wait for input.
export GIT_TERMINAL_PROMPT=0 GCM_INTERACTIVE=Never PIP_NO_INPUT=1
while (($#)); do
    case $1 in
        --prefix) prefix=${2:?Missing installation directory}; shift 2 ;;
        --jobs) jobs=${2:?Missing job count}; shift 2 ;;
        --mesher-path) mesher_path=${2:?Missing local mesher checkout}; shift 2 ;;
        --install-deps) install_deps=true; shift ;;
        --help) echo "Usage: $0 [--prefix DIR] [--jobs N] [--install-deps] [--mesher-path DIR]"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done
[[ $jobs =~ ^[1-9][0-9]*$ ]] || { echo 'jobs must be a positive integer' >&2; exit 2; }
# Upstream wmake scripts require source paths without whitespace.
[[ $prefix != *[[:space:]]* && $repo != *[[:space:]]* ]] || { echo 'Use paths without whitespace.' >&2; exit 2; }
if [[ -n $mesher_path && ! -f "$mesher_path/pyproject.toml" ]]; then
    echo "Missing authorized mesher checkout: $mesher_path" >&2; exit 1
fi
mkdir -p -- "$prefix"
prefix=$(cd -- "$prefix" && pwd)
# Task 1: optional system dependencies. Only this explicit flag uses sudo/apt.
if $install_deps; then
    command -v apt-get >/dev/null || { echo 'Dependency installation requires apt-get.' >&2; exit 1; }
    privileged=(); if ((EUID!=0)); then privileged=(sudo); fi
    "${privileged[@]}" apt-get update
    "${privileged[@]}" apt-get install -y build-essential git flex bison cmake zlib1g-dev \
        libopenmpi-dev openmpi-bin libreadline-dev libncurses-dev libxt-dev \
        python3 python3-venv python3-pip
fi
for tool in git g++ make flex bison cmake mpicc python3; do
    command -v "$tool" >/dev/null || { echo "Missing $tool: install dependencies first." >&2; exit 1; }
done
# Task 2: obtain exact reviewed upstream revisions. A rerun resumes their builds.
source "$repo/scripts/upstream-revisions.sh"
checkout() {
    local name=$1 revision=$2 destination="$prefix/$1"
    if [[ ! -e $destination ]]; then
        git clone "https://github.com/OpenFOAM/$name.git" "$destination"
        git -C "$destination" checkout --detach "$revision"
    else
        [[ -d $destination/.git ]] || { echo "Not a Git checkout: $destination" >&2; exit 1; }
        [[ $(git -C "$destination" rev-parse HEAD) == "$revision" ]] || { echo "Unexpected revision in $destination; choose another prefix." >&2; exit 1; }
        [[ -z $(git -C "$destination" status --porcelain --untracked-files=no) ]] || { echo "Tracked edits in $destination; keep them and choose another prefix." >&2; exit 1; }
    fi
}
checkout OpenFOAM-13 "$OPENFOAM_REVISION"
checkout ThirdParty-13 "$THIRDPARTY_REVISION"
# Task 3: initialise OF, using its bundled Scotch/Zoltan and system OpenMPI.
# Upstream startup files are not written for nounset/errexit; restore our flags.
set +eu
source "$prefix/OpenFOAM-13/etc/bashrc" WM_COMPILER=Gcc WM_MPLIB=SYSTEMOPENMPI SCOTCH_TYPE=ThirdParty ZOLTAN_TYPE=ThirdParty
set -eu
[[ ${WM_PROJECT_VERSION:-} == 13 && ${WM_PROJECT_DIR:-} == "$prefix/OpenFOAM-13" ]] || exit 1
export WM_NCOMPPROCS="$jobs"
# Task 4: build ThirdParty + OF libraries/applications (upstream Allwmake does both).
# Expect a substantial build: choose jobs for available RAM, not only CPU count.
(cd "$prefix/OpenFOAM-13" && ./Allwmake -j "$jobs") 2>&1 | tee "$prefix/log.OpenFOAM13"
command -v foamRun >/dev/null
command -v checkMesh >/dev/null
# Task 5: build this module and the student's independent controller plug-in.
"$repo/Allwmake" 2>&1 | tee "$prefix/log.oxyControlFoam"
# Task 6: isolate Python mesh dependencies; pin the existing O-grid implementation.
python3 -m venv "$prefix/mesh-venv"
if [[ -n $mesher_path ]]; then
    [[ -f "$mesher_path/pyproject.toml" ]] || { echo "Missing mesher checkout: $mesher_path" >&2; exit 1; }
    "$prefix/mesh-venv/bin/python" -m pip install "$mesher_path"
    git -C "$mesher_path" rev-parse HEAD > "$prefix/mesher-revision.txt"
else
    "$prefix/mesh-venv/bin/python" -m pip install \
        'git+https://github.com/khalifali/cylinder-ogrid.git@447b3831e5f85e4e6cbd22135f9ccc9322a94003' || {
        echo 'cylinder-ogrid requires authorized Git access. Retry with --mesher-path /path/to/your/checkout.' >&2
        exit 1
    }
    echo 447b3831e5f85e4e6cbd22135f9ccc9322a94003 > "$prefix/mesher-revision.txt"
fi
"$prefix/mesh-venv/bin/python" -m pip freeze > "$prefix/python-packages.txt"
# Task 7: generate one activation file. Do not modify the user's shell startup.
activate="$prefix/activate-oxyControlFoam.sh"
{
    printf '# Source this file in Bash to activate OF13, the solver and mesher.\n'
    printf 'oxy_saved_flags=$-\nset +eu\n'
    printf 'source %q WM_COMPILER=Gcc WM_MPLIB=SYSTEMOPENMPI SCOTCH_TYPE=ThirdParty ZOLTAN_TYPE=ThirdParty\n' "$prefix/OpenFOAM-13/etc/bashrc"
    printf 'source %q\n' "$prefix/mesh-venv/bin/activate"
    printf 'source %q\n' "$repo/scripts/activate.sh"
    printf 'case $oxy_saved_flags in *u*) set -u ;; esac\ncase $oxy_saved_flags in *e*) set -e ;; esac\nunset oxy_saved_flags\n'
} > "$activate"
printf '\nBuild complete. Activate this terminal with:\n  source %q\n' "$activate"
printf 'Then run the native verification case before the LES tank:\n  cd %q && ./Allrun\n' "$repo/cases/wellMixed"
