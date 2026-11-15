#ifndef GP_QUALIFYING_H
#define GP_QUALIFYING_H
#include "race_engine.h"
#define GP_QUAL_RUNS 2
#define GP_QUAL_SOFT_SETS 4
/* Two managed attempts per session; eliminated positions are frozen. */
typedef struct {
    uint8_t grid[GP_DRIVER_COUNT];
    uint32_t lap_ms[GP_DRIVER_COUNT];
    uint8_t track, team, session, run, active_count;
    uint32_t rng_state;
    GpTyre tyre[2];
    GpPace pace[2];
    uint8_t soft_used[2];
    bool complete;
    GpWeather weather;
    uint8_t temperature[GP_DRIVER_COUNT]; /* end temperature of best lap; zero before a run */
} GpQualifying;
void gp_qualifying_start(GpQualifying* q,uint8_t track,uint8_t team,uint32_t seed);
bool gp_qualifying_active(const GpQualifying* q,uint8_t driver);
bool gp_qualifying_run(GpQualifying* q);
bool gp_qualifying_next(GpQualifying* q);
/* Automatic weekend for simulation clients. */
void gp_qualify(GpQualifying* q,uint8_t track,uint32_t seed);
uint8_t gp_grid_position(const GpQualifying* q,uint8_t driver_id);
#endif
