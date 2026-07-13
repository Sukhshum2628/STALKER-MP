#!/usr/bin/env bash
# STALKER-MP — emits the current git revision for build stamping.
# Counterpart of print_git_revision.ps1 for POSIX build environments
# (e.g. compiling the test suite with GCC/Clang):
#   g++ ... -DSMP_GIT_REVISION="\"$(scripts/print_git_revision.sh)\""
# Unstamped builds report "unstamped" (include/stalkermp/core/Version.h).

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

revision="$(git -C "${script_dir}/.." rev-parse --short=12 HEAD)"
if [[ -n "$(git -C "${script_dir}/.." status --porcelain)" ]]; then
    revision="${revision}-dirty"
fi

printf '%s\n' "${revision}"
