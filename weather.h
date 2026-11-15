#ifndef GP_WEATHER_H
#define GP_WEATHER_H
#include <stdbool.h>
#include <stdint.h>
typedef enum {GpRainNone,GpRainLight,GpRainHeavy} GpRain;
/* One step is one race lap or one attempt by the whole qualifying field.
   rng=0 is the frozen dry state used by legacy saves. */
typedef struct {uint8_t wetness, rain, forecast, remaining;uint32_t rng;} GpWeather;
void gp_weather_start(GpWeather*,uint32_t seed);
void gp_weather_step(GpWeather*);
bool gp_weather_valid(const GpWeather*);
const char* gp_rain_name(uint8_t rain);
/* Tyre IDs match GpTyre. Shared by race and qualifying simulations. */
int32_t gp_weather_penalty(const GpWeather*,unsigned tyre);
unsigned gp_weather_wear(const GpWeather*,unsigned tyre);
unsigned gp_weather_tyre(const GpWeather*);
#endif
