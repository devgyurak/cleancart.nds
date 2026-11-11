#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
#
# Builds Lightenette inside the pinned BlocksDS container.
#   ./build.sh                 -> lightenette.nds
#   ./build.sh DEMO=2d-blend   -> lightenette-2d-blend.nds
#   ./build.sh demos           -> every feature demo ROM
#   ./build.sh SAVE=flash      -> save-chip variant
set -eu

cd "$(dirname "$0")"
IMAGE=lightenette-build:1

docker build -q -t "$IMAGE" . >/dev/null
exec docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp \
    -v "$PWD":/work -w /work "$IMAGE" make "$@"
