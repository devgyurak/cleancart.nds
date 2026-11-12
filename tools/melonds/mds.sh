#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
#
# Headless melonDS 1.1 core runner (tools/melonds/headless). Paths are relative
# to the repository root; outputs go under out/ (not committed).
#   tools/melonds/mds.sh run   games/corvane/corvane.nds 600 out/mds/boot "60:A,200:A" 150,599
#   tools/melonds/mds.sh demos games/corvane/build/release/demos   # RESULT line per demo ROM
# Environment passed through: SAV POKE PEEK WAV RTC TRACE TRACE_EVERY SHOT_EVERY PM0
# RENDERER SCALE SDDIR (see headless/mdsrun.cpp). MELONDS_COMMIT=<sha> runs a build
# of that melonDS commit instead of the 1.1 release.
# SDDIR=out/mds/x/sd attaches a DLDI SD card that mirrors that directory (the
# ROM runs as homebrew then, with no save chip); files the game writes appear
# there when the run ends.
set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
IMAGE=nds-melonds:1
if [ -n "${MELONDS_COMMIT:-}" ]; then
  IMAGE=nds-melonds:$MELONDS_COMMIT
fi
docker build -q --build-arg MELONDS_COMMIT="${MELONDS_COMMIT:-}" -t "$IMAGE" "$ROOT/tools/melonds/headless" >/dev/null
RUN="docker run --rm -e SAV=${SAV:-} -e POKE=${POKE:-} -e PEEK=${PEEK:-} -e WAV=${WAV:-} -e RTC=${RTC:-} -e TRACE=${TRACE:-} -e TRACE_EVERY=${TRACE_EVERY:-} -e SHOT_EVERY=${SHOT_EVERY:-} -e PM0=${PM0:-} -e RENDERER=${RENDERER:-} -e SCALE=${SCALE:-} -e SDDIR=${SDDIR:-} -v $ROOT:/work -w /work $IMAGE"

# Reject bad settings here too: the runner's error would be filtered out below.
case "${RENDERER:-soft}" in
  soft|gl|compute) ;;
  *) echo "ERROR unknown RENDERER '$RENDERER' (soft, gl or compute)" >&2; exit 2 ;;
esac
case "${SCALE:-1}" in
  [1-9]|1[0-6]) ;;
  *) echo "ERROR SCALE must be a number from 1 to 16, got '$SCALE'" >&2; exit 2 ;;
esac

cmd=$1
shift
case "$cmd" in
  run)
    mkdir -p "$ROOT/$(dirname "$3")"
    $RUN "$@" 2>&1 | grep -E '^(RESULT|PEEK|SAVE|PC9|CPU9|REG9|TRACE|GL|no EGL|GLRenderer|ComputeRenderer|ERROR)'
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
