#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mock_save_io.h"
#include "../tyre_model.h"
static void finish(GpRace* r,unsigned track){
    gp_race_start(r,0,0,track,123);r->lap=r->total_laps;
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){r->ranking[i]=i;r->drivers[i].time_ms=100000+i*1000;}
}
static void checksum(uint8_t* bytes,unsigned n){
    uint32_t crc=UINT32_MAX;for(unsigned i=0;i<n-4;i++){crc^=bytes[i];for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);}
    crc=~crc;for(unsigned i=0;i<4;i++)bytes[n-4+i]=(uint8_t)(crc>>(i*8));
}
int main(void){
    GpSavedGame a={0},b={0};gp_weekend_start(&a.weekend,false,123);finish(&a.race,0);
    a.weekend.stage=GpStageRace;a.weekend.phase=GpWeekendResult;gp_qualify(&a.weekend.qualifying,0,123);
    assert(gp_weekend_record(&a.weekend,&a.race));assert(!gp_weekend_record(&a.weekend,&a.race));
    assert(a.weekend.driver_points[0]==25&&a.weekend.driver_points[9]==1&&a.weekend.driver_points[10]==0);
    assert(a.weekend.team_points[0]==43&&a.weekend.driver_order[0]==0&&a.weekend.team_order[0]==0);
    uint8_t bytes[GP_SAVE_BYTES],bad[GP_SAVE_BYTES];assert(gp_save_encode(&a,bytes));
    assert(gp_save_decode(bytes,sizeof(bytes),&b)==GpSaveOk);
    assert(b.weekend.driver_points[0]==25&&!gp_weekend_record(&b.weekend,&b.race));
    for(unsigned i=640;i<=643;i++){
        memcpy(bad,bytes,sizeof(bad));bad[i]=255;checksum(bad,sizeof(bad));assert(gp_save_decode(bad,sizeof(bad),&b)==GpSaveCorrupt);
    }
    memcpy(bad,bytes,sizeof(bad));bad[645]=bad[644];checksum(bad,sizeof(bad));assert(gp_save_decode(bad,sizeof(bad),&b)==GpSaveCorrupt);
    /* Legacy v6 can resume, but never invents missing season results. */
    memcpy(bad,bytes,640);bad[4]=6;bad[324]=0;bad[325]=0; /* FP1, ready, zero completed rounds */
    checksum(bad,640);assert(gp_save_decode(bad,640,&b)==GpSaveOk);assert(b.race.mode==GpModeTeam);
    memcpy(bad,bytes,640);bad[4]=6;bad[323]=1;checksum(bad,640);
    assert(gp_save_decode(bad,640,&b)==GpSaveOk&&b.weekend.incomplete_history&&b.weekend.driver_points[0]==0);
    /* Each profile has its own two generations and failure recovery. */
    GpTestFs fs={0};GpSaveIo io=test_io(&fs);
    for(unsigned slot=0;slot<GP_SAVE_PROFILES;slot++){
        io.profile=slot;a.race.pace[0]=slot;assert(gp_save_store(&io,&a)==GpSaveOk);
        a.race.pace[0]=(slot+1)%3;assert(gp_save_store(&io,&a)==GpSaveOk);
    }
    for(unsigned slot=0;slot<GP_SAVE_PROFILES;slot++){
        io.profile=slot;assert(gp_save_load(&io,&b)==GpSaveOk&&b.sequence==2&&b.race.pace[0]==(GpPace)((slot+1)%3));
    }
    fs.data[4][30]^=1;io.profile=1;assert(gp_save_load(&io,&b)==GpSaveRecovered&&b.sequence==1);
    io.profile=0;assert(gp_save_load(&io,&b)==GpSaveOk&&b.sequence==2);io.profile=2;assert(gp_save_load(&io,&b)==GpSaveOk&&b.sequence==2);
    io.profile=3;assert(gp_save_store(&io,&a)==GpSaveIoError);
    /* Sprint scoring, DNF exclusion and countback use archived results. */
    GpWeekend w={0};gp_weekend_start(&w,false,1);unsigned sprint=0;while(!gp_weekend_sprint(&w,sprint))sprint++;
    finish(&a.race,sprint);w.stage=GpStageSprint;assert(gp_weekend_record(&w,&a.race));
    assert(w.driver_points[0]==8&&w.driver_points[7]==1&&w.driver_points[8]==0);
    w.results[sprint][1].finish[7]|=128;for(unsigned i=8;i<GP_DRIVER_COUNT;i++)w.results[sprint][1].finish[i]|=128;
    gp_championship_rebuild(&w);assert(w.driver_points[7]==0);
    gp_weekend_start(&w,false,1);
    for(unsigned t=0;t<2;t++){w.results[t][0].recorded=true;for(unsigned i=0;i<22;i++)w.results[t][0].finish[i]=i|128;}
    w.results[0][0].finish[0]=0;w.results[0][0].finish[1]=1;w.results[1][0].finish[0]=2;w.results[1][0].finish[1]=1;w.results[1][0].finish[2]=128;
    gp_championship_rebuild(&w);assert(w.driver_points[1]==36&&w.driver_order[0]==1);assert(w.driver_order[1]==0); // equal wins, roster tie-break
    gp_weekend_start(&w,false,1);
    const uint8_t first[]={1,2,0,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21};
    const uint8_t second[]={2,3,4,5,0,1,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21};
    for(unsigned i=0;i<22;i++){w.results[0][0].finish[i]=first[i]|(i>=3?128:0);w.results[1][0].finish[i]=second[i]|(i>=5?128:0);}
    w.results[0][0].recorded=w.results[1][0].recorded=true;gp_championship_rebuild(&w);
    assert(w.driver_points[0]==25&&w.driver_points[1]==25&&w.driver_order[1]==1&&w.driver_order[2]==0); // a win beats third+fifth
    /* Driver mode locks selection; the teammate makes their own pit/pace decisions. */
    for(unsigned slot=0;slot<2;slot++){
        GpRace r;gp_race_start(&r,0,slot,0,123);gp_race_set_mode(&r,GpModeDriver,slot);r.incident_rng=0;
        unsigned teammate=1-slot;gp_select_driver(&r,teammate);assert(r.active_slot==slot);
        r.weather=(GpWeather){0};r.drivers[teammate].wear=1;r.drivers[teammate].tyre=GpTyreMedium;r.lap=2;
        r.pace[teammate]=GpPaceAttack;assert(gp_driver_pace(&r,teammate)==GpPaceConserve);
        gp_advance_lap(&r);assert(r.drivers[teammate].pits==1&&r.drivers[slot].pits==0);
        GpQualifying q;gp_qualifying_start(&q,0,0,123);q.mode=GpModeDriver;q.controlled_slot=slot;
        q.weather=(GpWeather){0};q.tyre[teammate]=GpTyreSoft;q.tyre[slot]=GpTyreSoft;
        assert(gp_qualifying_run(&q)&&q.soft_used[slot]==1&&q.soft_used[teammate]==0);
        gp_set_event(&r,GpEventRed,0);r.pit_tyres[teammate]=GpTyreWet;r.pit_tyres[slot]=GpTyreHard;
        assert(gp_restart_race(&r)&&r.drivers[slot].tyre==GpTyreHard&&r.drivers[teammate].tyre!=GpTyreWet);
    }
    puts("PASS championship: race/sprint points, DNF, countback, idempotent results, archive corruption, v6 migration, three independent slots/recovery, driver-mode teammate AI and restart");
}
