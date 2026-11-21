@ SPDX-License-Identifier: Apache-2.0
@ SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
@
@ Quayholt's ARM9 measurement stubs (see stubs9.h). Instructions GBATEK marks as strange are
@ written as .word / .hword encodings so the assembler cannot refuse or change them.
        .syntax unified
        .text
        .arm

@ ---- B3: LDM writeback with the base in the list ---------------------------------------------
        .global qy9_b3
        .type   qy9_b3, %function
qy9_b3:
        push    {r4-r6, lr}
        mov     r6, r0
        mov     r4, r1
        .word   0xE8B40030              @ ldmia r4!, {r4, r5}: the base is not last
        str     r4, [r6]
        mov     r5, r1
        .word   0xE8B50030              @ ldmia r5!, {r4, r5}: the base is last
        str     r5, [r6, #4]
        mov     r4, r1
        .word   0xE8B40010              @ ldmia r4!, {r4}: the base alone
        str     r4, [r6, #8]
        pop     {r4-r6, lr}
        bx      lr

@ ---- B7: empty register list, ARM then Thumb -------------------------------------------------
        .global qy9_b7
        .type   qy9_b7, %function
qy9_b7:
        push    {r4-r6, lr}
        mov     r6, r0
        mov     r4, r1
        .word   0xE8B40000              @ ldmia r4!, {}
        str     r4, [r6]
        add     r2, r1, #0x100
        adr     r3, 1f
        add     r3, r3, #1
        bx      r3
        .thumb
        .thumb_func
1:      adds    r4, r2, #0
        .hword  0xCC00                  @ ldmia r4!, {}
        str     r4, [r6, #4]
        ldr     r3, =2f
        bx      r3
        .ltorg
        .arm
        .align  2
2:      pop     {r4-r6, lr}
        bx      lr

@ ---- B6: STM with PC in the list --------------------------------------------------------------
        .global qy9_b6
        .type   qy9_b6, %function
qy9_b6:
        push    {r4-r5, lr}
        mov     r4, r1
        adr     r5, 1f
1:      .word   0xE8848001              @ stmia r4, {r0, pc}
        ldr     r3, [r1, #4]
        sub     r3, r3, r5
        str     r3, [r0]
        pop     {r4-r5, lr}
        bx      lr

@ ---- C2: QDADD doubles Rn with its own saturation -------------------------------------------
        .global qy9_c2
        .type   qy9_c2, %function
qy9_c2:
        msr     cpsr_f, #0
        ldr     r1, =0x80000000
        ldr     r2, =0x40000000
        qdadd   r3, r1, r2              @ r1 + sat(2 * r2): 80000000h + 7FFFFFFFh = FFFFFFFFh, Q set
        mrs     r12, cpsr
        str     r3, [r0]
        lsr     r12, r12, #27
        and     r12, r12, #1
        str     r12, [r0, #4]
        msr     cpsr_f, #0
        bx      lr
        .ltorg

@ ---- C3: SMLAxy sets Q without saturating; SMLALxy does not set Q ---------------------------
        .global qy9_c3
        .type   qy9_c3, %function
qy9_c3:
        push    {r4-r7, lr}
        msr     cpsr_f, #0
        ldr     r1, =0x00007FFF
        mov     r2, r1
        ldr     r3, =0x7FFFFFFF
        smlabb  r5, r1, r2, r3          @ 3FFF0001h + 7FFFFFFFh = BFFF0000h (wraps), Q set
        mrs     r12, cpsr
        lsr     r12, r12, #27
        and     r12, r12, #1
        str     r5, [r0]
        str     r12, [r0, #4]
        msr     cpsr_f, #0
        mvn     r6, #0                  @ RdLo = FFFFFFFFh
        ldr     r7, =0x7FFFFFFF         @ RdHi
        mov     r1, #1
        mov     r2, #1
        smlalbb r6, r7, r1, r2          @ 7FFFFFFFFFFFFFFFh + 1 wraps to 8000000000000000h, Q unchanged
        mrs     r12, cpsr
        lsr     r12, r12, #27
        and     r12, r12, #1
        str     r7, [r0, #8]
        str     r12, [r0, #12]
        msr     cpsr_f, #0
        pop     {r4-r7, lr}
        bx      lr
        .ltorg

@ ---- C4: CP15 control bit 15 keeps PC loads in ARM state ------------------------------------
@ qy_landing is both ARM and Thumb code: as ARM it runs mov/b into armpath (marker 1); entered as
@ Thumb at the same address (bit 0 set, the ARMv5 interworking load) its first halfword is B to
@ qy_thumb (marker 2). Both continue at 9:.
        .global qy9_c4
        .type   qy9_c4, %function
qy9_c4:
        push    {r4-r7, lr}
        mov     r4, r0
        mov     r7, #0
        adr     r6, 9f
        adr     r2, qy_cell
        mrc     p15, 0, r5, c1, c0, 0
        orr     r0, r5, #0x8000
        cmp     r1, #0
        biceq   r0, r5, #0x8000         @ flag 0: bit 15 cleared (the game's "habit off")
        mcr     p15, 0, r0, c1, c0, 0
        ldr     pc, [r2]
9:      mcr     p15, 0, r5, c1, c0, 0
        str     r7, [r4]
        pop     {r4-r7, lr}
        bx      lr

        .align  2
qy_cell:
        .word   qy_landing + 1
        .align  2
qy_landing:
        .word   0xE1A0E002              @ ARM: mov lr, r2.  Thumb: b qy_thumb (lo halfword E002h)
        b       qy_armpath              @ ARM only: the Thumb branch skips this word
        .thumb
qy_thumb:
        movs    r7, #2
        bx      r6
        .arm
        .align  2
qy_armpath:
        mov     r7, #1
        b       9b

@ ---- prerequisite: plain LDM / STM, ARM and Thumb ------------------------------------------
        .global qy9_pre_ldm
        .type   qy9_pre_ldm, %function
qy9_pre_ldm:
        push    {r4-r7, lr}
        mov     r7, r0
        ldr     r2, =0x11111111
        ldr     r3, =0x22222222
        ldr     r4, =0x33333333
        stmia   r1, {r2-r4}
        ldmia   r1, {r2-r4}
        ldr     r5, =0x11111111
        cmp     r2, r5
        ldreq   r5, =0x22222222
        cmpeq   r3, r5
        ldreq   r5, =0x33333333
        cmpeq   r4, r5
        moveq   r5, #1
        movne   r5, #0
        str     r5, [r7]
        adr     r3, 1f
        add     r3, r3, #1
        bx      r3
        .thumb
        .thumb_func
1:      ldr     r2, =0xA1A2A3A4
        ldr     r3, =0xB1B2B3B4
        stmia   r1!, {r2, r3}
        subs    r1, #8
        ldmia   r1!, {r4, r5}
        cmp     r4, r2
        bne     3f
        cmp     r5, r3
        bne     3f
        movs    r6, #1
        b       4f
3:      movs    r6, #0
4:      str     r6, [r7, #4]
        ldr     r3, =2f
        bx      r3
        .ltorg
        .arm
        .align  2
2:      pop     {r4-r7, lr}
        bx      lr

@ ---- the game's one-instruction runner (same as the ARM7's, arm7/source/stubs7.s) -----------------
        .global qy9_run1
        .type   qy9_run1, %function
qy9_run1:
        push    {r4-r11, lr}
        mov     r8, r0
        mov     r9, r1
        mov     r10, r2
        msr     cpsr_f, #0
        ldmia   r8, {r0-r7}
        adr     lr, 1f
        bx      r10
1:      stmia   r9, {r0-r7}
        mrs     r0, cpsr
        str     r0, [r9, #32]
        pop     {r4-r11, pc}
