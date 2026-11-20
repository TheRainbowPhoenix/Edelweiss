"""Render the real EBrew C UI with a host API and Pillow text (not an emulator).

Generate assets, then compile a native shared library from payloads/ebrew.c and
wallpaper.c. Pass that .dll/.so and an output PNG to this script.
"""
import ctypes as C
from pathlib import Path
import sys
from PIL import Image, ImageDraw, ImageFont

class Event(C.Structure):
    _fields_ = [('key', C.c_int), ('x', C.c_int), ('y', C.c_int)]

class Record(C.Structure):
    _fields_ = [('path', C.c_char*256), ('title', C.c_char*32)]

class Metadata(C.Structure):
    _fields_ = [('magic', C.c_uint32), ('version', C.c_uint32),
                ('title', C.c_char*32), ('description', C.c_char*64),
                ('icon', C.c_uint16*1024)]

class Status(C.Structure):
    _fields_=[('year',C.c_uint16)]+[(n,C.c_uint8) for n in
        ('month','day','hours','minutes','seconds')]+[('battery',C.c_int8)]

Rect = C.CFUNCTYPE(None, C.c_int, C.c_int, C.c_int, C.c_int, C.c_uint16)
Text = C.CFUNCTYPE(None, C.c_int, C.c_int, C.c_uint16, C.c_char_p)
Blit = C.CFUNCTYPE(None, C.c_int, C.c_int, C.c_int, C.c_int, C.POINTER(C.c_uint16))
Present = C.CFUNCTYPE(None)
Wait = C.CFUNCTYPE(C.c_int, C.POINTER(Event))
Scan = C.CFUNCTYPE(C.c_int, C.c_uint, C.POINTER(Record), C.c_uint)
Meta = C.CFUNCTYPE(C.c_int, C.c_char_p, C.POINTER(Metadata))
Read = C.CFUNCTYPE(C.c_int, C.c_char_p, C.c_uint32, C.c_void_p, C.c_uint32)
Call = C.CFUNCTYPE(C.c_int, *([C.c_uint32]*5))
Launch = C.CFUNCTYPE(C.c_int, C.c_char_p)
GetStatus = C.CFUNCTYPE(C.c_int, C.POINTER(Status))
WaitTimeout = C.CFUNCTYPE(C.c_int, C.POINTER(Event), C.c_uint)

class API(C.Structure):
    _fields_ = [(n, C.c_uint32) for n in ('magic','version','size','width','height')]+[
        ('dispatcher', C.c_size_t), ('image_path', C.c_char_p),
        ('rect', Rect), ('text', Text), ('blit', Blit), ('present', Present),
        ('wait', Wait), ('scan', Scan), ('metadata', Meta), ('read', Read),
        ('os_call', Call), ('launch', Launch),
    ] + [(n, C.c_uint32) for n in ('arena_start','arena_end','stack_start','stack_end')]+[
        ('status',GetStatus),('wait_timeout',WaitTimeout)]

library = C.CDLL(str(Path(sys.argv[1]).resolve()))
output = Path(sys.argv[2]); output.parent.mkdir(parents=True, exist_ok=True)
canvas = Image.new('RGB', (528,320), 'white')
font = ImageFont.load_default()
def rgb(c):
    return ((c>>11)*255//31, ((c>>5)&63)*255//63, (c&31)*255//31)

@Rect
def rect(x,y,w,h,color):
    ImageDraw.Draw(canvas).rectangle((x,y,x+w-1,y+h-1),fill=rgb(color))
@Text
def text(x,y,color,s):
    ImageDraw.Draw(canvas).text((x,y-2),s.decode('ascii','replace'),font=font,fill=rgb(color))
@Blit
def blit(x,y,w,h,data):
    image=Image.new('RGB',(w,h)); image.putdata([rgb(data[i]) for i in range(w*h)])
    canvas.paste(image,(x,y))
@Present
def present():
    if not getattr(present,'shown',False):
        canvas.save(output.with_name(output.stem+'-splash.png'))
        present.shown=True
    canvas.save(output)
@Wait
def wait(e):
    e[0]=Event(9,0,0)  # EDL_HOME
    return 0
@GetStatus
def get_status(out):
    out[0]=Status(2026,11,20,23,43,34,1)
    return 0
@WaitTimeout
def wait_timeout(e,ms):
    return wait(e)

names = ['EBrew','Large ELF demo','Embedded example']
paths = [f'\\\\drv0\\EDELW\\_CONTENT\\{n}.edl'.encode() for n in ('EBrew','large-demo','main')]
@Scan
def scan(first,rows,capacity):
    for i in range(min(capacity,max(0,len(names)-first))):
        rows[i].path=paths[first+i]; rows[i].title=names[first+i].encode()
    return len(names)
@Meta
def metadata(path,out):
    i=paths.index(path)
    C.memset(out,0,C.sizeof(Metadata))
    out[0].magic=0x45444c31; out[0].version=1
    out[0].title=names[i].encode()
    out[0].description=b'The Edelweiss home screen' if not i else b'A real payload larger than the D01 buffer'
    if not i:
        source=Metadata.in_dll(library,'edl_meta')
        for j in range(1024): out[0].icon[j]=source.icon[j]
    else:
        for y in range(32):
            for x in range(32):
                c=(0x936f if i==1 else 0x4c91) if 1<x<30 and 1<y<30 else 0xffff
                if 8<=x<=23 and (8<=y<=11 or 20<=y<=23): c=0xffff
                if 8<=x<=11 and 8<=y<=23: c=0xffff
                out[0].icon[y*32+x]=c
    return 0

api=API(magic=0x45444c31,version=1,size=C.sizeof(API),width=528,height=320,
        image_path=paths[0],rect=rect,text=text,blit=blit,present=present,
        wait=wait,scan=scan,metadata=metadata,status=get_status,wait_timeout=wait_timeout)
library.edl_main.argtypes=[C.POINTER(API)]
assert library.edl_main(C.byref(api))==0
# Check the actual C decoder's displayed pixels against the original BMP.
source=Path(__file__).resolve().parents[1]/'assets'/'CASIO_EX.bmp'
expected=Image.open(source).convert('RGB')
quantized=[rgb(((r>>3)<<11)|((g>>2)<<5)|(b>>3)) for r,g,b in expected.getdata()]
shown=Image.open(output.with_name(output.stem+'-splash.png')).convert('RGB')
assert list(shown.getdata())==quantized
print(output)
