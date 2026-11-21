# Allocator-backed MMU loading (experimental backend)

The default EX-word loader still places EDLs in the known P1 arena. This
backend provides the **allocation and mapping contract** for replacing the
hardcoded physical arena once the platform can supply OS-owned RAM and a safe
TLB lease. It is **not enabled** on hardware by this PR.

## Layout

| Virtual region | Use |
| --- | --- |
| `00300000..003effff` | ELF executable/data, maximum 960 KiB |
| `003f0000..003f7fff` | gap |
| `003f8000..003fffff` | 32 KiB stack with canary |

These are P0 virtual addresses, not physical addresses. The backend requests
`0x10ffff` bytes and aligns the 1 MiB virtual backing to sixteen 64 KiB units.
It can use one contiguous P1 allocation, or an OS-virtual allocation with a
verified per-4-KiB translation callback. Each mapped 64 KiB frame still needs
to be physically aligned and contiguous, but the sixteen frames may be
**non-adjacent** in physical RAM. This deliberately trades up to 64 KiB of
allocation slack for sixteen UTLB slots, rather than requiring 256 separate
4 KiB mappings. `malloc()` does not reserve our fixed P0 destination or
guarantee any particular physical page layout.

An EX-word port supplies `edl_memory_ops`:

1. `alloc/free`: allocate and pin OS-managed RAM. A directly accessible P1
   block may omit `translate`, and then **must** be physically contiguous.
   A virtual OS allocation can supply `translate` to validate every 4 KiB
   physical subpage within each 64 KiB backing unit **while OS mappings are
   active**. Free with precisely the matching OS API, never while a UTLB or
   ITLB alias is installed.
2. `map`: reserve the P0 window and install a 64 KiB UTLB entry. The
   callback must not overwrite currently active OS/gint mappings. Use a
   *verified hardware-specific* MMU driver with a slot lease, proper
   MMUCR.URC/URB ownership, cache synchronization and interrupt masking.
3. `unmap`: remove the mapping and any ITLB alias, restore the host's
   previous TLB and MMU state and return success **only when safe to free**.
   A failed unmap intentionally leaks the allocation rather than allowing
   stale translations to access freed RAM.

The order is `edl_memory_open`, `edl_load_mapped`, flush P1/P2 operand
aliases and invalidate code lines, `edl_memory_activate`, switch to a
**mapped P0 stack**, execute the EDL, unwind to the resident P1 stack,
`edl_memory_close`.

`edl_load_mapped` uses the existing two-pass ELF relocation validation,
shifts SH DIR32/REL32 fixups to the mapped P0 base and writes file bytes
through uncached P2. It rejects oversized images before a write. The default
`edl_load` still uses the known firmware arena.

## Why it is not active automatically

EX-word gint's TLB-miss handler explicitly refills only the
`0x70000000` resident image and `0x74000000` small RAM window. An unmapped
P0 instruction fetch is sent to the original firmware exception handler.
The OS and gint also change MMUCR and UTLB ownership across
`gint_world_switch`; those calls occur during EDL filesystem operations.
It is not safe to enter P0 until we have either a reserved, non-evictable
set of sixteen slots **across OS calls** or an explicit restore/refill
integration on every relevant world transition. Ordinary `malloc()` cannot
solve that.

The referenced `libdataplus-ex` does not expose a host heap syscall in
`include/syscalls/syscalls.h`; its `__libexword_malloc` uses a local
`sbrk` that stops at `0x74008000`. This is the small D01 virtual window,
**not** a 1 MiB OS-managed heap. An OS-native allocator still needs a
verified dispatcher ID/function and matching release ABI, rather than an
invented syscall number.

Device-specific enablement therefore requires a verified OS allocator,
address-space reservation (including collision checks), TLB lease and
restore, a P0 executable-stack trampoline, cache/barrier tests and emulator
evidence for filesystem calls, interrupts and normal exit. A fingerprinted
firmware or equivalent trusted host API is still necessary. Arbitrary
firmware support cannot be guaranteed merely from a similar RAM map.

## Host validation

`tests/memory_test.c` checks alignment, P1 physical range, sixteen
mappings, bounds, rollback and fail-closed unmap. `tests/relocation_test.c`
also validates relocation from a P1 preferred image into a fixed P0
destination. These are CPU-independent tests: they do **not** demonstrate
successful SH7305 MMU programming, exception handling or allocator support.
