// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The rhythm piece that ends a chapter: eight notes fall to a line and A is tapped on the beat. Scoring is by VBlank frames,
// so no judged item can change it. The pilot of the QA run taps on the exact frame.

#ifndef PD_RHYTHM_H
#define PD_RHYTHM_H

#define RHYTHM_NOTES 8

int rhythm_note_frame(int i);   // scene frame of note i (hit line)

#endif // PD_RHYTHM_H
