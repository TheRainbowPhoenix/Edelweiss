# XD-A8500 ROM32A loader notes

The implemented EX-word port uses these observations. See
[EDL-ABI.md](docs/EDL-ABI.md) for the experimental arena assumptions and
execution contract; device execution has not yet been validated.

* `Addin_LoadAndInitialize` at `802d57a8` supplies a `0x20000` image
  buffer and `0x8000` API buffer, then enters the mapped image at `+0x80`.
* `Addin_InvokeEntry` at `80303960` writes the dispatcher into API backing
  at `8c1d0000`, normally mapped at `74000000`. The dispatcher receives its
  selector in r0 and arguments in r4-r7.
* `MMU_ConfigureAddinMappings` at `80046234` reads header offsets `0x2c`
  and `0x30`; zero means image `70000000` and API `74000000`.
* `Addin_BuildContentRoot` at `802d8a08` resolves internal content under
  `\\drv0\<ID>` and card content under `\\crd0\CASIO\<ID>`.
* Syscall `0x15` at `802d89bc` copies the active eight-byte content ID to
  r4 and eight bytes of associated metadata to r5. Disassembly confirms the
  two destination pointers; the initial decompiler prototype was incomplete.
* The SDRAM-save routine at `80065e58` walks the range through `ac7fffff`.
  Fixed OS work-buffer references reach the `ac6d02a0` region. Upper-RAM
  ownership remains an inference requiring a hardware test.

MPM's resident-loader/backing/entry separation is retained. The EX-word port
uses static ELF programs linked into P1, so payload instructions bypass the
TLB. It keeps the original OS/gint mappings active for native services and
does not copy Math+'s MMU-disable transition. The host API passes the captured
dispatcher, provides OS world switching, and queues the next launch until the
current ELF has returned to the resident stack.
