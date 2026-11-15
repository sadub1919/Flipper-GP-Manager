#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../gp_save.h"
#include "../tyre_model.h"
static GpRace fixture(void){
    GpRace r;gp_race_start(&r,0,0,0,123);r.incident_rng=0;r.battle_rng=0;r.weather=(GpWeather){0};
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){r.drivers[i].tyre=GpTyreMedium;r.drivers[i].temperature=90;}
    gp_advance_lap(&r);return r;
}
static void roundtrip(GpRace* r){
    GpSavedGame a={0},b={0};a.race=*r;uint8_t x[GP_SAVE_BYTES],y[GP_SAVE_BYTES];
    assert(gp_save_encode(&a,x));assert(gp_save_decode(x,sizeof(x),&b)==GpSaveOk);
    assert(gp_save_encode(&b,y)&&!memcmp(x,y,sizeof(x)));
    if(r->event==GpEventRed){assert(gp_restart_race(&a.race));assert(gp_restart_race(&b.race));}
    gp_advance_lap(&a.race);gp_advance_lap(&b.race);
    assert(gp_save_encode(&a,x)&&gp_save_encode(&b,y)&&!memcmp(x,y,sizeof(x)));
}
static void checksum(uint8_t* b){
    uint32_t crc=UINT32_MAX;for(unsigned i=0;i<(b[4]<7?640u:GP_SAVE_BYTES)-4;i++){crc^=b[i];for(int j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);}
    crc=~crc;for(unsigned i=0;i<4;i++)b[(b[4]<7?640u:GP_SAVE_BYTES)-4+i]=(uint8_t)(crc>>(8*i));
}
int main(void){
    GpRace r=fixture();r.pit_queued[0]=true;
    assert(gp_retire_driver(&r,0,GpDriverAccident));assert(!r.pit_queued[0]);
    assert(!gp_retire_driver(&r,0,GpDriverFailure));
    GpDriver frozen=r.drivers[0];gp_advance_lap(&r);
    assert(!memcmp(&frozen,&r.drivers[0],sizeof(frozen)));
    assert(gp_retire_driver(&r,1,GpDriverFailure));
    assert(gp_position(&r,1)==21&&gp_position(&r,0)==22);
    assert(gp_pit_rejoin_position(&r,0)==0&&!gp_pit_traffic(&r,0));roundtrip(&r);
    r=fixture();uint8_t order[GP_DRIVER_COUNT];memcpy(order,r.ranking,sizeof(order));
    uint32_t gaps[GP_DRIVER_COUNT];for(unsigned i=0;i<GP_DRIVER_COUNT;i++)gaps[i]=gp_gap_ms(&r,i);
    assert(gp_set_event(&r,GpEventVsc,2));gp_advance_lap(&r);
    assert(r.event==GpEventVsc&&r.event_remaining==1);
    assert(!memcmp(order,r.ranking,sizeof(order)));
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++)assert(gp_gap_ms(&r,i)==gaps[i]);roundtrip(&r);
    gp_advance_lap(&r);assert(r.event==GpEventNone&&!r.event_remaining);
    assert(gp_set_event(&r,GpEventSafetyCar,2));gp_advance_lap(&r);
    assert(!memcmp(order,r.ranking,sizeof(order)));
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++)assert(gp_gap_ms(&r,i)<=gaps[i]);
    assert(gp_retire_driver(&r,2,GpDriverAccident));frozen=r.drivers[2];
    assert(gp_set_event(&r,GpEventRed,0));GpRace stopped=r;
    gp_advance_lap(&r);assert(!memcmp(&r,&stopped,sizeof(r)));roundtrip(&r);
    r.pit_tyres[0]=GpTyreIntermediate;r.pit_tyres[1]=GpTyreWet;
    uint8_t p0=r.drivers[0].pits,p1=r.drivers[1].pits,lap=r.lap;
    assert(gp_restart_race(&r)&&r.event==GpEventSafetyCar&&r.event_remaining==1);
    assert(r.lap==lap&&!r.incident_pending&&!memcmp(&frozen,&r.drivers[2],sizeof(frozen)));
    assert(r.drivers[0].tyre==GpTyreIntermediate&&r.drivers[1].tyre==GpTyreWet);
    assert(r.drivers[0].wear==100&&r.drivers[1].wear==100&&r.drivers[0].pits==p0&&r.drivers[1].pits==p1);
    gp_advance_lap(&r);assert(r.event==GpEventNone&&r.lap==lap+1);
    r=fixture();for(unsigned i=0;i<GP_DRIVER_COUNT;i++)assert(gp_retire_driver(&r,i,GpDriverFailure));
    assert(gp_race_finished(&r)&&gp_running_count(&r)==0);gp_advance_lap(&r);assert(r.lap==1);roundtrip(&r);
    /* Semantic corruption, even with a valid CRC, and v4 migration. */
    GpSavedGame a={0},b={0};a.race=fixture();uint8_t bytes[GP_SAVE_BYTES],bad[GP_SAVE_BYTES];assert(gp_save_encode(&a,bytes));
    unsigned offsets[]={572,573,620,621,622,623};
    for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++){
        memcpy(bad,bytes,sizeof(bad));bad[offsets[i]]=255;checksum(bad);assert(gp_save_decode(bad,(bad[4]<7?640u:sizeof(bad)),&b)==GpSaveCorrupt);
    }
    bytes[4]=4;memset(bytes+572,0,62);checksum(bytes);assert(gp_save_decode(bytes,(bytes[4]<7?640u:sizeof(bytes)),&b)==GpSaveOk);
    assert(!b.race.incident_rng&&b.race.incident_driver==GP_DRIVER_COUNT&&gp_running_count(&b.race)==GP_DRIVER_COUNT);
    unsigned crashes=0,failures=0,reds=0,sc=0,vsc=0;
    for(unsigned seed=1;seed<=3000;seed++){
        gp_race_start(&r,seed%GP_TEAM_COUNT,seed%2,seed%GP_TRACK_COUNT,seed);
        if(seed%2)r.total_laps=gp_track_info(r.track)->actual_laps;
        unsigned steps=0;
        while(!gp_race_finished(&r)){
            assert(++steps<100);
            if(r.event==GpEventRed){reds++;roundtrip(&r);assert(gp_restart_race(&r));}
            uint8_t serial=r.incident_serial;gp_advance_lap(&r);
            if(r.incident_serial!=serial){
                if(r.drivers[r.incident_driver].status==GpDriverAccident)crashes++;else failures++;
                sc+=r.event==GpEventSafetyCar;vsc+=r.event==GpEventVsc;roundtrip(&r);
            }
            for(unsigned i=1;i<GP_DRIVER_COUNT;i++)assert(!gp_driver_precedes(&r.drivers[r.ranking[i]],&r.drivers[r.ranking[i-1]]));
        }
        assert(r.lap==r.total_laps||!gp_running_count(&r));
    }
    assert(crashes&&failures&&reds&&sc&&vsc);
    printf("PASS: DNF freeze/order, SC/VSC duration/gaps, red restart/tyres, all-DNF finish, v4 migration and corruption; 3000 seeded races: %u accidents, %u failures, %u red stops, %u SC, %u VSC\n",crashes,failures,reds,sc,vsc);
}
