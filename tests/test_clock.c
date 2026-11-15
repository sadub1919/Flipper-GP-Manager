#include "../race_clock.h"
#include "../race_engine.h"
#include "../track_shapes.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    GpRaceClock c;
    gp_clock_reset(&c);
    assert(gp_clock_multiplier(&c) == 1);
    assert(gp_clock_step(&c, 3000, 6000) == 0);
    assert(gp_clock_phase(&c, 6000) >= 126);
    assert(gp_clock_step(&c, 3000, 6000) == 1);
    assert(gp_clock_phase(&c, 6000) == 0);
    c.paused = true;
    assert(gp_clock_step(&c, 6000, 6000) == 0);
    c.paused = false;
    c.speed_index = 1;
    assert(gp_clock_multiplier(&c) == 2);
    assert(gp_clock_step(&c, 3000, 6000) == 1);
    c.speed_index = 2;
    assert(gp_clock_multiplier(&c) == 5);
    assert(gp_clock_step(&c, 1200, 6000) == 1);
    for(uint8_t track=0;track<GP_TRACK_COUNT;++track) {
        GpRace r;
        GpRaceClock clk;
        gp_clock_reset(&clk);
        gp_race_start(&r, 0, 0, track, track+123);
        for(uint8_t i=0;i<GP_TOTAL_LAPS;++i) {
            uint8_t n = gp_clock_step(&clk, 6000, 6000);
            assert(n == 1);
            gp_advance_lap(&r);
        }
        assert(r.lap==GP_TOTAL_LAPS);
    }
    puts("PASS live clock: pause, speed 1/2/5, 23 tracks, 12 laps");
    return 0;
}
