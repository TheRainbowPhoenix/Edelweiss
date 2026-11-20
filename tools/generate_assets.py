"""Deterministic RGB565 UI assets; no imaging dependencies required."""
from pathlib import Path
import sys

def rgb(r, g, b):
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)

def icon(color, letter):
    data = []
    for y in range(32):
        for x in range(32):
            c = color if 1 < x < 30 and 1 < y < 30 else 0xFFFF
            if letter == 'E' and (7 <= x <= 10 and 7 <= y <= 24 or
                7 <= x <= 23 and (7 <= y <= 10 or 14 <= y <= 17 or 21 <= y <= 24)):
                c = 0xFFFF
            if letter == 'D' and (7 <= x <= 10 and 7 <= y <= 24 or
                20 <= x <= 23 and 10 <= y <= 21 or
                10 <= x <= 20 and (7 <= y <= 10 or 21 <= y <= 24)):
                c = 0xFFFF
            data.append(c)
    return ','.join(f'0x{c:04x}' for c in data)

out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
(out / 'ebrew_assets.h').write_text(
    '#include <stdint.h>\nextern const uint16_t edl_wallpaper[528*160];\n'
    f'#define EDL_EBREW_ICON {icon(0x3d9b, "E")}\n'
    f'#define EDL_DEMO_ICON {icon(0x936f, "D")}\n')
with (out / 'wallpaper.c').open('w') as f:
    f.write('#include <stdint.h>\nconst uint16_t edl_wallpaper[528*160]={\n')
    for y in range(160):
        row = []
        for x in range(528):
            shade = 250 - y // 16
            if x % 8 == 0 or y % 8 == 0:
                shade -= 14
            row.append(str(rgb(shade, min(255, shade+2), min(255, shade+3))))
        f.write(','.join(row) + ',\n')
    f.write('};\n')
