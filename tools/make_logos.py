#!/usr/bin/env python3
"""11 monochrome logo silhouette adaptations, redrawn from 2026 team reference marks.
We distribute simplified fan-art pixel adaptations (not licensed vector originals).
At 48x32 they fit Flipper's team select screen with true team identities.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont,ImageOps
BASE=Path(__file__).resolve().parent.parent
W,H=48,32
S=4

def run(team):
    im=Image.new('L',(W*S,H*S),0)
    d=ImageDraw.Draw(im)
    def P(points): return [(round(x*S),round(y*S)) for x,y in points]
    def poly(points,fill=255): d.polygon(P(points),fill=fill)
    def line(points,width=2):d.line(P(points),fill=255,width=round(width*S),joint='curve')
    def ring(box,width=2):d.ellipse(tuple(round(v*S) for v in box),outline=255,width=round(width*S))
    def rect(box,fill=255):d.rectangle(tuple(round(v*S) for v in box),fill=fill)
    if team==0:  # McLaren actual speedmark: asymmetric slanted comma/wing
        poly([(4,21),(14,12),(28,8),(44,4),(39,13),(29,20),(16,25),(7,25),(22,21),(33,16),(37,12),(24,16),(14,20)])
        poly([(19,24),(25,22),(31,21),(34,23),(28,27),(20,28)],0)
    elif team==1: # Mercedes-Benz three pointed star
        ring((8,1,39,31),2.4)
        poly([(24,4),(22,15),(24,17),(26,15)])
        poly([(10,24),(22,15),(25,18),(19,27)])
        poly([(38,24),(25,15),(22,18),(29,27)])
        ring((8,1,39,31),1.25)
    elif team in (2,3):
        from refine_badges import badge
        data=badge(team)
        small=Image.new('1',(W,H))
        for y in range(H):
            for x in range(W):small.putpixel((x,y),255 if data[y*6+x//8]&(1<<(x%8)) else 0)
        return small
    elif team==4: # Williams - stylized multiple W diagonal strokes
        poly([(3,4),(13,4),(18,22),(23,12),(28,22),(34,4),(45,4),(32,30),(27,30),(23,21),(19,30),(13,30)])
        poly([(16,4),(23,4),(26,10),(23,17)],255)
    elif team==5: # Racing Bulls 2026 RB letter monogram w/energy diagonals
        # racing horn silhouette plus italic 'RB'  identity
        poly([(1,8),(12,11),(18,16),(14,16),(8,13),(6,17)])
        poly([(47,8),(36,11),(30,16),(34,16),(40,13),(42,17)])
        # italic R and B
        poly([(9,10),(19,10),(22,14),(20,19),(16,20),(22,27),(16,27),(10,20),(10,27),(5,27)])
        poly([(11,14),(16,14),(17,17),(11,17)],0)
        poly([(25,10),(35,10),(39,13),(37,18),(40,22),(37,27),(23,27)])
        poly([(29,14),(34,14),(35,17),(29,17)],0)
        poly([(29,20),(35,20),(36,23),(29,23)],0)
    elif team==6: # Aston Martin real wing crest with layered feathers
        poly([(1,8),(18,8),(24,11),(30,8),(47,8),(44,16),(30,21),(27,25),(21,25),(18,21),(4,16)])
        poly([(3,11),(19,11),(24,15),(29,11),(45,11),(42,15),(30,18),(27,22),(21,22),(18,18),(6,15)],0)
        for i in range(4):
            line([(4+i*4,12),(9+i*3,18)],1)
            line([(44-i*4,12),(39-i*3,18)],1)
        line([(20,12),(28,12)],2)
        rect((20,15,28,20))
        rect((22,17,26,18),0)
    elif team==7: # Haas circular slashed H monogram (current Haas mark)
        ring((10,1,39,30),2.2)
        poly([(15,6),(19,6),(18,24),(14,24)])
        poly([(28,6),(32,6),(31,24),(27,24)])
        poly([(17,13),(30,13),(30,17),(17,17)])
        poly([(34,4),(38,5),(34,27),(30,28)])
    elif team==8: # Audi current four overlapping rings
        for i in range(4):ring((2+i*10,10,18+i*10,25),2.1)
    elif team==9: # Alpine angular A / slashed wing mark
        poly([(24,3),(4,28),(11,28),(24,13),(35,28),(44,28),(28,3)])
        poly([(18,22),(33,22),(29,18),(22,18)],0)
        poly([(32,3),(42,3),(47,9),(39,11)],255)
    elif team==10: # Cadillac heraldic crest / horizontal stripes (2026)
        poly([(7,4),(41,4),(40,19),(24,29),(8,19)])
        poly([(10,7),(38,7),(37,18),(24,26),(11,18)],0)
        for i in range(3):
            rect((12,9+i*4,36,10+i*4))
        for x in (14,21,28):rect((x,13,x+3,20))
        rect((12,6,36,7))
    # high resolution to 1-bit, anti alias then threshold; preserve thin strokes
    small=im.resize((W,H),Image.Resampling.LANCZOS).point(lambda v: 255 if v>=75 else 0,'1')
    return small

imgs=[run(t) for t in range(11)]
ROW=(W+7)//8
arrays=[]
for im in imgs:
    p=im.load(); arr=[]
    for y in range(H):
        for byte in range(ROW):
            arr.append(sum(1<<i for i in range(8) if byte*8+i<W and p[byte*8+i,y]))
    arrays.append(arr)
(BASE/'team_logos.h').write_text(f'''#ifndef GP_TEAM_LOGOS_INCLUDED
#define GP_TEAM_LOGOS_INCLUDED
#include <stdint.h>
#define GP_TEAM_LOGO_W {W}
#define GP_TEAM_LOGO_H {H}
#define GP_TEAM_LOGO_ROW {ROW}
#define GP_TEAM_LOGO_BYTES {ROW*H}
const uint8_t* gp_logo_data(uint8_t team);
struct Canvas;
void gp_logo_draw(struct Canvas* c, uint8_t team, int16_t x, int16_t y);
#endif
''')
content=['#include "team_logos.h"','/* 2026 team logo silhouette adaptations, 48x32 monochrome. */',f'static const uint8_t pixels[11][{ROW*H}] = {{']
for t,array in enumerate(arrays):content.append('    { '+','.join(f'0x{x:02X}' for x in array)+' }, /* '+str(t)+' */')
content.extend(['};','const uint8_t* gp_logo_data(uint8_t team){return pixels[team%11];}'])
(BASE/'team_logos.c').write_text('\n'.join(content)+'\n')
(BASE/'logo_render.c').write_text('''#include "team_logos.h"
#include <gui/canvas.h>
void gp_logo_draw(Canvas* c, uint8_t team, int16_t x, int16_t y){
 const uint8_t* data=gp_logo_data(team);
 for(uint8_t yy=0;yy<GP_TEAM_LOGO_H;++yy)
  for(uint8_t xx=0;xx<GP_TEAM_LOGO_W;++xx)
   if(data[yy*GP_TEAM_LOGO_ROW+xx/8]&(1u<<(xx%8)))canvas_draw_dot(c,x+xx,y+yy);
}
''')
CAN_W=220;CAN_H=130
preview=Image.new('RGB',(CAN_W*4,CAN_H*3),'white')
font=ImageFont.truetype('DejaVuSans.ttf',16)
names=['McLaren','Mercedes','Red Bull','Ferrari','Williams','Racing Bulls','Aston Martin','Haas','Audi','Alpine','Cadillac']
for i,im in enumerate(imgs):
    xx=i%4*CAN_W;yy=i//4*CAN_H
    icon=im.convert('L'); icon=ImageOps.invert(icon)
    icon=icon.resize((W*3,H*3),Image.Resampling.NEAREST)
    preview.paste(icon,(xx+(CAN_W-W*3)//2, yy+3))
    ImageDraw.Draw(preview).text((xx+17,yy+101),names[i],font=font,fill='black')
preview.save(BASE/'assets/logos_preview.png')
print('Recreated 11 references-informed 48x32 team marks')
