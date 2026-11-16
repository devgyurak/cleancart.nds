// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The kiln check: a fixed command sequence is sent to the geometry engine (direct ports and
// packed GXFIFO), then GXSTAT and the matrix result registers are read after the engine
// has stopped, and compared with an integer model of GBATEK's matrix and stack rules.
// The matrices only hold values that are exact in 1.19.12 (0, +-0.5, +-1, +-2, whole
// translations, quarter turns), so the answer does not depend on how an implementation
// rounds a product. The check does not draw anything and does not touch VRAM.

#ifndef PV_BENCH_H
#define PV_BENCH_H

#include <nds.h>
#include <stdbool.h>

#define KILN_ITEMS 13

enum {
    KILN_PUSH_LEVEL,        // stack level after three pushes (3)
    KILN_POP_RESTORES,      // words of the position matrix that differ after a pop (0)
    KILN_POP_N_LEVEL,       // stack level after pop with N = 2 from level 3 (1)
    KILN_POP_N_MATRIX,      // words that differ after that pop (0)
    KILN_STORE_LEVEL,       // stack level after a store at level 2 (2)
    KILN_RESTORE_MATRIX,    // words that differ after restoring the stored matrix (0)
    KILN_PROJ_PUSH_LEVEL,   // projection stack level after a push (1)
    KILN_PROJ_POP_LEVEL,    // projection stack level after the pop (0)
    KILN_PROJ_POP_MATRIX,   // words of the projection matrix that differ after the pop (0)
    KILN_LOAD_MULT_CLIP,    // of the 16 CLIPMTX_RESULT words, those that differ from the model (0)
    KILN_VEC_MULT,          // of the 9 VECMTX_RESULT words, those that differ from the model (0)
    KILN_PACKED_FIFO,       // wrong words (25) after the mixed sequence through the packed GXFIFO (0)
    KILN_DIRECT_PORTS,      // the same through the direct command ports (0)
};

// Runs every check; r[i] is the value of item i. Leaves the stacks empty, both modelview and
// projection matrices at the identity and the matrix mode at modelview.
// Returns false when a GXSTAT wait ran out (the values are then not to be trusted).
bool kiln_run(u32 r[KILN_ITEMS]);

// True when every value is the expected one (the lamp of the game).
bool kiln_ok(const u32 r[KILN_ITEMS]);

#endif // PV_BENCH_H
