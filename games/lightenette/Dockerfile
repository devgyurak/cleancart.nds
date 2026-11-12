# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
#
# Reproducible build environment for Lightenette.
# The BlocksDS image is pinned by digest so every build uses the same
# toolchain, libnds and crts. Python 3 runs the build-time asset tools.

FROM skylyrac/blocksds@sha256:c5f19b95d9c2f312e6a68ea244d2ed5120203b4b82af9785937a349096f8e5e4

RUN apt-get update \
 && apt-get install -y --no-install-recommends python3 \
 && rm -rf /var/lib/apt/lists/*

ENV BLOCKSDS=/opt/wonderful/thirdparty/blocksds/core \
    WONDERFUL_TOOLCHAIN=/opt/wonderful \
    PYTHONHASHSEED=0 \
    SOURCE_DATE_EPOCH=0

WORKDIR /work
