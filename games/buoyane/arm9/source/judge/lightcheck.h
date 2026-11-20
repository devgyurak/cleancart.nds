// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The light check: five still scenes (dot, fog, edge, depth, blend), each drawn with pixel-exact
// projections, captured from the 3D layer (or, for the alpha paths, from the blended screen) into
// bank D and read back at fixed pixels. Every judged item is one bit or word of the result block;
// the bit layout is in game/rehearsal.h. A scene that did not finish all its captures leaves its
// bit in LC_CAPS clear and its other words zero, so a stale picture never passes.
//
// Colours are RGB555 values that survive the 3D engine's 6-bit path; samples lie at least 3
// pixels inside polygons, except where the item is about an edge or a 1-pixel polygon.

#ifndef BY_LIGHTCHECK_H
#define BY_LIGHTCHECK_H

#include <nds.h>

enum {
    LC_DOT, LC_FOG, LC_EDGE, LC_DEPTH, LC_BLEND,
    LC_SCENES
};

// Result block words (values[n]) written by the scenes; see game/rehearsal.h.
enum {
    W_CAPS = 0,        // bit n: scene n's captures all finished
    W_DOT = 1,
    W_DOT_COLOURS = 2,
    W_FOG = 3,
    W_FOG_NEAR_REF = 4,
    W_FOG_127_126 = 5,
    W_EDGE = 6,
    W_EDGE_A = 7,
    W_EDGE_B = 8,
    W_DEPTH = 9,
    W_BAND_L = 10,
    W_BAND_R = 11,
    W_ZEQ_3 = 12,
    W_ZEQ_1 = 13,
    W_AA_LINES = 14,
    W_BLEND = 15,
    W_HI_DECAL = 16,
    W_WIRE_BYPASS = 17,
    W_HOLES = 18,
    W_WORDS = 20
};

// Takes over both engines, draws and captures one scene, ORs its words into v[] and sets its bit in
// v[W_CAPS]. The 3D engine is left idle.
void lightcheck_scene(int scene, u32 *v);

// All five scenes in order.
void lightcheck_all(u32 *v);

const char *lightcheck_scene_name(int scene);

#endif // BY_LIGHTCHECK_H
