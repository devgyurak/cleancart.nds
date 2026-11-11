#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
#
# Headless melonDS 1.1 core runner (tools/melonds/headless). Paths are relative
# to the repository root; outputs go under out/ (not committed).
#   tools/melonds/mds.sh run   corvane/corvane.nds 600 out/mds/boot "60:A,200:A" 150,599
#   tools/melonds/mds.sh demos corvane/build/release/demos   # RESULT line per demo ROM
# Environment passed through: SAV POKE PEEK WAV RTC TRACE TRACE_EVERY SHOT_EVERY PM0
# RENDERER SCALE (see headless/mdsrun.cpp).
set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
IMAGE=nds-melonds:1
docker build -q -t "$IMAGE" "$ROOT/tools/melonds/headless" >/dev/null
RUN="docker run --rm -e SAV=${SAV:-} -e POKE=${POKE:-} -e PEEK=${PEEK:-} -e WAV=${WAV:-} -e RTC=${RTC:-} -e TRACE=${TRACE:-} -e TRACE_EVERY=${TRACE_EVERY:-} -e SHOT_EVERY=${SHOT_EVERY:-} -e PM0=${PM0:-} -e RENDERER=${RENDERER:-} -e SCALE=${SCALE:-} -v $ROOT:/work -w /work $IMAGE"

cmd=$1
shift
case "$cmd" in
  run)
    mkdir -p "$ROOT/$(dirname "$3")"
    $RUN "$@" 2>&1 | grep -E '^(RESULT|PEEK|SAVE|PC9|CPU9|REG9|TRACE|GL|no EGL|GLRenderer|ComputeRenderer)'
    ;;
  demos)
    dir=$1
    mkdir -p "$ROOT/out/mds/demos"
    for rom in "$ROOT/$dir"/*.nds; do
      n=$(basename "$rom" .nds)
      $RUN "$dir/$n.nds" 180 "out/mds/demos/$n" "30:T120/100:20,60:A:5" 120 2>&1 \
        | grep -E '^(RESULT|SAVE)' | sed "s/^/$n /"
    done
    ;;
  *)
    echo "usage: mds.sh run|demos ..." >&2
    exit 1
    ;;
esac
