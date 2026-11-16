# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
#
# Headless DeSmuME (py-desmume) for smoke tests. The wheel is x86_64 only,
# so on Apple Silicon this image runs under emulation (--platform linux/amd64).

FROM --platform=linux/amd64 python:3.11-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends libsdl2-2.0-0 libglib2.0-0 \
    libpcap0.8 libopenal1 libagg2 libsoundtouch1 zlib1g libgl1 libegl1 \
 && rm -rf /var/lib/apt/lists/*
RUN pip install --no-cache-dir py-desmume==0.0.9 pillow

ENV SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
WORKDIR /work
