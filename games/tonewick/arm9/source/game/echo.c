// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// District 3, the echo room: the ADPCM loop and the capture units.
// Scaffold: the district has no signal lines and no audit yet.

#include <nds.h>

#include "game/lab.h"
#include "game/rooms.h"

static void audit_echo(void)
{
}

const Room room_echo = {
    "THE ECHO ROOM", "LOWER TOWER", "DS SOUND CAPTURE", NULL, 0, audit_echo,
};
