#include "../race_engine.h"
#include "../tyre_model.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    unsigned long scenarios=0;
    for(uint8_t track=0;track<GP_TRACK_COUNT;++track){
        for(uint8_t team=0;team<GP_TEAM_COUNT;++team){
            GpRace base,attack,save,pit,soft,repeat;
            gp_race_start(&base,team,0,track,5500u+team*13u+track);base.battle_rng=0;
            base.weather=(GpWeather){0};
            for(unsigned i=0;i<GP_DRIVER_COUNT;i++){base.drivers[i].tyre=GpTyreMedium;base.drivers[i].temperature=gp_tyre_optimal(GpTyreMedium);}
            attack=save=pit=soft=repeat=base;
            attack.pace[0]=GpPaceAttack;
            save.pace[0]=GpPaceConserve;
            pit.pit_queued[0]=true;pit.pit_tyres[0]=GpTyreSoft;
            soft.drivers[team*2].tyre=GpTyreSoft;
            gp_advance_lap(&base);gp_advance_lap(&attack);gp_advance_lap(&save);
            gp_advance_lap(&pit);gp_advance_lap(&soft);gp_advance_lap(&repeat);
            uint8_t id=2*team;
            assert(attack.drivers[id].last_lap_ms<base.drivers[id].last_lap_ms);
            assert(save.drivers[id].last_lap_ms>base.drivers[id].last_lap_ms);
            assert(attack.drivers[id].wear<base.drivers[id].wear);
            assert(save.drivers[id].wear>base.drivers[id].wear);
            assert(soft.drivers[id].last_lap_ms+1400==base.drivers[id].last_lap_ms);
            assert(pit.drivers[id].pits==1 && !pit.pit_queued[0]);
            assert(pit.drivers[id].tyre==GpTyreSoft);
            assert(pit.drivers[id].wear>0);
            assert(base.drivers[id].time_ms==repeat.drivers[id].time_ms);
            ++scenarios;
        }
    }
    printf("PASS: %lu deterministic strategy cases (attack/save, tyres, pit, repeatability)\n",scenarios);
}
