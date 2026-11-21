# EDL ABI v1

## Execution model

An `.edl` is a static ELF32, big-endian, `EM_SH` (42), `ET_EXEC` image.
The ELF entry receives `const struct edl_api *` in r4 and returns an integer
through the normal SH GCC ABI. The SDK links to an **initial preferred P1
base** (the XD-A8500 arena by default), not the native D01 window. At load
time the resident reads the `.edl.reloc` link-range marker, verifies that
the declared range fits the detected firmware's backing arena, and applies
SH relocations and any resident imports. The same EDL can therefore run at a
different verified P1 address without relinking. The file must retain its
SDK relocation marker and `--emit-relocs` entries. P1 accesses bypass address
translation even
while the MMU remains enabled. This removes the shell's 128 KiB image-buffer
limit without disabling the MMU or changing the OS's mapping descriptors.

The resident D01 keeps gint active while a payload runs. Its interrupt and TLB
handlers continue servicing the resident loader's original virtual ranges.
ELF payload code, data, and stack use P1 addresses and require no UTLB slots.
The resident services cross into the OS with `gint_world_switch` for native
filesystem and syscall access, then return to the same gint world.

Payloads currently use the supplied display/input/filesystem services. Linking
a second, unmodified gint kernel into the payload is not supported. Existing
G3A and D01 files must be rebuilt for this ABI; a filename change is insufficient.

## Relocation is not universal device support

The runtime load base is a **firmware-owned decision**; an EDL's link base
is not a physical allocation request. For example, an EDL built at
`0x8c900000` can be relocated into either known ROM32A arena provided
the complete declared link range fits. Malformed markers, unsupported
relocation kinds, and undersized arenas are rejected before payload writes.

This adopts the useful relocation idea from the older `dynload` G3A
experiment, but it does **not** copy that loader's UTLB layout or replace
MMUCR. The NTOS_SH4 loader similarly separates relocation from mapping;
its paging and VBR handoff experiments are not generic EX-word driver code.

"Any device" also needs a working native bootstrap, CPU/ABI compatibility,
verified RAM ownership, cache coordination, firmware-specific OS services
and safe driver/context handoff. The two current EX-word ROM profiles are
not proof that arbitrary EX-word firmware or other Casio platforms work.
An unknown firmware must continue to fail closed.

## ROM32A memory profile

| Address | Purpose |
| --- | --- |
| `70000000..7001ffff` | Original D01 image and native header |
| `74000000..74007fff` | Original API dispatcher, resident gint RAM and host stack |
| `8c700000..8c7effff` | ELF PT_LOAD arena, 960 KiB including payload BSS |
| `8c7f0000..8c7f7fff` | Gap between arena and payload stack |
| `8c7f8000..8c7fffff` | Separate 32 KiB payload stack with bottom canary |

The ROM's SDRAM save path (`80065e58`) processes RAM through `ac7fffff`.
Observed fixed OS work-buffer references include the `ac680440`, `ac6cb444`
and `ac6d02a0` region. The selected upper 1 MiB is an **experimental inference**,
not a proven OS allocation contract. Its suitability, including asynchronously
used buffers, needs hardware validation. Identical SH7305 CPUs do not establish
identical firmware RAM ownership.

`firmware.c` checks three 16-byte instruction fingerprints before
selecting a supported ROM32A profile. XD-A8500 and XD-D6600 CY436 use
different candidate P1 arenas, neither an OS allocation guarantee. Unknown
firmware is rejected. `crd1` discovery is independent of the profile.

The loader flushes operand aliases before writing ELF bytes through uncached
P2, synchronizes, and invalidates instruction lines before entering through
P1. It does not modify MMUCR, UTLB, ITLB, or the OS refill descriptors. The
entry trampoline changes stacks and preserves the host's callee-saved
registers, GBR, PR, MACH/MACL, SR and VBR across normal return. Payloads must
obey the C ABI, leave interrupt/driver ownership with the host, and unwind
normally. There is no process isolation or recovery from an exception, hang,
or arbitrary memory corruption.

## Services and return

`sdk/edl.h` is the versioned interface. Check `magic`, `version`, and `size`
before accessing it (the supplied `sdk/start.c` does this automatically).
It provides drawing, input, paginated discovery, metadata, bounded file reads,
and an OS syscall bridge. `syscall_dispatcher` is the dispatcher pointer copied
from the word at `74000000`, not a conventional function-pointer array.
Use `os_call(id,a,b,c,d)` to invoke it in the OS world; calling it directly
while gint owns hardware is invalid.

`image_path` is the exact native path for this invocation. All API pointers
expire when the payload returns. `launch(path)` copies a requested next ELF
path into resident storage. On success, return `EDL_LAUNCH`. The current stack
is unwound before loading the next image into the shared arena. Return
`EDL_EXIT` for normal exit or a negative value for a reported error. There is
no recursive loading, and no persistent payload heap across launches.

The ELF parser validates the complete program-header table before the first
destination write. It bounds file reads, segment addresses, BSS, alignment,
overlaps and entry, and rejects interpreters, dynamic linking and TLS. A failed
read during copying may leave a partial arena; it is never executed.

## Metadata

An optional non-allocated `.edl.meta` PROGBITS section contains exactly
`struct edl_metadata`: two big-endian 32-bit values (`EDL1`, version 1), a
32-byte NUL-terminated title, a 64-byte NUL-terminated description and 1024
RGB565 pixels (32x32). The SDK linker retains it outside PT_LOAD. The launcher
reads it from the file without executing code. Malformed or absent metadata
falls back to the filename and a generic icon.

## Minimal payload project

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyEDL LANGUAGES C ASM)
include(/path/to/Edelweiss/sdk/GenerateEDL.cmake)
add_edl(my_program main.c)
```

```c
#include "edl.h"
int edl_main(const struct edl_api *api)
{
    api->rect(0,0,528,320,0xffff);
    api->text(16,16,0,"Hello from ELF");
    api->present();
    struct edl_event event;
    api->wait(&event);
    return EDL_EXIT;
}
```

Configure with the EXWORD toolchain. The SDK supplies startup, constructor
handling, a small memory/string runtime and the fixed-address linker script.
The loader performs initialized-data copies and BSS clearing before entry.
