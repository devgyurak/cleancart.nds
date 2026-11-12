// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// keypid <pid> <macVirtualKeyCode> <down|up>
// Posts one key event straight to a process (works without focus, e.g. with
// the screen locked). Needs Accessibility trust for the calling terminal.
// Delivery to a background app is unreliable: repeat and confirm with
// gdbread.py rather than trusting a single attempt.
// Build: swiftc tools/melonds/keypid.swift -o /tmp/keypid
// Key codes: A 0, S 1, D 2, Z 6, X 7, Q 12, W 13, Return 36, Delete 51,
// Left 123, Right 124, Down 125, Up 126.

import AppKit

let a = CommandLine.arguments
let pid = pid_t(Int32(a[1])!)
let code = CGKeyCode(Int(a[2])!)
let down = a[3] == "down"
let src = CGEventSource(stateID: .privateState)
let e = CGEvent(keyboardEventSource: src, virtualKey: code, keyDown: down)!
// Real arrow keys carry these flags; Qt turns them into KeypadModifier.
e.flags = (123...126).contains(Int(code)) ? [.maskNumericPad, .maskSecondaryFn] : []
e.postToPid(pid)
