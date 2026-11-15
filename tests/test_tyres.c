#include "../gp_save.h"
#include "../tyre_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void checksum(uint8_t* b){
    uint32_t crc=UINT32_MAX;for(unsigned i=0;i<GP_SAVE_BYTES-4;i++){crc^=b[i];for(int j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);}
    crc=~crc;for(int i=0;i<4;i++)b[GP_SAVE_BYTES-4+i]=(uint8_t)(crc>>(8*i));
}
static GpRace scenario(unsigned seed,unsigned wear,GpTyre tyre){
    GpRace r;gp_race_start(&r,0,1,0,seed);r.incident_rng=0;r.weather=(GpWeather){0};r.lap=3;
    for(unsigned i=0;i<22;i++){
        r.ranking[i]=i;r.drivers[i].time_ms=i<2?i*800:50000+i*10000;
        r.drivers[i].wear=i<2?wear:100;r.drivers[i].tyre=tyre;r.drivers[i].temperature=gp_tyre_optimal(tyre);
    }
    return r;
}
int main(void){
    unsigned combinations=0,undercuts=0,overcuts=0;
    for(unsigned tyre=0;tyre<GpTyreCount;tyre++)for(unsigned temperature=20;temperature<=140;temperature++)
    for(unsigned pace=0;pace<3;pace++)for(unsigned wet=0;wet<=100;wet+=25)for(unsigned event=0;event<4;event++){
        GpWeather w={wet,0,0,3,123};
        GpTyreThermal t=gp_tyre_thermal_step(tyre,temperature,pace,gp_track_info(0),&w,event);
        assert(t.temperature>=20&&t.temperature<=140&&t.loss_ms<20000&&t.wear_extra<=20);combinations++;
    }
    GpWeather dry={0};
    uint8_t soft=gp_tyre_blanket(GpTyreSoft),hard=gp_tyre_blanket(GpTyreHard);
    unsigned soft_loss=0,hard_loss=0;
    for(unsigned lap=0;lap<3;lap++){
        GpTyreThermal s=gp_tyre_thermal_step(GpTyreSoft,soft,GpPaceNormal,gp_track_info(0),&dry,GpEventNone);
        GpTyreThermal h=gp_tyre_thermal_step(GpTyreHard,hard,GpPaceNormal,gp_track_info(0),&dry,GpEventNone);
        soft_loss+=s.loss_ms;hard_loss+=h.loss_ms;soft=s.temperature;hard=h.temperature;
        if(lap==0)assert(s.loss_ms>0&&h.loss_ms>s.loss_ms);
    }
    assert(hard_loss>soft_loss&&soft>hard);
    uint8_t temp=95;GpTyreThermal heat={0};
    for(unsigned lap=0;lap<5;lap++){heat=gp_tyre_thermal_step(GpTyreSoft,temp,GpPaceAttack,gp_track_info(5),&dry,GpEventNone);temp=heat.temperature;}
    assert(gp_tyre_temperature_state(GpTyreSoft,temp)>0&&heat.loss_ms>0&&heat.wear_extra>0);
    for(unsigned lap=0;lap<3;lap++){heat=gp_tyre_thermal_step(GpTyreSoft,temp,GpPaceConserve,gp_track_info(5),&dry,GpEventNone);temp=heat.temperature;}
    assert(gp_tyre_temperature_state(GpTyreSoft,temp)==0);
    heat=gp_tyre_thermal_step(GpTyreSoft,95,GpPaceNormal,gp_track_info(0),&dry,GpEventSafetyCar);assert(heat.temperature<95);
    /* Equal pit counts, identical random draws: compare early and late stops. */
    for(unsigned seed=1;seed<=100;seed++)for(unsigned worn=0;worn<2;worn++){
        GpRace early=scenario(seed,worn?10:90,worn?GpTyreMedium:GpTyreHard),late=early;
        for(unsigned step=0;step<2;step++){
            early.event=late.event=GpEventNone;
            early.pit_queued[1]=step==0;late.pit_queued[1]=step==1;
            early.pit_queued[0]=late.pit_queued[0]=step==(worn?1:0);
            early.pit_tyres[0]=early.pit_tyres[1]=late.pit_tyres[0]=late.pit_tyres[1]=worn?GpTyreMedium:GpTyreHard;
            gp_advance_lap(&early);gp_advance_lap(&late);
        }
        assert(early.drivers[1].pits==1&&late.drivers[1].pits==1);
        if(worn){assert(early.drivers[1].time_ms<late.drivers[1].time_ms);if(gp_position(&early,1)<gp_position(&late,1))undercuts++;}
        else{assert(late.drivers[1].time_ms<early.drivers[1].time_ms);if(gp_position(&late,1)<gp_position(&early,1))overcuts++;}
    }
    assert(undercuts>0&&overcuts>0);
    GpRace clear=scenario(123,100,GpTyreMedium),traffic=clear;
    /* Driver zero is processed before our driver: only previous times may be used. */
    traffic.drivers[0].time_ms=traffic.drivers[1].time_ms+gp_pit_loss_ms(&traffic)-500;
    traffic.ranking[0]=1;traffic.ranking[1]=0;
    assert(!gp_pit_traffic(&clear,1)&&gp_pit_traffic(&traffic,1));
    assert(gp_pit_rejoin_position(&traffic,1)==2);
    clear.pit_queued[1]=traffic.pit_queued[1]=true;clear.pit_tyres[1]=traffic.pit_tyres[1]=GpTyreMedium;
    gp_advance_lap(&clear);gp_advance_lap(&traffic);
    assert(traffic.drivers[1].last_lap_ms==clear.drivers[1].last_lap_ms+600+gp_track_info(0)->difficulty*300);
    /* Preserve every driver's temperature and the next simulation step. */
    for(unsigned tyre=0;tyre<5;tyre++)for(unsigned temperature=20;temperature<=140;temperature+=10){
        GpSavedGame a={0},b={0};gp_race_start(&a.race,0,0,0,123);a.race.incident_rng=0;
        for(unsigned i=0;i<22;i++){a.race.drivers[i].tyre=tyre;a.race.drivers[i].temperature=temperature;}
        uint8_t bytes[GP_SAVE_BYTES],other[GP_SAVE_BYTES];assert(gp_save_encode(&a,bytes));assert(gp_save_decode(bytes,sizeof(bytes),&b)==GpSaveOk);
        gp_advance_lap(&a.race);gp_advance_lap(&b.race);assert(gp_save_encode(&a,bytes)&&gp_save_encode(&b,other));assert(!memcmp(bytes,other,sizeof(bytes)));
        for(unsigned offset=528;offset<550;offset++){
            memcpy(other,bytes,sizeof(bytes));other[offset]=0;checksum(other);assert(gp_save_decode(other,sizeof(other),&b)==GpSaveCorrupt);
            other[offset]=141;checksum(other);assert(gp_save_decode(other,sizeof(other),&b)==GpSaveCorrupt);
        }
        // Version 3 migration: existing race starts with tyres in their working window.
        bytes[4]=3;memset(bytes+528,0,96);checksum(bytes);assert(gp_save_decode(bytes,sizeof(bytes),&b)==GpSaveOk);
        for(unsigned i=0;i<22;i++)assert(b.race.drivers[i].temperature==gp_tyre_optimal(b.race.drivers[i].tyre));
    }
    GpSavedGame a={0},b={0};gp_race_start(&a.race,0,0,0,123);a.race.incident_rng=0;gp_weekend_start(&a.weekend,false,1);
    a.weekend.stage=GpStageFP1;a.weekend.phase=GpWeekendQualResult;
    GpQualifying* q=&a.weekend.qualifying;gp_qualifying_start(q,0,0,123);q->weather=(GpWeather){0};q->tyre[0]=GpTyreSoft;
    gp_qualifying_run(q);uint32_t best=q->lap_ms[0];uint8_t best_temp=q->temperature[0];q->tyre[0]=GpTyreHard;
    gp_qualifying_run(q);assert(q->lap_ms[0]==best&&q->temperature[0]==best_temp);
    uint8_t bytes[GP_SAVE_BYTES];assert(gp_save_encode(&a,bytes));assert(gp_save_decode(bytes,sizeof(bytes),&b)==GpSaveOk);
    assert(!memcmp(q->temperature,b.weekend.qualifying.temperature,sizeof(q->temperature)));
    bytes[550]=141;checksum(bytes);assert(gp_save_decode(bytes,sizeof(bytes),&b)==GpSaveCorrupt);
    printf("PASS: %u thermal combinations; cold outlaps, overheating/cooling, traffic snapshot, v3 migration and temperature saves; %u/100 undercuts and %u/100 overcuts gain a position\n",combinations,undercuts,overcuts);
}
