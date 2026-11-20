from pathlib import Path
import struct
import sys

base = Path(sys.argv[1])
for locale in ('ja', 'cn'):
    for appid, module, payloads in (
        ('EDELW', 'edelweiss.d01', ('EBrew.edl', 'large-demo.edl')),
        ('EXMPL', 'example.d01', ('main.edl',)),
    ):
        package = base / 'packages' / locale / appid
        image = (package / module).read_bytes()
        assert image[:16] == b'CASIODICPLUGIN10'
        assert 128 < len(image) <= 131072
        assert (package / 'fileinfo.cji').is_file()
        html = list(package.glob('*.htm'))
        assert html
        for p in html:
            data = p.read_bytes()
            assert b'@APP' not in data
            assert b'\r\n' in data
        for name in payloads:
            data = (package / name).read_bytes()
            assert data[:7] == b'\x7fELF\x01\x02\x01'
            assert struct.unpack_from('>H', data, 18)[0] == 42
            assert len(data) > 131072
        print(locale, appid, len(image), 'D01 bytes: package complete')
