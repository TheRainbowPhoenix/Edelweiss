.section .text.entry
.global _reloc_entry

/* MPM loads us at 0x8c700000. We don't need that much memory, so relocate
   ourselves to _reloc_target (loader-cg.x) to leave more room for add-ins. */
_reloc_entry:
    mov.l   .src, r0
    mov.l   .dst, r1
    mov.l   .size, r2

1:  /* Copy an entire cache line (32 bytes) */
    mov.l   @r0+, r3
    mov.l   r3, @r1
    mov.l   @r0+, r3
    mov.l   r3, @(4, r1)
    mov.l   @r0+, r3
    mov.l   r3, @(8, r1)
    mov.l   @r0+, r3
    mov.l   r3, @(12, r1)
    mov.l   @r0+, r3
    mov.l   r3, @(16, r1)
    mov.l   @r0+, r3
    mov.l   r3, @(20, r1)
    mov.l   @r0+, r3
    mov.l   r3, @(24, r1)
    mov.l   @r0+, r3
    mov.l   r3, @(28, r1)

    /* To make sure the code loads properly, write-back OC, invalidate IC */
    ocbwb   @r1
    synco
    icbi    @r1

    dt      r2
    bf.s    1b
    add     #32, r1

    /* Jump over to relocated code */
    mov.l   .start, r0
    jmp     @r0
    nop

.balign 4
.src:   .long 0x8c700000
.dst:   .long _reloc_target
.size:  .long _reloc_size32
.start: .long _start
