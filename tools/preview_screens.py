#!/usr/bin/env python3
"""Create a contact sheet from test_screenshots.c output. No image dependencies."""
import struct,zlib,sys
from pathlib import Path
names=['main_001','temperature_000','temperature_001','temperature_002','temperature_006','temperature_007','temperature_010','temperature_013','pit_estimate_000','pit_estimate_001','pit_estimate_006','pit_estimate_007','practice_setup_000','practice_result_001','qual_result_000','sprint_qual_result_000','map_040','weather_014','help_012','help_013']
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
