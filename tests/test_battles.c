#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../gp_save.h"
#include "../tyre_model.h"
/* Isolate one faster player car catching a hard-shod rival, with a distant
   field. The actual lap simulation and classification run unchanged. */
static GpRace duel(unsigned track,unsigned slot,unsigned seed){
    GpRace r;gp_race_start(&r,0,slot,track,seed);r.lap=3;r.incident_rng=0;r.weather=(GpWeather){0};
    unsigned rank=0;r.ranking[rank++]=2;r.ranking[rank++]=slot;
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++)if(i!=2&&i!=slot)r.ranking[rank++]=i;
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){
        GpDriver* d=&r.drivers[r.ranking[i]];d->time_ms=100000+i*20000;d->tyre=GpTyreMedium;d->temperature=90;d->wear=100;
    }
    r.drivers[slot].time_ms=100500;r.drivers[slot].tyre=GpTyreSoft;r.drivers[slot].temperature=gp_tyre_optimal(GpTyreSoft);
    r.drivers[2].tyre=GpTyreHard;r.drivers[2].temperature=gp_tyre_optimal(GpTyreHard);r.drivers[2].wear=40;
    r.pace[slot]=GpPaceAttack;return r;
}
static void checksum(uint8_t* b){
    uint32_t crc=UINT32_MAX;for(unsigned i=0;i<(b[4]<7?640u:GP_SAVE_BYTES)-4;i++){crc^=b[i];for(unsigned j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);}
    crc=~crc;for(unsigned i=0;i<4;i++)b[(b[4]<7?640u:GP_SAVE_BYTES)-4+i]=(uint8_t)(crc>>(i*8));
}
int main(void){
    unsigned monaco=0,monza=0,eco=0,mistakes=0,blocked=0;
    for(unsigned seed=1;seed<=1000;seed++)for(unsigned slot=0;slot<2;slot++){
        GpRace narrow=duel(5,slot,seed),wide=duel(12,slot,seed),save=wide;
        save.pace[slot]=GpPaceConserve;
        gp_advance_lap(&narrow);gp_advance_lap(&wide);gp_advance_lap(&save);
        monaco+=gp_position(&narrow,slot)==1;monza+=gp_position(&wide,slot)==1;eco+=gp_position(&save,slot)==1;
        GpBattleReport b=narrow.battle[slot];assert(b.lap==4&&b.opponent==2);
        if(b.result==GpBattlePassed)assert(gp_position(&narrow,slot)==1);
        else {assert(gp_position(&narrow,slot)==2);assert(b.result==GpBattleBlocked||b.result==GpBattleMistake);blocked++;}
        mistakes+=b.result==GpBattleMistake;
        assert(narrow.drivers[slot].time_ms==100500+narrow.drivers[slot].last_lap_ms);
        assert(wide.drivers[slot].wear<save.drivers[slot].wear);
        for(unsigned i=1;i<GP_DRIVER_COUNT;i++)assert(!gp_driver_precedes(&narrow.drivers[narrow.ranking[i]],&narrow.drivers[narrow.ranking[i-1]]));
        GpSavedGame a={0},loaded={0};a.race=narrow;uint8_t bytes[GP_SAVE_BYTES],again[GP_SAVE_BYTES];
        assert(gp_save_encode(&a,bytes)&&gp_save_decode(bytes,(bytes[4]<7?640u:sizeof(bytes)),&loaded)==GpSaveOk);
        gp_advance_lap(&a.race);gp_advance_lap(&loaded.race);
        assert(gp_save_encode(&a,bytes)&&gp_save_encode(&loaded,again)&&!memcmp(bytes,again,sizeof(bytes)));
    }
    assert(monza>monaco+300&&monza>eco+300&&mistakes&&blocked);
    GpRace r=duel(5,0,123);r.drivers[2].wear=0;gp_advance_lap(&r);
    assert(r.drivers[2].pits==1&&gp_position(&r,0)==1&&r.battle[0].result==GpBattleNone);
    r=duel(5,0,123);r.pit_queued[0]=true;gp_advance_lap(&r);assert(r.battle[0].result==GpBattleNone);
    for(unsigned flag=GpEventYellow;flag<=GpEventSafetyCar;flag++){
        r=duel(12,0,123);r.drivers[2].wear=100;assert(gp_set_event(&r,flag,2));uint32_t rng=r.battle_rng;
        gp_advance_lap(&r);assert(gp_position(&r,0)==2&&r.battle[0].result==GpBattleNone&&r.battle_rng==rng);
    }
    r=duel(12,0,123);gp_retire_driver(&r,2,GpDriverFailure);gp_advance_lap(&r);
    assert(gp_position(&r,0)==1&&r.battle[0].result==GpBattleNone);
    r=duel(12,0,123);gp_set_event(&r,GpEventRed,0);GpRace stopped=r;gp_advance_lap(&r);assert(!memcmp(&r,&stopped,sizeof(r)));
    /* AI attacks/defends only with suitable tyres and a nearby competitor. */
    r=duel(12,0,123);r.drivers[2].wear=100;assert(gp_driver_pace(&r,2)==GpPaceAttack);
    r.drivers[2].wear=20;assert(gp_driver_pace(&r,2)==GpPaceConserve);
    r.drivers[2].wear=100;r.drivers[2].temperature=140;assert(gp_driver_pace(&r,2)==GpPaceConserve);
    r.drivers[2].temperature=90;r.drivers[0].time_ms+=10000;assert(gp_driver_pace(&r,2)==GpPaceNormal);
    /* v5 files have no battle state; migration keeps the old model until a new race. */
    GpSavedGame a={0},b={0};a.race=duel(5,0,123);uint8_t bytes[GP_SAVE_BYTES],bad[GP_SAVE_BYTES];assert(gp_save_encode(&a,bytes));
    bytes[4]=5;memset(bytes+624,0,10);checksum(bytes);assert(gp_save_decode(bytes,(bytes[4]<7?640u:sizeof(bytes)),&b)==GpSaveOk&&!b.race.battle_rng);
    gp_advance_lap(&a.race);assert(gp_save_encode(&a,bytes));
    unsigned offsets[]={628,629,630,631,632,633};
    for(unsigned i=0;i<6;i++){memcpy(bad,bytes,sizeof(bad));bad[offsets[i]]=255;checksum(bad);assert(gp_save_decode(bad,(bad[4]<7?640u:sizeof(bad)),&b)==GpSaveCorrupt);}
    printf("PASS battles: 2000 paired duels, passes Monaco=%u Monza=%u Eco=%u; %u blocked, %u attack mistakes; both pilots, AI, pits/DNF/flags, v5 migration, report validation and deterministic save continuation\n",monaco,monza,eco,blocked,mistakes);
}
