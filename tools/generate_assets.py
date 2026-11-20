"""Deterministic RGB565 UI assets; no imaging dependencies required."""
from pathlib import Path
import sys
import struct

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

def bmp565(data):
    if len(data)<54 or data[:2]!=b'BM':
        raise ValueError('Invalid BMP header')
    offset=struct.unpack_from('<I',data,10)[0]
    dib,width,height,planes,bpp,compression=struct.unpack_from('<IiiHHI',data,14)
    if dib<40 or planes!=1 or bpp not in (24,32) or compression!=0 or width<=0 or not height:
        raise ValueError('Expected uncompressed 24/32-bit BMP')
    pitch=((width*bpp+31)//32)*4
    if offset<14+dib or offset+pitch*abs(height)>len(data):
        raise ValueError('Truncated BMP pixels')
    pixels=[]
    for y in range(abs(height)):
        row=offset+(abs(height)-1-y if height>0 else y)*pitch
        for x in range(width):
            blue,green,red=data[row+x*(bpp//8):row+x*(bpp//8)+3]
            pixels.append(rgb(red,green,blue))
    return width,abs(height),pixels

def encode_rle(pixels):
    runs=[]
    for color in pixels:
        if runs and runs[-1][1]==color and runs[-1][0]<65535:
            runs[-1]=(runs[-1][0]+1,color)
        else:
            runs.append((1,color))
    return runs

def generate(out):
    out.mkdir(parents=True,exist_ok=True)
    source=Path(__file__).resolve().parents[1]/'assets'/'CASIO_EX.bmp'
    width,height,pixels=bmp565(source.read_bytes())
    if (width,height)!=(528,320):
        raise ValueError('The EX-word splash must be 528x320')
    runs=encode_rle(pixels)
    (out/'ebrew_assets.h').write_text(
        '#include <stdint.h>\nextern const uint16_t edl_wallpaper[528*160];\n'
        'extern const uint16_t edl_splash_rle[][2];\nextern const unsigned edl_splash_runs;\n'
        f'#define EDL_SPLASH_WIDTH {width}\n#define EDL_SPLASH_HEIGHT {height}\n'
        f'#define EDL_EBREW_ICON {icon(0x3d9b,"E")}\n'
        f'#define EDL_DEMO_ICON {icon(0x936f,"D")}\n')
    with (out/'splash.c').open('w') as f:
        f.write('#include <stdint.h>\nconst uint16_t edl_splash_rle[][2]={\n')
        f.writelines(f'{{{count},0x{color:04x}}},\n' for count,color in runs)
        f.write(f'}};\nconst unsigned edl_splash_runs={len(runs)};\n')
    print(f'Splash: {len(pixels)*2} RGB565 bytes -> {len(runs)*4} RLE bytes, '
          f'{len(set(pixels))} shades retained')
    with (out/'wallpaper.c').open('w') as f:
        f.write('#include <stdint.h>\nconst uint16_t edl_wallpaper[528*160]={\n')
        for y in range(160):
            row=[]
            for x in range(528):
                shade=250-y//16
                if x%8==0 or y%8==0:
                    shade-=14
                row.append(str(rgb(shade,min(255,shade+2),min(255,shade+3))))
            f.write(','.join(row)+',\n')
        f.write('};\n')

if __name__=='__main__':
    generate(Path(sys.argv[1]))
