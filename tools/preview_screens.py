#!/usr/bin/env python3
"""Create a contact sheet from test_screenshots.c output. No image dependencies."""
import struct,zlib,sys
from pathlib import Path
names=['main_001','role_000','role_001','slots_002','championship_000','championship_020','champions_000','champions_001','history_empty_000','history_000','history_result_000','history_result_019','single_qual_000','single_red_000','help_017','help_018']
folder=Path(sys.argv[1]);output=Path(sys.argv[2]) if len(sys.argv)>2 else Path(__file__).resolve().parent.parent/'assets/ui_preview.png'
scale=3;cols=4;cellw=128*scale+12;cellh=64*scale+12;w=cols*cellw+12;h=((len(names)+cols-1)//cols)*cellh+12
pixels=[bytearray([230]*w) for _ in range(h)]
for i,name in enumerate(names):
    data=(folder/(name+'.pbm')).read_bytes().split(b'\n',2)[2]
    for y in range(64):
        for x in range(128):
            color=0 if data[y*16+x//8]&(128>>(x%8)) else 255
            for sy in range(scale):
                xx=12+(i%cols)*cellw+x*scale;yy=12+(i//cols)*cellh+y*scale+sy
                pixels[yy][xx:xx+scale]=bytes([color])*scale
raw=b''.join(b'\0'+bytes(row) for row in pixels)
def chunk(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d))
output.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
print(output)
