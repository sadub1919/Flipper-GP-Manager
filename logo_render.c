#include "team_logos.h"
#include <gui/canvas.h>
void gp_logo_draw(Canvas* c, uint8_t team, int16_t x, int16_t y){
 const uint8_t* data=gp_logo_data(team);
 for(uint8_t yy=0;yy<GP_TEAM_LOGO_H;++yy)
  for(uint8_t xx=0;xx<GP_TEAM_LOGO_W;++xx)
   if(data[yy*GP_TEAM_LOGO_ROW+xx/8]&(1u<<(xx%8)))canvas_draw_dot(c,x+xx,y+yy);
}
