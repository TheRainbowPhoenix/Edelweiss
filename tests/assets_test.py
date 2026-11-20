from pathlib import Path
import struct
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from generate_assets import bmp565, encode_rle

source=Path(__file__).resolve().parents[1]/'assets'/'CASIO_EX.bmp'
data=source.read_bytes()
w,h,pixels=bmp565(data)
assert (w,h)==(528,320)
runs=encode_rle(pixels)
assert all(0<count<=65535 for count,_ in runs)
assert [color for count,color in runs for _ in range(count)]==pixels
assert len(set(pixels))>3  # Preserve antialiased shades, not a 3-color approximation.
assert len(runs)*4<len(pixels)*2//20
for bad in (b'bad',data[:-1],data[:54]):
    try:
        bmp565(bad)
    except ValueError:
        pass
    else:
        raise AssertionError('Truncated BMP accepted')
# Verify negative-height top-down BMPs, too.
top=bytearray(data)
offset=struct.unpack_from('<I',top,10)[0]
pitch=w*4
struct.pack_into('<i',top,22,-h)
top[offset:offset+pitch*h]=b''.join(data[offset+y*pitch:offset+(y+1)*pitch] for y in reversed(range(h)))
assert bmp565(top)[2]==pixels
print(f'Splash RLE round-trip passed: {len(data)} BMP bytes -> {len(runs)*4} RLE bytes')
