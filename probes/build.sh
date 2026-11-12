#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
#
# Builds the probes inside the pinned BlocksDS container.
#   ./build.sh                      -> every probe ROM (probes-l4-*.nds)
#   ./build.sh DEMO=l4-3d-fifo      -> probes-l4-3d-fifo.nds
#   ./build.sh -j8 release          -> build/release/{demos,judge}
set -eu

cd "$(dirname "$0")"
IMAGE=probes-build:1

docker build -q -t "$IMAGE" . >/dev/null
exec docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp \
    -v "$PWD":/work -w /work "$IMAGE" make "$@"
