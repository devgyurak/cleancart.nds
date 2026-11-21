#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
#
# Builds Quayholt inside the pinned BlocksDS container.
#   ./build.sh                 -> quayholt.nds
#   ./build.sh DEMO=rehearsal  -> quayholt-rehearsal.nds
#   ./build.sh demos           -> every demo ROM (rehearsal)
#   ./build.sh SAVE=flash      -> save-chip variant
set -eu

cd "$(dirname "$0")"
IMAGE=quayholt-build:1

docker build -q -t "$IMAGE" . >/dev/null
exec docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp \
    -v "$PWD":/work -w /work "$IMAGE" make "$@"
