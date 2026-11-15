#!/usr/bin/env python3
"""Pixel glyph authoring on build workstation; exported C resource only, no font file bundled.
Glyphs are rasterized from an installed SIL OFL font (DejaVu Sans Mono Bold).
"""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
BASE=Path(__file__).resolve().parent.parent
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf'
f=ImageFont.truetype(FONT,9)
chars=''.join(chr(i) for i in range(32,127))+''.join(chr(i) for i in range(0x410,0x450))+'Ёё'
# 6x10 matrix, x-step 6. Lowercase rendered as uppercase for game aesthetic.
chars= ''.join(chr(i) for i in range(32,127))+''.join(chr(i) for i in range(0x410,0x430))+'Ё'
rows=[]
for ch in chars:
 im=Image.new('L',(6,10),0)
 d=ImageDraw.Draw(im)
 d.text((0,-1),ch,font=f,fill=255)
 packed=[]
 for y in range(10):
  bits=sum((1<<x) for x in range(6) if im.getpixel((x,y))>=110)
  packed.append(bits)
 if ch=="Ё": packed[0]=0x12  # visible two dots
 if ch=="~": packed=[0,0,0,0,0x13,0x0c,0,0,0,0]  # readable approximate-position sign
 rows.append(packed)
text=['#include "ru_font.h"','#include <gui/canvas.h>','/* Independently embedded 6x10 pixel glyphs; only C bitmap data is distributed. */',f'static const unsigned char glyph_ascii[{95}][10] = {{']
for ch,arr in zip(chars[:95],rows[:95]):
 text.append('    {'+','.join(f'0x{x:02x}' for x in arr)+'}, /* U+%04X */'%ord(ch))
text.append('};')
text.append('static const unsigned char glyph_ru[33][10] = {')
for ch,arr in zip(chars[95:],rows[95:]):
 text.append('    {'+','.join(f'0x{x:02x}' for x in arr)+'}, /* U+%04X */'%ord(ch))
text.append('};')
text.extend('''
static unsigned int utf8_next(const unsigned char** in){
    const unsigned char* p=*in; unsigned int a=*p++;
    if(a<128){*in=p;return a;}
    if((a&0xe0)==0xc0 && (*p&0xc0)==0x80){
       unsigned int result=((a&0x1f)<<6)|(*p++&0x3f); *in=p;return result;
    }
    *in=p;return '?';
}
static const unsigned char* glyph(unsigned int c){
    if(c>=0x430&&c<=0x44f)c-=0x20;
    if(c==0x451)c=0x401;
    if(c>='a'&&c<='z')c-=32;
    if(c>=32&&c<=126)return glyph_ascii[c-32];
    if(c>=0x410&&c<=0x42f)return glyph_ru[c-0x410];
    if(c==0x401)return glyph_ru[32];
    return glyph_ascii['?'-32];
}
void gp_ru_text(struct Canvas* c,int32_t x,int32_t y,const char* text){
    if(!text)return;
    const unsigned char* p=(const unsigned char*)text;
    while(*p && x<128){
      const unsigned char* g=glyph(utf8_next(&p));
      for(int dy=0;dy<10;++dy)
        for(int dx=0;dx<6;++dx)
          if((g[dy]>>dx)&1 && x+dx<128 && y+dy<64 && x+dx>=0 && y+dy>=0)
              canvas_draw_dot(c,x+dx,y+dy);
      x+=6;
    }
}
int32_t gp_ru_width(const char* s){
   int32_t n=0;if(!s)return 0;
   const unsigned char* p=(const unsigned char*)s;
   while(*p){utf8_next(&p);n+=6;}return n;
}
'''.splitlines())
(BASE/'ru_font.c').write_text('\n'.join(text)+'\n',encoding='utf8')
(BASE/'ru_font.h').write_text('''#ifndef GP_RU_FONT_H
#define GP_RU_FONT_H
#include <stdint.h>
#define GP_FONT_STEP 6
#define GP_FONT_HEIGHT 10
struct Canvas;
void gp_ru_text(struct Canvas*,int32_t,int32_t,const char*);
int32_t gp_ru_width(const char*);
#endif
''')
print('Generated glyphs:',len(rows))
