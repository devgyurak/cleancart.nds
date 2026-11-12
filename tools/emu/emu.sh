#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
#
# Headless DeSmuME runner. Paths are relative to the repository root.
#   tools/emu/emu.sh run   games/corvane/corvane.nds 600 out/boot "60:A,200:START" 150,599
#   tools/emu/emu.sh demos games/corvane        # every corvane-*.nds, prints RESULT lines
#   tools/emu/emu.sh montage out/m.png out/boot_00150.png out/boot_00599.png
# Output files land under the repository (e.g. out/); keep them out of git.
set -eu

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
IMAGE=nds-emu:1
docker build -q --platform linux/amd64 -t "$IMAGE" "$ROOT/tools/emu" >/dev/null
# Words of the result block to print: lightenette's block is 64 words, the other
# games' blocks are 16, so the default is 64 only for lightenette ROMs.
words_for() {
  case "$1" in
    lightenette|*/lightenette|*/lightenette/*) echo "${RESULT_WORDS:-64}" ;;
    *) echo "${RESULT_WORDS:-8}" ;;
  esac
}
RW=8
RUN() {
  docker run --rm --platform linux/amd64 -e SAVETYPE=${SAVETYPE:-3} -e POKE=${POKE:-} \
    -e RESULT_WORDS="$RW" -v "$ROOT":/work -w /work "$IMAGE" "$@"
}

cmd=$1
shift
case "$cmd" in
  run)
    mkdir -p "$ROOT/$(dirname "$3")"
    RW=$(words_for "$1")
    RUN python tools/emu/run.py "$@" 2>&1 | grep -E '^(RESULT|PC9)'
    ;;
  demos)
    game=$1                      # game folder relative to the repo root, e.g. games/corvane
    name=$(basename "$game")
    RW=$(words_for "$game")
    mkdir -p "$ROOT/out/demos"
    for rom in "$ROOT/$game"/"$name"-*.nds; do
      n=$(basename "$rom" .nds)
      case "$n" in
        *io-save-flash) st=5 ;;
        *io-save-fram)  st=4 ;;
        *)              st=3 ;;
      esac
      docker run --rm --platform linux/amd64 -e SAVETYPE=$st -e RESULT_WORDS="$RW" -v "$ROOT":/work -w /work "$IMAGE" \
        python tools/emu/run.py "$game/$n.nds" 180 "out/demos/$n" "30:T120/100:20,60:A:5" 2>&1 \
        | grep '^RESULT' | sed "s/^/$n /"
    done
    ;;
  montage)
    RUN python tools/emu/montage.py "$@"
    ;;
  *)
    echo "usage: emu.sh run|demos|montage ..." >&2
    exit 1
    ;;
esac
