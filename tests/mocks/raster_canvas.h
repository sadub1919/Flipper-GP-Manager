/* Host bitmap canvas. Game fonts, logos and tracks are linked unchanged. */
#include <string.h>
#include <stdlib.h>
typedef struct Canvas { uint8_t pixels[64][128]; Color color; } Canvas;
static inline void canvas_clear(Canvas* c){memset(c->pixels,0,sizeof(c->pixels));}
static inline void canvas_set_color(Canvas* c,Color color){c->color=color;}
static inline void canvas_set_font(Canvas* c,Font f){UNUSED(c);UNUSED(f);}
static inline void canvas_draw_dot(Canvas* c,int x,int y){if(x>=0&&x<128&&y>=0&&y<64)c->pixels[y][x]=c->color==ColorBlack;}
static inline void canvas_draw_line(Canvas* c,int x,int y,int xx,int yy){
    int dx=abs(xx-x),sx=x<xx?1:-1,dy=-abs(yy-y),sy=y<yy?1:-1,err=dx+dy;
    for(;;){canvas_draw_dot(c,x,y);if(x==xx&&y==yy)break;int e=2*err;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
}
static inline void canvas_draw_frame(Canvas* c,int x,int y,int w,int h){
    canvas_draw_line(c,x,y,x+w-1,y);canvas_draw_line(c,x,y+h-1,x+w-1,y+h-1);
    canvas_draw_line(c,x,y,x,y+h-1);canvas_draw_line(c,x+w-1,y,x+w-1,y+h-1);
}
static inline void canvas_draw_disc(Canvas* c,int x,int y,int r){for(int yy=-r;yy<=r;yy++)for(int xx=-r;xx<=r;xx++)if(xx*xx+yy*yy<=r*r)canvas_draw_dot(c,x+xx,y+yy);}
static inline void canvas_draw_circle(Canvas* c,int x,int y,int r){for(int yy=-r;yy<=r;yy++)for(int xx=-r;xx<=r;xx++)if(xx*xx+yy*yy<=r*r&&xx*xx+yy*yy>(r-1)*(r-1))canvas_draw_dot(c,x+xx,y+yy);}
