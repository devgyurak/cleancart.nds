// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Puzzle data (the cv_puzzles blob, tools/evpuzzle/pack.py) and the player's pile:
// silhouettes, the judge and the star rating. Pure logic, no hardware access.

#ifndef EV_PUZZLE_H
#define EV_PUZZLE_H

#include <nds.h>
#include <stdbool.h>

#define PZ_MAX_DIM   8
#define PZ_MAX_PARTS 3
#define PZ_NAME_MAX  24

typedef struct {
    u8 w, d, h;          // grid
    u8 par;              // fewest blocks that deliver the order
    const u8 *front;     // w bytes, bit z of byte x
    const u8 *plan;      // d bytes, bit x of byte y
    u8 nsol;             // cells of a minimal solution
    const u8 *sol;       // nsol x { x, y, z }
} PzPart;

typedef struct {
    u8 chapter, index;   // index is 1-based within the chapter
    u8 mode;             // 0 stack (piles), 1 layers
    u8 nparts;
    bool reference;      // par is the maker's count, not proven minimal
    char name[PZ_NAME_MAX + 1];
    PzPart part[PZ_MAX_PARTS];
} PzPuzzle;

// The player's structure for one part: bit z of cell[y][x] is a block (layers
// mode); stack mode keeps the same data, filled from the floor up.
typedef struct {
    u8 cell[PZ_MAX_DIM][PZ_MAX_DIM];
} PzGrid;

int  pz_count(void);
bool pz_get(int n, PzPuzzle *out);                 // n counts from 0, in file order
int  pz_find(int chapter, int index, PzPuzzle *out);   // puzzle number or -1
int  pz_chapter_size(int chapter);

// Piles (stack mode)
int  pz_height(const PzGrid *g, int x, int y);     // blocks in the column
void pz_set_height(PzGrid *g, int x, int y, int h);
// Silhouettes of what the player built
void pz_views(const PzPart *p, const PzGrid *g, u8 *front, u8 *plan);
// 0 right, 1 extra block, 2 missing block
int  pz_front_cell(const PzPart *p, const PzGrid *g, int x, int z);
int  pz_plan_cell(const PzPart *p, const PzGrid *g, int x, int y);
bool pz_matches(const PzPart *p, const PzGrid *g);   // both views right
int  pz_blocks(const PzGrid *g, const PzPart *p);
// Face-adjacent support: every block reaches z = 0 (always true for piles)
bool pz_supported(const PzPart *p, const PzGrid *g);
// Cells that do not reach the floor, as a grid of the same shape (bit z set = floating).
void pz_floating(const PzPart *p, const PzGrid *g, PzGrid *out);
int  pz_top(const PzGrid *g, int x, int y);   // 1 + highest block, 0 for an empty column
int  pz_stars(int blocks, int par);                // 3, 2 or 1

#endif // EV_PUZZLE_H
