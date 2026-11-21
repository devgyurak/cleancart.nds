# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Quayholt's job model: the devices (ops), the GBATEK model of every device and the 30 jobs.

The ROM does not compute the model: `pack.py` writes the model table and the jobs into the generated assets
(`qy_ops`, `qy_jobs`, `qy_model`) and the game looks the answers up. `check.py` proves that every job can be
solved with the model alone (so the game ends on any emulator, whatever the hardware answers)."""
