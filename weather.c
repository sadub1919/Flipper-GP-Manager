#include "weather.h"
static uint32_t next(uint32_t* s){uint32_t x=*s;x^=x<<13;x^=x>>17;x^=x<<5;return *s=x;}
static uint8_t rain(uint32_t* s){unsigned n=next(s)%100;return n<55?GpRainNone:(n<85?GpRainLight:GpRainHeavy);}
void gp_weather_start(GpWeather* w,uint32_t seed){
    *w=(GpWeather){0};w->rng=seed?seed:0x7002026u;
    w->rain=rain(&w->rng);w->wetness=w->rain==GpRainNone?0:(w->rain==GpRainLight?40:80);
    w->forecast=rain(&w->rng);w->remaining=2+next(&w->rng)%3;
}
void gp_weather_step(GpWeather* w){
    if(!w->rng)return;
    int wet=w->wetness+(w->rain==GpRainHeavy?25:(w->rain==GpRainLight?12:-15));
    w->wetness=wet<0?0:(wet>100?100:wet);
    if(--w->remaining==0){w->rain=w->forecast;w->forecast=rain(&w->rng);w->remaining=2+next(&w->rng)%3;}
}
bool gp_weather_valid(const GpWeather* w){
    if(!w->rng)return !w->wetness&&!w->rain&&!w->forecast&&!w->remaining;
    return w->wetness<=100&&w->rain<=GpRainHeavy&&w->forecast<=GpRainHeavy&&w->remaining>=1&&w->remaining<=4;
}
const char* gp_rain_name(uint8_t r){return r==GpRainHeavy?"ЛИВЕНЬ":(r==GpRainLight?"ДОЖДЬ":"БЕЗ ДОЖДЯ");}
int32_t gp_weather_penalty(const GpWeather* w,unsigned tyre){
    int n=w->wetness,loss=0;
    if(tyre<=2)loss=n>20?(n-20)*350:0;
    else if(tyre==3)loss=1000+(n<25?(25-n)*180:0)+(n>65?(n-65)*260:0);
    else loss=3000+(n<65?(65-n)*200:0);
    return loss+n*80;
}
unsigned gp_weather_wear(const GpWeather* w,unsigned tyre){
    if(tyre==3&&w->wetness<40)return (40-w->wetness)/3;
    if(tyre==4&&w->wetness<65)return (65-w->wetness)/3;
    return tyre<=2&&w->wetness>20?3:0;
}
unsigned gp_weather_tyre(const GpWeather* w){return w->wetness>=70?4:(w->wetness>=25?3:1);}
