#ifndef GP_TEAM_LOGOS_INCLUDED
#define GP_TEAM_LOGOS_INCLUDED
#include <stdint.h>
#define GP_TEAM_LOGO_W 48
#define GP_TEAM_LOGO_H 32
#define GP_TEAM_LOGO_ROW 6
#define GP_TEAM_LOGO_BYTES 192
const uint8_t* gp_logo_data(uint8_t team);
struct Canvas;
void gp_logo_draw(struct Canvas* c, uint8_t team, int16_t x, int16_t y);
#endif
