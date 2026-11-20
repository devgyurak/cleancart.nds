// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The harbour game: six piers of five jobs (tools/qyjobs). A job names a device, the foreman's wanted chalk word and
// the cranes that may work it; the player picks the crane and the setup and lifts. The ROM's own GBATEK model
// (qy_model in the generated assets) decides whether the chalk word is right, so every job can be finished on any
// emulator; the real console's answer is shown beside it and a difference marks the box "crooked".

#ifndef QY_HARBOUR_H
#define QY_HARBOUR_H

#include <nds.h>

#include "assets.h"

extern u8 qy_job_done[QY_NUM_JOBS];
extern u8 qy_job_crooked[QY_NUM_JOBS];
extern int qy_pier;     // selected pier and job (the job's index in qy_jobs)
extern int qy_job;
extern int qy_crane;    // the job scene's crane (0 old, 1 new) and setup
extern int qy_setup;
extern int qy_job_state;   // 0 choosing, 1 lifting, 2 balanced

int jobs_cleared(void);
// Runs the first job of every pier by the model's own solution (the rehearsal), recording the hardware's answers.
void jobs_script_firsts(void);
// A hash of the job table (the plan) for the result block.
u32 jobs_plan_hash(void);
// The key the QA pilot presses next in the pier / job scenes; 0 for none.
u16 harbour_pilot_key(int scene);

#endif // QY_HARBOUR_H
