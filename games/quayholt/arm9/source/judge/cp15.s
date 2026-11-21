@ SPDX-License-Identifier: Apache-2.0
@ SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
@
@ CP15 access, the exception harness and the TCM stubs of Quayholt's ARM9 measurements (see cp15.h).
@ GBATEK "ARM CP15 Control Register", "ARM CP15 Protection Unit (PU)", "ARM CP15 Tightly Coupled Memory (TCM)",
@ "ARM CPU Exceptions", "DS DMA Transfers".
        .syntax unified
        .text
        .arm

@ ---- state save / restore --------------------------------------------------------------------------
        .global qy9_cp15_get
        .type   qy9_cp15_get, %function
qy9_cp15_get:
        mrc     p15, 0, r1, c1, c0, 0
        str     r1, [r0, #0]
        mrc     p15, 0, r1, c5, c0, 2
        str     r1, [r0, #4]
        mrc     p15, 0, r1, c5, c0, 3
        str     r1, [r0, #8]
        mrc     p15, 0, r1, c6, c0, 0
        str     r1, [r0, #12]
        mrc     p15, 0, r1, c6, c1, 0
        str     r1, [r0, #16]
        mrc     p15, 0, r1, c6, c2, 0
        str     r1, [r0, #20]
        mrc     p15, 0, r1, c6, c3, 0
        str     r1, [r0, #24]
        mrc     p15, 0, r1, c6, c4, 0
        str     r1, [r0, #28]
        mrc     p15, 0, r1, c6, c5, 0
        str     r1, [r0, #32]
        mrc     p15, 0, r1, c6, c6, 0
        str     r1, [r0, #36]
        mrc     p15, 0, r1, c6, c7, 0
        str     r1, [r0, #40]
        mrc     p15, 0, r1, c2, c0, 0
        str     r1, [r0, #44]
        mrc     p15, 0, r1, c2, c0, 1
        str     r1, [r0, #48]
        mrc     p15, 0, r1, c3, c0, 0
        str     r1, [r0, #52]
        mrc     p15, 0, r1, c9, c1, 0
        str     r1, [r0, #56]
        mrc     p15, 0, r1, c9, c1, 1
        str     r1, [r0, #60]
        bx      lr

        .global qy9_cp15_set
        .type   qy9_cp15_set, %function
qy9_cp15_set:
        ldr     r1, [r0, #4]
        mcr     p15, 0, r1, c5, c0, 2
        ldr     r1, [r0, #8]
        mcr     p15, 0, r1, c5, c0, 3
        ldr     r1, [r0, #12]
        mcr     p15, 0, r1, c6, c0, 0
        ldr     r1, [r0, #16]
        mcr     p15, 0, r1, c6, c1, 0
        ldr     r1, [r0, #20]
        mcr     p15, 0, r1, c6, c2, 0
        ldr     r1, [r0, #24]
        mcr     p15, 0, r1, c6, c3, 0
        ldr     r1, [r0, #28]
        mcr     p15, 0, r1, c6, c4, 0
        ldr     r1, [r0, #32]
        mcr     p15, 0, r1, c6, c5, 0
        ldr     r1, [r0, #36]
        mcr     p15, 0, r1, c6, c6, 0
        ldr     r1, [r0, #40]
        mcr     p15, 0, r1, c6, c7, 0
        ldr     r1, [r0, #44]
        mcr     p15, 0, r1, c2, c0, 0
        ldr     r1, [r0, #48]
        mcr     p15, 0, r1, c2, c0, 1
        ldr     r1, [r0, #52]
        mcr     p15, 0, r1, c3, c0, 0
        ldr     r1, [r0, #56]
        mcr     p15, 0, r1, c9, c1, 0
        ldr     r1, [r0, #60]
        mcr     p15, 0, r1, c9, c1, 1
        ldr     r1, [r0, #0]
        mcr     p15, 0, r1, c1, c0, 0
        bx      lr

        .global qy9_set_ctrl
        .type   qy9_set_ctrl, %function
qy9_set_ctrl:
        mcr     p15, 0, r0, c1, c0, 0
        bx      lr
        .global qy9_get_ctrl
        .type   qy9_get_ctrl, %function
qy9_get_ctrl:
        mrc     p15, 0, r0, c1, c0, 0
        bx      lr
        .global qy9_set_dperm
        .type   qy9_set_dperm, %function
qy9_set_dperm:
        mcr     p15, 0, r0, c5, c0, 2
        bx      lr
        .global qy9_set_iperm
        .type   qy9_set_iperm, %function
qy9_set_iperm:
        mcr     p15, 0, r0, c5, c0, 3
        bx      lr
        .global qy9_set_region4
        .type   qy9_set_region4, %function
qy9_set_region4:
        mcr     p15, 0, r0, c6, c4, 0
        bx      lr
        .global qy9_set_region7
        .type   qy9_set_region7, %function
qy9_set_region7:
        mcr     p15, 0, r0, c6, c7, 0
        bx      lr
        .global qy9_get_cpsr
        .type   qy9_get_cpsr, %function
qy9_get_cpsr:
        mrs     r0, cpsr
        bx      lr
        .global qy9_set_cpsr_c
        .type   qy9_set_cpsr_c, %function
qy9_set_cpsr_c:
        msr     cpsr_c, r0
        bx      lr
        .global qy9_set_dtcm
        .type   qy9_set_dtcm, %function
qy9_set_dtcm:
        mcr     p15, 0, r0, c9, c1, 0
        bx      lr
        .global qy9_set_itcm
        .type   qy9_set_itcm, %function
qy9_set_itcm:
        mcr     p15, 0, r0, c9, c1, 1
        bx      lr

@ ---- cache maintenance of our own: the first 64 bytes of the ITCM hold libnds' cache functions, which the
@ exception table overwrites while a test runs, so nothing in a test window may call them.
        .global qy9_dc_flush_all
        .type   qy9_dc_flush_all, %function
qy9_dc_flush_all:
        mov     r1, #0                  @ way 0..3 in bits 31:30
1:      mov     r2, #0                  @ set 0..31 in bits 9:5
2:      orr     r3, r1, r2
        mcr     p15, 0, r3, c7, c14, 2  @ clean and invalidate data cache line by set/way
        add     r2, r2, #0x20
        cmp     r2, #0x400
        bne     2b
        add     r1, r1, #0x40000000
        cmp     r1, #0
        bne     1b
        mov     r0, #0
        mcr     p15, 0, r0, c7, c10, 4  @ drain write buffer
        bx      lr

        .global qy9_dc_flush_range
        .type   qy9_dc_flush_range, %function
qy9_dc_flush_range:
        add     r1, r1, r0
        bic     r0, r0, #0x1F
1:      mcr     p15, 0, r0, c7, c14, 1  @ clean and invalidate data cache line by address
        add     r0, r0, #0x20
        cmp     r0, r1
        blo     1b
        mov     r0, #0
        mcr     p15, 0, r0, c7, c10, 4
        bx      lr

        .global qy9_dc_clean_range
        .type   qy9_dc_clean_range, %function
qy9_dc_clean_range:
        add     r1, r1, r0
        bic     r0, r0, #0x1F
1:      mcr     p15, 0, r0, c7, c10, 1  @ clean data cache line by address (the line stays valid)
        add     r0, r0, #0x20
        cmp     r0, r1
        blo     1b
        mov     r0, #0
        mcr     p15, 0, r0, c7, c10, 4
        bx      lr

        .global qy9_ic_inv_all
        .type   qy9_ic_inv_all, %function
qy9_ic_inv_all:
        mov     r0, #0
        mcr     p15, 0, r0, c7, c5, 0
        bx      lr

@ ---- exception vectors and handlers -----------------------------------------------------------------
@ The handler records what it found, switches to system mode with IRQ and FIQ masked and jumps to
@ qy_exc_resume. It uses only R12 and the banked LR (rewritten on every exception entry): the banked SPs
@ of the exception modes are never touched, so the BIOS's own frames on them stay where they are.
        .macro QYH id
        ldr     r12, =qy_exc_rec
        str     lr, [r12, #4]
        mrs     lr, spsr
        str     lr, [r12, #8]
        mrs     lr, cpsr
        str     lr, [r12, #12]
        mov     lr, #\id
        str     lr, [r12]
        ldr     lr, =qy_exc_resume
        ldr     r12, [lr]
        msr     cpsr_c, #0xDF
        bx      r12
        .ltorg
        .endm

        .align  2
qy_h_undef:   QYH 1
qy_h_swi:     QYH 2
qy_h_pabt:    QYH 3
qy_h_dabt:    QYH 4
qy_h_other:   QYH 5

        .align  2
        .global qy9_exc_table
        .type   qy9_exc_table, %object
qy9_exc_table:
        .rept   8
        .word   0xE59FF018              @ ldr pc, [pc, #0x18]
        .endr
        .word   qy_h_other              @ 00h reset
        .word   qy_h_undef              @ 04h undefined
        .word   qy_h_swi                @ 08h SWI
        .word   qy_h_pabt               @ 0Ch prefetch abort
        .word   qy_h_dabt               @ 10h data abort
        .word   qy_h_other              @ 14h reserved
        .word   qy_h_other              @ 18h IRQ
        .word   qy_h_other              @ 1Ch FIQ

@ ---- exception triggers ------------------------------------------------------------------------------
        .macro QYPRE
        push    {r4-r5, lr}
        ldr     r1, =qy_exc_resume
        adr     r2, 2f
        str     r2, [r1]
        ldr     r1, =qy_exc_rec
        mov     r2, #0
        str     r2, [r1]
        .endm

        .global qy9_do_swi
        .type   qy9_do_swi, %function
qy9_do_swi:
        QYPRE
        adr     r0, 1f
1:      swi     0x42
2:      pop     {r4-r5, pc}
        .ltorg

        .global qy9_do_undef
        .type   qy9_do_undef, %function
qy9_do_undef:
        QYPRE
        adr     r0, 1f
1:      .word   0xE7F000F0              @ permanently undefined
2:      pop     {r4-r5, pc}
        .ltorg

        .global qy9_do_bkpt
        .type   qy9_do_bkpt, %function
qy9_do_bkpt:
        QYPRE
        adr     r0, 1f
1:      .word   0xE1200070              @ bkpt #0
2:      pop     {r4-r5, pc}
        .ltorg

        .global qy9_do_load
        .type   qy9_do_load, %function
qy9_do_load:
        push    {r4-r5, lr}
        mov     r3, r0
        mov     r4, r1
        ldr     r1, =qy_exc_resume
        adr     r2, 2f
        str     r2, [r1]
        ldr     r1, =qy_exc_rec
        mov     r2, #0
        str     r2, [r1]
        adr     r0, 1f
        ldr     r2, =0xDEADBEEF
1:      ldr     r2, [r3]
2:      str     r2, [r4]
        pop     {r4-r5, pc}
        .ltorg

        .global qy9_do_store
        .type   qy9_do_store, %function
qy9_do_store:
        push    {r4-r5, lr}
        mov     r3, r0
        mov     r4, r1
        ldr     r1, =qy_exc_resume
        adr     r2, 2f
        str     r2, [r1]
        ldr     r1, =qy_exc_rec
        mov     r2, #0
        str     r2, [r1]
        adr     r0, 1f
1:      str     r4, [r3]
2:      pop     {r4-r5, pc}
        .ltorg

        .global qy9_do_user_rw
        .type   qy9_do_user_rw, %function
qy9_do_user_rw:
        push    {r4-r6, lr}
        mov     r3, r0
        mov     r4, r1
        ldr     r1, =qy_exc_resume
        adr     r2, 2f
        str     r2, [r1]
        ldr     r1, =qy_exc_rec
        mov     r2, #0
        str     r2, [r1]
        ldr     r5, =0x5A5A5A5A
        ldr     r6, =0xDEADBEEF
        str     r6, [r4]
        adr     r0, 1f
        msr     cpsr_c, #0xD0           @ user mode, IRQ and FIQ masked
        ldr     r2, [r3]                @ read: allowed with AP 2
        str     r2, [r4]
1:      str     r5, [r3]                @ write: aborts with AP 2
        swi     0x43                    @ no abort: leave user mode through the SWI handler
2:      pop     {r4-r6, pc}
        .ltorg

        .global qy9_do_call
        .type   qy9_do_call, %function
qy9_do_call:
        push    {r4-r5, lr}
        mov     r3, r0
        ldr     r1, =qy_exc_resume
        adr     r2, 2f
        str     r2, [r1]
        ldr     r1, =qy_exc_rec
        mov     r2, #0
        str     r2, [r1]
1:      blx     r3
2:      mov     r0, r3
        pop     {r4-r5, pc}
        .ltorg

@ ---- E1: ITCM wins over an overlapping DTCM -------------------------------------------------------
@ DTCM is moved to base 01000000h (the ITCM's mirror, where the protection regions allow access) with a virtual
@ size of 32 KB over it; a word written at 01005000h lands in the ITCM (GBATEK) or in the DTCM's physical
@ word at offset 1000h. Both words are put back. No stack
@ access while DTCM is away from the stack.
        .global qy9_tcm_e1
        .type   qy9_tcm_e1, %function
qy9_tcm_e1:
        push    {r4-r11, lr}
        mov     r6, r0
        mrc     p15, 0, r4, c9, c1, 0
        mov     r8, r4, lsr #12
        mov     r8, r8, lsl #12
        add     r8, r8, #0x1000
        ldr     r9, [r8]                @ DTCM word at offset 1000h
        ldr     r10, =0x01005000
        ldr     r11, [r10]              @ ITCM word at 01005000h
        ldr     r1, =0xB1B2B3B4
        mov     r0, #0x0C
        orr     r0, r0, #0x01000000     @ base 01000000h, size 512 << 6 = 32 KB
        mcr     p15, 0, r0, c9, c1, 0
        str     r1, [r10]
        mcr     p15, 0, r4, c9, c1, 0
        ldr     r0, [r10]
        ldr     r2, [r8]
        str     r11, [r10]
        str     r9, [r8]
        str     r0, [r6]
        str     r2, [r6, #4]
        pop     {r4-r11, pc}
        .ltorg

@ ---- E2: a 32 KB virtual DTCM mirrors its 16 KB ----------------------------------------------------
@ The virtual DTCM must be aligned to its size: it is put at the 32 KB boundary below the normal base. A word
@ written in its lower half is read back in the upper half (+4000h). The stack keeps working: its address
@ maps to the same physical word in the upper half.
        .global qy9_tcm_e2
        .type   qy9_tcm_e2, %function
qy9_tcm_e2:
        push    {r4-r11, lr}
        mov     r6, r0
        mrc     p15, 0, r4, c9, c1, 0
        mov     r1, r4, lsr #15
        mov     r1, r1, lsl #15
        add     r2, r1, #0x1000
        add     r3, r1, #0x5000
        orr     r5, r1, #0x0C           @ size 512 << 6 = 32 KB
        mcr     p15, 0, r5, c9, c1, 0
        ldr     r8, [r2]
        ldr     r10, =0xA5C3E10F
        str     r10, [r2]
        ldr     r11, [r3]               @ the mirror: +4000h
        str     r8, [r2]
        mcr     p15, 0, r4, c9, c1, 0
        str     r11, [r6]
        pop     {r4-r11, pc}
        .ltorg

@ ---- E4: a DMA read of a DTCM address returns the main RAM behind it ------------------------------
@ r1 = M (a main RAM mirror address, 16 KB aligned, where the DTCM is put), r2 = U (the same RAM through a
@ different, uncached mirror), r3 = destination of the DMA. P1 is written at U + 100h, P2 into the DTCM at
@ M + 100h; the word DMA channel 3 copies from M + 100h is in r3 afterwards. Alternate stack while the DTCM is away.
        .global qy9_tcm_e4
        .type   qy9_tcm_e4, %function
qy9_tcm_e4:
        push    {r4-r11, lr}
        mov     r6, r0
        mov     r7, r3
        mov     r8, r1
        mrc     p15, 0, r4, c9, c1, 0
        ldr     r9, =0x4D41494E         @ P1: in the main RAM
        str     r9, [r2, #0x100]
        add     r0, r2, #0x100
        mcr     p15, 0, r0, c7, c14, 1  @ no stale cache line, then drain the write buffer
        mov     r0, #0
        mcr     p15, 0, r0, c7, c10, 4
        ldr     r10, =0xD7D70001        @ P2: in the DTCM
        mov     r11, sp
        ldr     sp, =qy_alt_stack_top
        orr     r0, r1, #0x0A           @ DTCM at M, 16 KB
        mcr     p15, 0, r0, c9, c1, 0
        ldr     r5, [r8, #0x100]        @ the DTCM word that is overwritten, put back below
        str     r10, [r8, #0x100]
        ldr     r0, =0x040000D4         @ DMA3SAD
        mov     r1, #0
        strh    r1, [r0, #10]           @ channel 3 idle first
        add     r1, r8, #0x100
        str     r1, [r0]
        str     r7, [r0, #4]            @ DMA3DAD
        ldr     r1, =0x84000001         @ enable, 32-bit, 1 word, immediate
        str     r1, [r0, #8]
        ldr     r12, =0x100000          @ bounded wait
3:      ldr     r1, [r0, #8]
        tst     r1, #0x80000000
        beq     4f
        subs    r12, r12, #1
        bne     3b
4:      str     r5, [r8, #0x100]
        mcr     p15, 0, r4, c9, c1, 0
        mov     sp, r11
        pop     {r4-r11, pc}
        .ltorg

@ ---- E5: DTCM load mode (control bit 17): reads go to the bus behind -------------------------------
        .global qy9_tcm_e5
        .type   qy9_tcm_e5, %function
qy9_tcm_e5:
        push    {r4-r11, lr}
        mov     r6, r0
        mov     r8, r1
        mrc     p15, 0, r4, c9, c1, 0
        mrc     p15, 0, r5, c1, c0, 0
        ldr     r9, =0x4D41494E
        str     r9, [r2, #0x100]
        add     r0, r2, #0x100
        mcr     p15, 0, r0, c7, c14, 1
        mov     r0, #0
        mcr     p15, 0, r0, c7, c10, 4
        ldr     r10, =0xD7D70001
        mov     r11, sp
        ldr     sp, =qy_alt_stack_top
        orr     r0, r8, #0x0A
        mcr     p15, 0, r0, c9, c1, 0
        ldr     r7, [r8, #0x100]        @ the DTCM word that is overwritten, put back below
        str     r10, [r8, #0x100]
        ldr     r0, [r8, #0x100]        @ normal read: the DTCM word
        orr     r1, r5, #0x20000
        mcr     p15, 0, r1, c1, c0, 0
        ldr     r1, [r8, #0x100]        @ load mode: the bus behind
        mcr     p15, 0, r5, c1, c0, 0
        str     r7, [r8, #0x100]
        mcr     p15, 0, r4, c9, c1, 0
        mov     sp, r11
        str     r0, [r6]
        str     r1, [r6, #4]
        pop     {r4-r11, pc}
        .ltorg

@ ---- E6: ITCM load mode (control bit 19), data read of the ITCM mirror from main RAM code ------------------------------
        .global qy9_tcm_e6
        .type   qy9_tcm_e6, %function
qy9_tcm_e6:
        push    {r4-r11, lr}
        mov     r6, r0
        mrc     p15, 0, r5, c1, c0, 0
        ldr     r10, =0x01005000
        ldr     r11, [r10]              @ ITCM word at 01005000h, put back below
        ldr     r9, =0xD7D70002
        str     r9, [r10]
        ldr     r0, [r10]               @ normal read
        orr     r1, r5, #0x80000        @ ITCM load mode
        mcr     p15, 0, r1, c1, c0, 0
        ldr     r1, [r10]
        mcr     p15, 0, r5, c1, c0, 0
        str     r11, [r10]
        str     r0, [r6]
        str     r1, [r6, #4]
        pop     {r4-r11, pc}
        .ltorg

@ ---- the alternate stack the TCM stubs use while the DTCM (and with it the normal stack) is moved away ---
        .section .bss
        .align  3
qy_alt_stack:
        .space  1024
        .global qy_alt_stack_top
qy_alt_stack_top:
        .space  8
