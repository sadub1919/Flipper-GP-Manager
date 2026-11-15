#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mock_save_io.h"
#include "../tyre_model.h"
static void same(const GpSavedGame* a,const GpSavedGame* b){uint8_t x[GP_SAVE_BYTES],y[GP_SAVE_BYTES];assert(gp_save_encode(a,x));assert(gp_save_encode(b,y));assert(!memcmp(x,y,sizeof(x)));}
static void commands(GpRace* r){
    for(int slot=0;slot<2;slot++){
        r->pace[slot]=(GpPace)((r->lap+slot)%3);
        if(r->drivers[r->team*2+slot].wear<35){r->pit_queued[slot]=true;r->pit_tyres[slot]=(GpTyre)((r->lap+slot)%3);}
    }
}
static void rechecksum(uint8_t* b){
    uint32_t crc=UINT32_MAX;for(unsigned i=0;i<GP_SAVE_BYTES-4;i++){crc^=b[i];for(int j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);}crc=~crc;
    for(int i=0;i<4;i++)b[GP_SAVE_BYTES-4+i]=(uint8_t)(crc>>(8*i));
}
int main(void){
    unsigned snapshots=0;
    for(int full=0;full<2;full++)for(int track=0;track<GP_TRACK_COUNT;track++)for(int team=0;team<GP_TEAM_COUNT;team++){
        GpSavedGame a={0};gp_race_start(&a.race,team,1,track,9876u+track+team);a.race.battle_rng=0;a.race.incident_rng=0;
        if(full)a.race.total_laps=gp_track_info(track)->actual_laps;
        uint8_t total=a.race.total_laps;
        for(int lap=0;lap<=total;lap++){
            a.clock.progress_ticks=(lap*131u)%GP_LAP_MS;a.clock.speed_index=lap%3;a.clock.paused=lap%2;
            commands(&a.race);
            uint8_t bytes[GP_SAVE_BYTES];assert(gp_save_encode(&a,bytes));GpSavedGame b={0};assert(gp_save_decode(bytes,sizeof(bytes),&b)==GpSaveOk);same(&a,&b);
            gp_advance_lap(&a.race);gp_advance_lap(&b.race);same(&a,&b);snapshots++;
        }
        assert(a.race.lap==total);
        assert(a.race.drivers[team*2].pits>0);
    }
    GpSavedGame a={0},b={0},sentinel={0};gp_race_start(&a.race,3,1,5,2345);a.race.battle_rng=0;a.race.incident_rng=0;a.race.total_laps=78;a.clock.progress_ticks=3456;a.race.weather=(GpWeather){0};
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){a.race.drivers[i].tyre=GpTyreMedium;a.race.drivers[i].temperature=gp_tyre_optimal(GpTyreMedium);}
    a.race.pit_tyres[0]=a.race.pit_tyres[1]=GpTyreMedium;
    uint8_t bytes[GP_SAVE_BYTES],bad[GP_SAVE_BYTES];assert(gp_save_encode(&a,bytes));sentinel=a;
    for(unsigned i=0;i<GP_SAVE_BYTES;i++){
        memcpy(bad,bytes,sizeof(bad));bad[i]^=128;b=sentinel;
        assert(gp_save_decode(bad,sizeof(bad),&b)==GpSaveCorrupt);same(&b,&sentinel);
    }
    for(unsigned n=0;n<GP_SAVE_BYTES;n++)assert(gp_save_decode(bytes,n,&b)==GpSaveCorrupt);
    assert(gp_save_decode(bytes,GP_SAVE_BYTES+1,&b)==GpSaveCorrupt);
    memcpy(bad,bytes,sizeof(bad));bad[4]=99;rechecksum(bad);assert(gp_save_decode(bad,sizeof(bad),&b)==GpSaveVersion);
    /* Invalid state with a correct CRC must still be rejected. */
    for(unsigned offset=12;offset<=18;offset++){memcpy(bad,bytes,sizeof(bad));bad[offset]=255;rechecksum(bad);if(offset!=18)assert(gp_save_decode(bad,sizeof(bad),&b)==GpSaveCorrupt);}
    memcpy(bad,bytes,sizeof(bad));bad[25]=2;rechecksum(bad);assert(gp_save_decode(bad,sizeof(bad),&b)==GpSaveCorrupt);
    /* Migrate an old v0.6.0/0.6.1 race without changing its simulation. */
    uint8_t legacy[384]={0};memcpy(legacy,bytes,321);legacy[4]=1;
    uint32_t crc=UINT32_MAX;
    for(unsigned i=0;i<380;i++){crc^=legacy[i];for(int j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320u);}
    crc=~crc;for(int i=0;i<4;i++)legacy[380+i]=(uint8_t)(crc>>(8*i));
    assert(gp_save_decode(legacy,sizeof(legacy),&b)==GpSaveOk);same(&a,&b);
    memcpy(bad,bytes,sizeof(bad));bad[4]=2;memset(bad+528,0,106);rechecksum(bad);
    assert(gp_save_decode(bad,sizeof(bad),&b)==GpSaveOk);same(&a,&b);
    gp_weekend_start(&a.weekend,true,123);
    assert(gp_save_encode(&a,bad));
    uint8_t weekend_bytes[GP_SAVE_BYTES];memcpy(weekend_bytes,bad,sizeof(bad));
    for(unsigned offset=321;offset<=325;offset++){
        memcpy(bad,weekend_bytes,sizeof(bad));bad[offset]=255;rechecksum(bad);
        assert(gp_save_decode(bad,sizeof(bad),&b)==GpSaveCorrupt);
    }
    memcpy(bad,weekend_bytes,sizeof(bad));bad[326]=0;bad[327]=0;bad[328]=0;bad[329]=0;rechecksum(bad);
    assert(gp_save_decode(bad,sizeof(bad),&b)==GpSaveCorrupt);
    memset(&a.weekend,0,sizeof(a.weekend));
    GpTestFs fs={0};GpSaveIo io=test_io(&fs);
    assert(gp_save_load(&io,&b)==GpSaveMissing);fs.no_card=true;assert(gp_save_store(&io,&a)==GpSaveNoCard);fs.no_card=false;
    assert(gp_save_store(&io,&a)==GpSaveOk);assert(gp_save_load(&io,&b)==GpSaveOk);assert(b.sequence==1);a.sequence=1;same(&a,&b);
    gp_advance_lap(&a.race);assert(gp_save_store(&io,&a)==GpSaveOk);assert(gp_save_load(&io,&b)==GpSaveOk);assert(b.sequence==2);a.sequence=2;same(&a,&b);
    GpTestFs clean=fs;
    for(int n=0;n<(int)GP_SAVE_BYTES;n++){
        fs=clean;fs.fail_write=true;fs.truncate_at=n;
        assert(gp_save_store(&io,&a)==GpSaveIoError);fs.fail_write=false;
        assert(gp_save_load(&io,&b)==GpSaveOk);same(&a,&b);
    }
    fs=clean;fs.fail_replace=true;assert(gp_save_store(&io,&a)==GpSaveIoError);assert(gp_save_load(&io,&b)==GpSaveOk);same(&a,&b);
    fs=clean;fs.bad_readback=true;assert(gp_save_store(&io,&a)==GpSaveIoError);assert(gp_save_load(&io,&b)==GpSaveOk);same(&a,&b);
    fs=clean;fs.data[1][40]^=1;assert(gp_save_load(&io,&b)==GpSaveRecovered);assert(b.sequence==1&&b.race.lap==0);
    assert(gp_save_store(&io,&b)==GpSaveOk);assert(gp_save_load(&io,&b)==GpSaveOk);
    fs=clean;fs.fail_read=true;assert(gp_save_store(&io,&a)==GpSaveIoError);assert(!memcmp(fs.data,clean.data,sizeof(fs.data)));
    fs=clean;fs.data[0][4]=99;rechecksum(fs.data[0]);assert(gp_save_store(&io,&a)==GpSaveVersion);
    fs=clean;fs.data[0][0]^=1;fs.data[1][0]^=1;assert(gp_save_load(&io,&b)==GpSaveCorrupt);
    printf("PASS: %u short/full-distance save roundtrips; CRC, semantic validation, truncation at all 640 bytes, readback/write/rename failures, backup recovery\n",snapshots);
}
