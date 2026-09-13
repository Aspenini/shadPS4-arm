#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Gets the FEX submodule into a state where FEXCore can be cross compiled:
# initialises only the submodules FEXCore needs, and applies the patch that
# makes FEX's CMake tolerate Android and cross compilation.
#
# Kept idempotent so it can run on every configure.

set -euo pipefail

FEX_DIR=""
PATCH=""

while [ $# -gt 0 ]; do
    case "$1" in
        --fex-dir) FEX_DIR="$2"; shift 2 ;;
        --patch) PATCH="$2"; shift 2 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

[ -n "$FEX_DIR" ] || { echo "--fex-dir is required" >&2; exit 1; }
[ -n "$PATCH" ] || { echo "--patch is required" >&2; exit 1; }
[ -d "$FEX_DIR" ] || { echo "FEX submodule not found at $FEX_DIR" >&2; exit 1; }
[ -f "$PATCH" ] || { echo "patch not found at $PATCH" >&2; exit 1; }

# Resolve before changing directory, so relative paths still work.
PATCH="$(cd "$(dirname "$PATCH")" && pwd)/$(basename "$PATCH")"

cd "$FEX_DIR"

# FEX carries three submodules of prebuilt test binaries -- fex-gvisor-tests-bins alone is over
# 700 MB -- which nothing in FEXCore needs. Initialise only what the library actually builds
# against rather than recursing over everything.
NEEDED="
External/vixl
External/fmt
External/xxhash
External/unordered_dense
External/robin-map
External/tiny-json
External/Catch2
External/range-v3
External/jemalloc_glibc
External/rpmalloc
External/drm-headers
External/Vulkan-Headers
Source/Common/cpp-optparse
"

# Match on paths, not submodule names: they are not always the same. FEX registers
# Source/Common/cpp-optparse under the name External/cpp-optparse, for instance.
ALL_PATHS="$(git config -f .gitmodules --get-regexp '^submodule\..*\.path$' | awk '{print $2}')"

for module in $NEEDED; do
    # Not every path exists in every FEX revision; skip the ones that do not.
    case " $(echo $ALL_PATHS) " in
        *" $module "*) ;;
        *) continue ;;
    esac
    if [ ! -e "$module/.git" ]; then
        echo "initialising $module"
        git submodule update --init --depth 1 -- "$module"
    fi
done

# Apply the patch only when it is not already in place, so repeated configures are harmless.
if git apply --check --reverse "$PATCH" >/dev/null 2>&1; then
    echo "FEX patch already applied"
elif git apply --check "$PATCH" >/dev/null 2>&1; then
    git apply "$PATCH"
    echo "FEX patch applied"
else
    echo "error: $PATCH does not apply to this FEX revision and is not already applied." >&2
    echo "       The submodule has probably moved; the patch needs refreshing." >&2
    exit 1
fi
