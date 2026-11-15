/* Same pit policy, dry weather and no retirements isolate pace/tyre tradeoffs.
   Battles and opponent AI remain enabled; results are scenario-specific. */
#include "race_engine.h"
#include "tyre_model.h"
#include <stdio.h>
#include <assert.h>
static unsigned run(unsigned seed,unsigned track,bool full,GpPace pace){
    GpRace r;gp_race_start(&r,0,0,track,seed);
    r.weather=(GpWeather){0};r.incident_rng=0;
    r.total_laps=full?gp_track_info(track)->actual_laps:GP_TOTAL_LAPS;r.pace[0]=pace;
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){
        r.drivers[i].tyre=GpTyreMedium;r.drivers[i].temperature=gp_tyre_optimal(GpTyreMedium);
    }
    while(!gp_race_finished(&r)){
        if(r.drivers[0].wear<25){r.pit_queued[0]=true;r.pit_tyres[0]=GpTyreMedium;}
        gp_advance_lap(&r);
    }
    return r.drivers[0].time_ms;
}
int main(void){
    unsigned norm=0,eco=0,attack=0;
    for(unsigned seed=1;seed<=30;seed++)for(unsigned t=0;t<GP_TRACK_COUNT;t++)for(unsigned full=0;full<2;full++){
        unsigned a=run(seed,t,full,GpPaceAttack),n=run(seed,t,full,GpPaceNormal),e=run(seed,t,full,GpPaceConserve);
        norm+=n<a;eco+=e<a;attack+=a<n&&a<e;
    }
    assert(norm&&eco&&attack);
    printf("PASS balance: 1380 paired track/distance/seed scenarios; normal beats constant attack in %u, conserve in %u, attack wins both in %u\n",norm,eco,attack);
}
