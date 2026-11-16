// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// District 4, the junction: the capture bugs and the morning log.
// Scaffold: the district has no signal lines and no audit yet.

#include <nds.h>

#include "game/lab.h"
#include "game/rooms.h"

static void audit_junction(void)
{
}

const Room room_junction = {
    "THE JUNCTION", "LOWER TOWER", "DS SOUND CAPTURE + I/O", NULL, 0, audit_junction,
};
