#ifndef GP_RU_FONT_H
#define GP_RU_FONT_H
#include <stdint.h>
#define GP_FONT_STEP 6
#define GP_FONT_HEIGHT 10
struct Canvas;
void gp_ru_text(struct Canvas*,int32_t,int32_t,const char*);
int32_t gp_ru_width(const char*);
#endif
