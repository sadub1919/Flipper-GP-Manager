"""Dependency-free 48x32 monochrome Ferrari and Red Bull badge authoring."""
from pathlib import Path
import re, struct, zlib
ROOT=Path(__file__).resolve().parent.parent

def badge(team):
    pixels=[[0]*48 for _ in range(32)]
    def polygon(points, ink=1):
        for y in range(32):
            for x in range(48):
                inside=False
                for (ax,ay),(bx,by) in zip(points,points[1:]+points[:1]):
                    if (ay>y+.5)!=(by>y+.5) and x+.5<(bx-ax)*(y+.5-ay)/(by-ay)+ax:inside=not inside
                if inside:pixels[y][x]=ink
    def line(points):
        for (ax,ay),(bx,by) in zip(points,points[1:]):
            n=max(abs(bx-ax),abs(by-ay))
            for i in range(n+1):
                x=round(ax+(bx-ax)*i/max(n,1));y=round(ay+(by-ay)*i/max(n,1))
                if 0<=x<48 and 0<=y<32:pixels[y][x]=1
    if team==2:
        for y in range(32):
            for x in range(48):
                r=(x-23.5)**2+(y-13)**2
                if 100<r<127:pixels[y][x]=1
        bull=[(3,14),(6,10),(11,9),(15,11),(18,13),(20,12),(22,15),(21,18),(18,18),(16,16),(13,17),(11,20),(7,22),(5,21),(9,18),(8,16),(6,17),(3,20),(1,19),(4,16)]
        for flip in (False,True):
            def pts(p):return [(47-x,y) for x,y in p] if flip else p
            polygon(pts(bull))
            for p in [[(5,11),(3,8),(1,9),(2,12)],[(20,14),(19,10),(22,12)],[(15,17),(17,21),(21,22)]]:line(pts(p))
    else:
        # Black cells transcribed from the user's 60x75 pixel-chart reference.
        # Preserve its tall shield; omit the coloured stripes in monochrome.
        source=(ROOT/'tools/ferrari_reference_mask.txt').read_text().splitlines()
        for y in range(30):
            for x in range(24):
                hits=0
                for sy in range(6):
                    for sx in range(6):
                        xx=min(59,int(3+(x+(sx+.5)/6)*54/24))
                        yy=min(74,int(7+(y+(sy+.5)/6)*68/30))
                        hits+=source[yy][xx]=='#'
                pixels[y+1][x+12]=int(hits>=11)
    return [sum(pixels[y][x+i]<<i for i in range(8)) for y in range(32) for x in range(0,48,8)]

def main():
    p=ROOT/'team_logos.c';s=p.read_text()
    for team in (2,3):
        data=badge(team)
        s=re.sub(r'    \{[^\n]+\}, /\* '+str(team)+r' \*/','    { '+','.join(f'0x{x:02X}' for x in data)+' }, /* '+str(team)+' */',s)
    p.write_text(s)
    data=[int(x,16) for x in re.findall(r'0x([0-9A-F]{2})',s)]
    w,h=48*6*2,32*6
    raw=bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            team=3 if x<w//2 else 2
            xx=(x%(w//2))//6;yy=y//6
            raw.append(0 if data[team*192+yy*6+xx//8]&(1<<(xx%8)) else 255)
    def chunk(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d))
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
    (ROOT/'assets/refined_badges.png').write_bytes(png)
if __name__=='__main__':main()
