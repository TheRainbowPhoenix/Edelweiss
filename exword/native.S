/* EX-word dispatch ABI: r0 = selector, r4-r7 = arguments. */
.text
.global _edl_native_dispatch
_edl_native_dispatch:
    mov r4, r0
    mov r5, r4
    mov r6, r5
    mov r7, r6
    mov.l @r15, r7
    mov.l .dispatcher, r2
    mov.l @r2, r2
    jmp @r2
    nop
.balign 4
.dispatcher: .long 0x74000000

/* Separate stack, preserving the host's SH ABI and control registers.
   r8 holds the host frame; an ABI-conforming payload preserves it. */
.global _edl_invoke
_edl_invoke:
    sts.l pr, @-r15
    stc.l gbr, @-r15
    stc.l sr, @-r15
    stc.l vbr, @-r15
    sts.l mach, @-r15
    sts.l macl, @-r15
    mov.l r8, @-r15
    mov.l r9, @-r15
    mov.l r10, @-r15
    mov.l r11, @-r15
    mov.l r12, @-r15
    mov.l r13, @-r15
    mov.l r14, @-r15
    mov r15, r8
    mov r4, r0
    mov r5, r4
    mov r6, r15
    jsr @r0
    nop
    mov r8, r15
    mov.l @r15+, r14
    mov.l @r15+, r13
    mov.l @r15+, r12
    mov.l @r15+, r11
    mov.l @r15+, r10
    mov.l @r15+, r9
    mov.l @r15+, r8
    lds.l @r15+, macl
    lds.l @r15+, mach
    ldc.l @r15+, vbr
    ldc.l @r15+, sr
    ldc.l @r15+, gbr
    lds.l @r15+, pr
    rts
    nop
