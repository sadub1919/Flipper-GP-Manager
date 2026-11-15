#ifndef GP_RACE_CLOCK_H
#define GP_RACE_CLOCK_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint32_t progress_ticks;
    uint8_t speed_index;
    bool paused;
} GpRaceClock;
void gp_clock_reset(GpRaceClock* clock);
uint32_t gp_clock_multiplier(const GpRaceClock* clock);
uint8_t gp_clock_step(GpRaceClock* clock, uint32_t dt_ticks, uint32_t lap_ticks);
uint8_t gp_clock_phase(const GpRaceClock* clock, uint32_t lap_ticks);
#endif
