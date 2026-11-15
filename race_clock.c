#include "race_clock.h"
#include <string.h>

void gp_clock_reset(GpRaceClock* clock) {
    memset(clock, 0, sizeof(*clock));
}

uint32_t gp_clock_multiplier(const GpRaceClock* clock) {
    const uint8_t speeds[] = {1, 2, 5};
    return speeds[clock->speed_index % 3];
}

/* Tick units are supplied by Furi in the app. The clock is portable C. */
uint8_t gp_clock_step(GpRaceClock* clock, uint32_t dt_ticks, uint32_t lap_ticks) {
    if(clock->paused || !lap_ticks || !dt_ticks) return 0;
    /* Saturate extremely large stalls, preventing 32-bit overflow. */
    if(dt_ticks > lap_ticks * 16u) dt_ticks = lap_ticks * 16u;
    uint32_t inc = dt_ticks * gp_clock_multiplier(clock);
    uint32_t progressed = clock->progress_ticks + inc;
    uint32_t laps = progressed / lap_ticks;
    clock->progress_ticks = progressed % lap_ticks;
    return laps > 255 ? 255 : (uint8_t)laps;
}

uint8_t gp_clock_phase(const GpRaceClock* clock, uint32_t lap_ticks) {
    if(!lap_ticks) return 0;
    return (uint8_t)(((uint64_t)clock->progress_ticks * 255u) / lap_ticks);
}
