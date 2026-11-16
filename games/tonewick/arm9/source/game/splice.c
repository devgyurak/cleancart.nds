// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// District 2, the splice: channel edges: a short tape, the hold latch, the noise shift register.
// Scaffold: the district has no signal lines and no audit yet.

#include <nds.h>

#include "game/lab.h"
#include "game/rooms.h"

static void audit_splice(void)
{
}

const Room room_splice = {
    "THE SPLICE", "UPPER TOWER", "DS SOUND CHANNELS", NULL, 0, audit_splice,
};
