# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
#
# Headless melonDS 1.1 core (GPL-3.0, downloaded at build time, never part of a ROM)
# plus our runner mdsrun.cpp. Native arch; no JIT. Software renderer by default;
# RENDERER=gl|compute runs the GL renderers on Mesa llvmpipe (surfaceless EGL).

FROM debian:bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates curl cmake make g++ \
    libegl-dev libegl1 libegl-mesa0 libgl1-mesa-dri \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
RUN curl -sSL -o melonDS.tar.gz https://github.com/melonDS-emu/melonDS/archive/refs/tags/1.1.tar.gz \
 && echo "61e339bcb18a68a17485973637d972ea628c5624d7e6b8adf6870f895d5e26fd  melonDS.tar.gz" | sha256sum -c - \
 && tar xzf melonDS.tar.gz && mv melonDS-1.1 melonDS && rm melonDS.tar.gz
COPY CMakeLists.txt mdsrun.cpp ./
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null \
 && cmake --build build -j"$(nproc)" --target mdsrun \
 && cp build/mdsrun /usr/local/bin/ && rm -rf build

WORKDIR /work
ENTRYPOINT ["mdsrun"]
