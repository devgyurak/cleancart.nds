@ SPDX-License-Identifier: Apache-2.0
@ SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
@
@ Quayholt's ARM7 measurement stubs. Instructions GBATEK marks as strange (writeback with the base
@ in the list, empty lists, odd addresses) are written as .word / .hword encodings so the assembler
@ can neither refuse nor "fix" them. See stubs7.h for what each stub stores.
        .syntax unified
        .text
        .arm

@ ---- A1, A2: LDRH / LDRSH at an odd address -------------------------------------------------
        .global qy7_a12
        .type   qy7_a12, %function
qy7_a12:
        add     r2, r1, #1
        ldrh    r3, [r2]
        str     r3, [r0]
        ldrsh   r3, [r2]
        str     r3, [r0, #4]
        bx      lr

@ ---- A3: Thumb LDRH / LDRSH with an odd address ---------------------------------------------
        .global qy7_a3
        .type   qy7_a3, %function
qy7_a3:
        adr     r2, 1f
        add     r2, r2, #1
        bx      r2
        .thumb
        .thumb_func
1:      movs    r2, #1
        ldrh    r3, [r1, r2]
        str     r3, [r0]
        ldrsh   r3, [r1, r2]
        str     r3, [r0, #4]
        ldr     r2, =2f
        bx      r2
        .ltorg
        .arm
        .align  2
2:      bx      lr

@ ---- B1: STM with the base in the list -------------------------------------------------------
        .global qy7_b1
        .type   qy7_b1, %function
qy7_b1:
        push    {r4-r6, lr}
        mov     r6, r0
        mov     r4, r1
        mov     r5, #0
        .word   0xE8A40032              @ stmia r4!, {r1, r4, r5}: the base is not first
        ldr     r3, [r1, #4]            @ the word that was stored for r4
        str     r3, [r6]
        mov     r4, r1
        .word   0xE8A40030              @ stmia r4!, {r4, r5}: the base is first
        ldr     r3, [r1]
        str     r3, [r6, #4]
        pop     {r4-r6, lr}
        bx      lr

@ ---- B2: LDM with the base in the list -------------------------------------------------------
        .global qy7_b2
        .type   qy7_b2, %function
qy7_b2:
        push    {r4-r5, lr}
        mov     r3, r0
        mov     r4, r1
        .word   0xE8B40030              @ ldmia r4!, {r4, r5}
        str     r4, [r3]
        pop     {r4-r5, lr}
        bx      lr

@ ---- B4: empty register list (ARM, then Thumb) ----------------------------------------------
        .global qy7_b4
        .type   qy7_b4, %function
qy7_b4:
        push    {r4-r6, lr}
        mov     r6, r0
        mov     r4, r1
        .word   0xE8A40000              @ stmia r4!, {}
        str     r4, [r6]
        ldr     r3, [r1]
        str     r3, [r6, #4]
        mvn     r3, #0
        str     r3, [r1]                @ the Thumb run starts from a clean word again
        add     r2, r1, #0x100
        adr     r3, 1f
        add     r3, r3, #1
        bx      r3
        .thumb
        .thumb_func
1:      adds    r4, r2, #0
        .hword  0xC400                  @ stmia r4!, {}
        str     r4, [r6, #8]
        ldr     r3, [r2]
        str     r3, [r6, #12]
        ldr     r3, =2f
        bx      r3
        .ltorg
        .arm
        .align  2
2:      pop     {r4-r6, lr}
        bx      lr

@ ---- B5: Thumb STMIA with the base in the list -----------------------------------------------
        .global qy7_b5
        .type   qy7_b5, %function
qy7_b5:
        push    {r4-r7, lr}
        mov     r6, r0
        mov     r7, r1
        adr     r3, 1f
        add     r3, r3, #1
        bx      r3
        .thumb
        .thumb_func
1:      adds    r4, r7, #0
        movs    r5, #0
        .hword  0xC432                  @ stmia r4!, {r1, r4, r5}
        ldr     r3, [r7, #4]
        str     r3, [r6]
        adds    r4, r7, #0
        .hword  0xC430                  @ stmia r4!, {r4, r5}
        ldr     r3, [r7]
        str     r3, [r6, #4]
        ldr     r3, =2f
        bx      r3
        .ltorg
        .arm
        .align  2
2:      pop     {r4-r7, lr}
        bx      lr

@ ---- B6: STM with PC in the list --------------------------------------------------------------
        .global qy7_b6
        .type   qy7_b6, %function
qy7_b6:
        push    {r4-r5, lr}
        mov     r4, r1
        adr     r5, 1f
1:      .word   0xE8848001              @ stmia r4, {r0, pc}
        ldr     r3, [r1, #4]
        sub     r3, r3, r5
        str     r3, [r0]
        pop     {r4-r5, lr}
        bx      lr

@ ---- C1: ALU with Rn = PC and a register-specified shift -----------------------------------
        .global qy7_c1
        .type   qy7_c1, %function
qy7_c1:
        push    {r4, lr}
        mov     r4, r0
        mov     r1, #0
        mov     r2, #0
        adr     r3, 1f
1:      .word   0xE08F0211              @ add r0, pc, r1, lsl r2
        sub     r0, r0, r3
        str     r0, [r4]
        pop     {r4, lr}
        bx      lr

@ ---- prerequisite: plain LDM / STM, ARM and Thumb ------------------------------------------
        .global qy7_pre_ldm
        .type   qy7_pre_ldm, %function
qy7_pre_ldm:
        push    {r4-r7, lr}
        mov     r7, r0
        ldr     r2, =0x11111111
        ldr     r3, =0x22222222
        ldr     r4, =0x33333333
        stmia   r1, {r2-r4}
        mov     r5, #0
        mov     r6, #0
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

@ ---- the game's one-instruction runner ------------------------------------------------------------
        .global qy7_run1
        .type   qy7_run1, %function
qy7_run1:
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
        pop     {r4-r11, lr}
        bx      lr                      @ ARMv4T: POP {PC} does not interwork
