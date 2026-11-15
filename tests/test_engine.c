#include "../race_engine.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    unsigned long races=0;
    for(uint8_t t=0;t<GP_TRACK_COUNT;++t) {
        for(uint8_t team=0;team<GP_TEAM_COUNT;++team) {
            for(uint8_t initial=0;initial<2;++initial) {
                GpRace r;
                gp_race_start(&r,team,initial,t,0xCAFEu+t*47u+team*11u+initial);r.incident_rng=0;
                assert(r.player==2*team+initial);
                assert(r.pace[0]==GpPaceNormal && r.pace[1]==GpPaceNormal);
                assert(r.lap==0);
                for(int lap=0;lap<GP_TOTAL_LAPS;++lap) {
                    if(lap==2) {
                        r.pace[0]=GpPaceAttack;
                        r.pace[1]=GpPaceConserve;
                        r.pit_queued[0]=true;
                        r.pit_tyres[0]=GpTyreSoft;
                    }
                    if(lap==5) {
                        r.pit_queued[1]=true;
                        r.pit_tyres[1]=GpTyreHard;
                    }
                    gp_advance_lap(&r);
                    assert(r.lap==lap+1);
                    int used[GP_DRIVER_COUNT]={0};
                    for(int k=0;k<GP_DRIVER_COUNT;++k) {
                        int id=r.ranking[k];
                        assert(id>=0 && id<GP_DRIVER_COUNT);
                        assert(!used[id]);used[id]=1;
                        assert(r.drivers[id].wear<=100);
                        assert(r.drivers[id].last_lap_ms>1000);
                        if(k>0)assert(r.drivers[r.ranking[k-1]].time_ms<=r.drivers[id].time_ms);
                    }
                    assert(r.drivers[2*team].pits>= (lap>=2?1:0));
                    assert(r.drivers[2*team+1].pits>= (lap>=5?1:0));
                }
                uint32_t end=r.drivers[0].time_ms;
                gp_advance_lap(&r);
                assert(r.lap==GP_TOTAL_LAPS);
                assert(r.drivers[0].time_ms==end);
                gp_select_driver(&r,1);
                assert(r.player==2*team+1);
                assert(gp_position(&r,r.player)>=1 && gp_position(&r,r.player)<=GP_DRIVER_COUNT);
                ++races;
            }
        }
    }
    printf("PASS: %lu race simulations, 12 laps, both pilots, pits, standings\n",races);
    return 0;
}
