# Allocator-backed MMU loading (experimental backend)

The normal EX-word loader now **prefers syscall 0xae selector 3** across
all ROMs that expose it. It checks the returned P1/P2 SDRAM pointer and size,
aligns down to a set of physical 64 KiB chunks, and loads relocatable EDLs
directly through P1/P2 aliases. This provides a generic **direct P1 execution
path** without requiring a firmware fingerprint. If the syscall returns an
invalid buffer, the loader falls back only to known firmware legacy arenas.

The separate **fixed-VA MMU backend remains experimental and disabled**.
The ROM's 0xae buffer is a fixed scratch region, not malloc/free. Execution
using it relies on the user-supplied deployment assumption that the host
allows use of the buffer throughout the EDL lifetime, including its native
filesystem/UI callbacks. This cannot be inferred solely from the A8500
ROM, so this is not a proved all-device guarantee.

## Generic syscall 0xae P1 execution

`edl_syscall_ae_probe` calls `edl_os_call(0xae, 3, &bytes, 0, 0)` via
the native OS world. `edl_scratch_layout` validates a P1/P2 SDRAM alias,
rejects physical overflow, and requires at least three complete 64 KiB
chunks. It does **not** require a ROM fingerprint or fixed pointer value.
It caps a larger returned buffer to a 4 MiB executable arena.

One final 64 KiB chunk holds a 32 KiB guard and 32 KiB stack; preceding
full chunks hold the ELF. On the A8500, the returned
`0xac3c2018 / 0x100004` produces:
- executable P1 image `0x8c3d0000..0x8c4b0000` (896 KiB)
- guard `0x8c4b0000..0x8c4b8000`
- stack `0x8c4b8000..0x8c4c0000`

Different models can return different lengths and addresses. Existing ELF
relocations adjust to the actual P1 address. **No new MMU mapping is
installed on this path.** It does not yet provide a common fixed virtual VA.
Both this generic scratch path and its eventual MMU replacement depend on a
host-owned buffer whose lifetime is not independently verified by the ROM.

## Experimental fixed-VA MMU layout

| Virtual region | Use |
| --- | --- |
| `71000000..710effff` | ELF executable/data, maximum 960 KiB |
| `710f0000..710f7fff` | gap |
| `710f8000..710fffff` | 32 KiB stack with canary |

These are P0 virtual addresses, not physical addresses. The backend requests
`0x10ffff` bytes and aligns the 1 MiB virtual backing to sixteen 64 KiB units.
It can use one contiguous P1 allocation, or an OS-virtual allocation with a
verified per-4-KiB translation callback. Each mapped 64 KiB frame still needs
to be physically aligned and contiguous, but the sixteen frames may be
**non-adjacent** in physical RAM. This deliberately trades up to 64 KiB of
allocation slack for sixteen UTLB slots, rather than requiring 256 separate
4 KiB mappings. `malloc()` does not reserve our fixed P0 destination or
guarantee any particular physical page layout.

The old `0x00300000` candidate is **invalid on A8500**: the firmware's
`MMU_HandleTranslationFault` maps addresses below `0x70000000` to
`0x10000000 | (VA & 0x007ff000)` on TLB miss. It cannot reliably alias
allocated SDRAM. The new `0x71000000` candidate is *within the firmware's
add-in primary virtual-address class*, but that class is only refilled inside
the OS-configured `ADDIN_PRIMARY_VIRTUAL_START..END` interval; therefore
this candidate still requires explicit OS-world MMU lease/refill integration.

An EX-word port supplies `edl_memory_ops`:

1. `alloc/free`: allocate and pin OS-managed RAM. A directly accessible P1
   block may omit `translate`, and then **must** be physically contiguous.
   A virtual OS allocation can supply `translate` to validate every 4 KiB
   physical subpage within each 64 KiB backing unit **while OS mappings are
   active**. Free with precisely the matching OS API, never while a UTLB or
   ITLB alias is installed.
2. `acquire_window/release_window`: **mandatory** host MMU lease. Refuse
   any collision, missing TLB refill for an evicted EDL page, unsafe interrupt
   path, or lack of remapping across OS/gint world switches. A generic
   always-success stub is not a valid hardware implementation. Release only
   after all UTLB and ITLB aliases are verified absent.
3. `map`: install a 64 KiB UTLB entry. Never overwrite OS/gint mappings.
   Use a *verified hardware-specific* driver with correct MMUCR.URC/URB,
   cache synchronization and interrupt masking.
4. `unmap`: remove the mapping and any ITLB alias; report success only
   when the physical frame can no longer be accessed through that virtual page.
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
A8500's ROM similarly refills **only** its configured add-in primary/API
ranges; it does not permit arbitrary P0 RAM aliases. The OS and gint also
change MMUCR and UTLB ownership across
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
