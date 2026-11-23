# Hosted gint API surface for EDL payloads

Edelweiss reuses the resident gint kernel. Hosted EDL programs can link against
functions listed in [sdk/gint_imports.def](../sdk/gint_imports.def) without
initializing or installing a second kernel. The ELF loader resolves these
imports to resident function addresses at launch.

## Graphics

Exposed fast paths include:

- \`dimage(x,y,image)\` and
  \`dsubimage(x,y,image,left,top,width,height,flags)\`
  from \`<gint/display.h>\`, with \`image_t\` from \`<gint/image.h>\`.
  Images must use the correct native gint layout, a valid backing buffer and
  appropriate byte stride (RGB565 rows are 4-byte padded).
- \`dpixel\`, \`dgetpixel\`, \`dline\`, \`dhline\`, \`dvline\`,
  \`dcircle\`, \`dellipse\`, and \`dwindow_set\` for drawing/window clipping.
- \`dsize\` and \`dnsize\` for font-dependent text measurement.
  These are the same proportional-font metrics used by the Toybox shell.

Existing \`struct edl_api\` callbacks (\`rect\`, \`text\`, \`blit\`, \`present\`)
remain supported; no ABI field has been inserted or reordered. Native
\`dimage\` should be preferred for repeated sprite/tile rendering rather than
decoding JS arrays or issuing per-pixel callbacks on each frame.

## Input and touch

The resident already exports the underlying hardware event functions
\`pollevent\`, \`waitevent\`, \`keydown\`, \`keypressed\`,
\`keyreleased\`, \`clearevents\`, and \`cleareventflips\`.
These supply **physical gint keycodes** and distinct
\`KEYEV_TOUCH_DOWN\`, \`KEYEV_TOUCH_DRAG\`, \`KEYEV_TOUCH_UP\`
events, with coordinates and timestamps (see \`<gint/keyboard.h>\`).
The additional \`keycode_digit\` and \`keycode_function\` exports help
portable input handling.

The older \`struct edl_event\` semantic callback remains intentionally
unchanged; its \`EDL_UP\`/\`EDL_OK\` codes are **not** physical
\`KEY_UP\`/\`KEY_EXE\` codes. Apps that need a PythonExtra-style
\`pollevent()\` interface should use the native hosted imports.

## Compatibility

The ABI version, \`struct edl_api\` size/offsets, and Toybox's embedded
\`struct shell_api\` remain unchanged. This is essential: inserting new fields
in the middle of \`struct edl_api\` would shift Toybox's extension data
and break existing applets.

The new imports require a **matching updated resident**. A QJS payload
linked against newly exported image symbols cannot be loaded by an old
\`toybox.d01\` or \`qjs.d01\`. Install both native resident and payload
together, and retain a backup of the last working package until tested on the
actual EX-word firmware.
