#ifndef GP_TYRE_MODEL_H
#define GP_TYRE_MODEL_H
#include "race_engine.h"
#define GP_TEMPERATURE_MIN 20
#define GP_TEMPERATURE_MAX 140
/* Fictional balance parameters, temperatures expressed in Celsius. */
typedef struct {uint16_t loss_ms;uint8_t temperature,wear_extra;} GpTyreThermal;
uint8_t gp_tyre_optimal(GpTyre);
uint8_t gp_tyre_blanket(GpTyre);
int gp_tyre_temperature_state(GpTyre,uint8_t); /* -1 cold, 0 window, +1 hot */
GpTyreThermal gp_tyre_thermal_step(GpTyre,uint8_t,GpPace,const GpTrackInfo*,const GpWeather*,GpEvent);
#endif
