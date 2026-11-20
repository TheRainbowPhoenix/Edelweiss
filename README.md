# Edelweiss for EX-word

Edelweiss is an embeddable stage-0 D01 loader for big-endian SH ELF programs
(`.edl`). The EX-word port keeps the original MPM/Math+ sources available and
adds an XD-A8500 loader, EBrew home screen, SDK, and Japanese/Chinese installation
packages. The EX-word code is in `exword/`, `sdk/`, and `payloads/`.

![EBrew host preview](docs/ebrew-preview.png)

Host preview of the real EBrew C UI using Pillow text; this is not a device screenshot.

**Experimental hardware build:** compilation and host checks are separate from
device validation. This port uses a ROM32A-specific upper-RAM arena, inferred
from the firmware layout rather than reserved through an OS allocator. It has
not yet been validated on a physical XD-A8500. Unknown firmware is rejected
before the payload arena is touched. JP/CN refer to installation metadata;
they do not enable execution on unverified dictionary models.

## Install and try

Download **Edelweiss-ja** or **Edelweiss-cn** from the
[EX-word workflow](https://github.com/TheRainbowPhoenix/Edelweiss/actions/workflows/exword.yml).
Each artifact contains two installation folders:

```text
EDELW/
  edelweiss.d01       resident loader, below the native 128 KiB limit
  EBrew.edl          ELF home screen, larger than 128 KiB
  large-demo.edl     independent large ELF example
  diction.htm, ...   locale-specific install metadata
  fileinfo.cji
EXMPL/
  example.d01        identical stage 0 under a different filename
  main.edl           launches directly without the EBrew menu
  diction.htm, ...
  fileinfo.cji
```

Install these folders with the usual EX-word add-in transfer tool. It places
program content under `_CONTENT`. When adding files to already-installed
content, keep the EDL adjacent to the D01:

```text
\\drv0\EDELW\_CONTENT\edelweiss.d01
\\drv0\EDELW\_CONTENT\EBrew.edl
\\drv0\EDELW\_CONTENT\large-demo.edl
\\crd0\CASIO\EXMPL\_CONTENT\example.d01
\\crd0\CASIO\EXMPL\_CONTENT\main.edl
```

EBrew discovers `*.edl` in each application's `_CONTENT` folder under
`\\drv0`, `\\drv0\CASIO`, `\\crd0\CASIO`, and `\\crd1\CASIO`.
Missing card slots are skipped. Press the arrows to select, EXE to launch,
R to rescan after changing cards, and Back to exit. Tap a tile to select it
and tap again to launch. Returning from an application reloads EBrew. Selecting
EBrew itself performs a normal ELF reload through the same loader.

The included wallpaper contributes 168,960 actual loadable bytes to both ELF
programs. CI verifies loadable segment sizes, rather than counting ELF padding
or debug information, and runs the same segment copying and BSS zeroing code
used on the device.

## Embed stage 0 in another add-in

Copy the generated `edelweiss.d01` under your module filename, update the
installation metadata to use that filename, and put a compatible `main.edl`
next to it. The loader queries the active content ID through syscall 0x15 to
find the installation directory; it prefers `main.edl` to `EBrew.edl`. EXMPL
is a complete packaged example. If no active content ID is available, the
build's `EDL_APPID` provides the fallback ID.

## Build

GitHub CI builds fxSDK from `fix/exword-data-load-alignment`, gint from
`feature/exword-runtime-bridge`, the small D01, both ELF programs, and both
package locales. It uploads installation folders, ELF/maps and the SDK. There
is no ROM dump in the build or artifacts.

For an existing EX-word SDK installation:

```sh
cmake -S . -B build-exword \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/fxsdk/lib/cmake/fxsdk/EXWORD.cmake \
  -DCMAKE_PREFIX_PATH=/path/to/gint \
  '-DCMAKE_MODULE_PATH=/path/to/gint/lib/cmake/fxsdk;/path/to/fxsdk/lib/cmake/fxsdk' \
  -DEXWORD_METADATA_SOURCE=/path/to/exword-xd-a8500-TEST
cmake --build build-exword --parallel 2
```

The original Math+ target still builds with `fxsdk build-cg`.
See [the payload ABI](docs/EDL-ABI.md) for independent ELF programs and the
firmware-specific memory assumptions.
