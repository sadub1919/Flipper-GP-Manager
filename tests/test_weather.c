#include "../gp_save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static GpWeather condition(unsigned wet){return (GpWeather){wet,wet>=70?GpRainHeavy:(wet>=25?GpRainLight:GpRainNone),GpRainNone,3,123};}
static void same(const GpSavedGame* a,const GpSavedGame* b){uint8_t x[GP_SAVE_BYTES],y[GP_SAVE_BYTES];assert(gp_save_encode(a,x));assert(gp_save_encode(b,y));assert(!memcmp(x,y,sizeof(x)));}
static void checksum(uint8_t* b){
    uint32_t crc=UINT32_MAX;for(unsigned i=0;i<GP_SAVE_BYTES-4;i++){crc^=b[i];for(int bit=0;bit<8;bit++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}
    crc=~crc;for(unsigned i=0;i<4;i++)b[GP_SAVE_BYTES-4+i]=(uint8_t)(crc>>(8*i));
}
int main(void){
    for(unsigned seed=0;seed<1000;seed++){
        GpWeather a,b;gp_weather_start(&a,seed);b=a;
        for(unsigned step=0;step<200;step++){
            assert(gp_weather_valid(&a));gp_weather_step(&a);gp_weather_step(&b);assert(!memcmp(&a,&b,sizeof(a)));
        }
    }
    GpWeather w={0,GpRainHeavy,GpRainNone,4,1};
    for(int i=0;i<4;i++)gp_weather_step(&w);
    assert(w.wetness==100&&w.rain==GpRainNone);
    w.remaining=4;w.forecast=GpRainNone;
    for(int i=0;i<4;i++)gp_weather_step(&w);
    assert(w.wetness==40&&w.rain==GpRainNone);
    w.remaining=4;for(int i=0;i<3;i++)gp_weather_step(&w);assert(w.wetness==0);
    for(unsigned wet=0;wet<=100;wet++){
        w=condition(wet);
        unsigned good=gp_weather_tyre(&w);assert(good<GpTyreCount);
        if(wet==0){assert(good==GpTyreMedium);assert(gp_weather_penalty(&w,3)>gp_weather_penalty(&w,1));assert(gp_weather_wear(&w,4)>gp_weather_wear(&w,1));}
        if(wet==50){assert(good==3);assert(gp_weather_penalty(&w,3)<gp_weather_penalty(&w,1));assert(gp_weather_penalty(&w,3)<gp_weather_penalty(&w,4));}
        if(wet==100){assert(good==4);assert(gp_weather_penalty(&w,4)<gp_weather_penalty(&w,3));}
        for(unsigned tyre=0;tyre<GpTyreCount;tyre++){
            GpSavedGame a={0},b={0};gp_race_start(&a.race,0,0,0,234);a.race.incident_rng=0;a.race.weather=w;
            for(unsigned slot=0;slot<2;slot++){a.race.pit_queued[slot]=true;a.race.pit_tyres[slot]=tyre;}
            uint8_t bytes[GP_SAVE_BYTES];assert(gp_save_encode(&a,bytes));assert(gp_save_decode(bytes,sizeof(bytes),&b)==GpSaveOk);same(&a,&b);
            gp_advance_lap(&a.race);gp_advance_lap(&b.race);same(&a,&b);
            assert(a.race.drivers[0].tyre==tyre&&a.race.drivers[1].tyre==tyre);
            GpQualifying q,dry;gp_qualifying_start(&q,0,0,234);q.weather=w;q.tyre[0]=tyre;dry=q;dry.tyre[0]=(GpTyre)good;
            gp_qualifying_run(&q);gp_qualifying_run(&dry);
            if(wet==100&&tyre==GpTyreSoft)assert(q.lap_ms[0]>dry.lap_ms[0]+20000);
        }
    }
    /* Same race/random draws, dry -> wet -> drying: timely changes help both
       drivers, and AI changes without receiving player pit commands. */
    GpRace smart,stay;gp_race_start(&smart,3,0,0,234);smart.incident_rng=0;smart.weather=condition(0);
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++)smart.drivers[i].tyre=GpTyreMedium;
    stay=smart;
    const unsigned wetness[12]={0,0,40,75,100,100,100,100,65,40,10,0};
    for(unsigned lap=0;lap<12;lap++){
        smart.weather=stay.weather=condition(wetness[lap]);
        GpTyre tyre=(GpTyre)gp_weather_tyre(&smart.weather);
        for(unsigned slot=0;slot<2;slot++)if(smart.drivers[6+slot].tyre!=tyre){smart.pit_queued[slot]=true;smart.pit_tyres[slot]=tyre;}
        gp_advance_lap(&smart);gp_advance_lap(&stay);
        if(lap==4)assert(smart.drivers[0].tyre==GpTyreWet&&smart.drivers[0].pits>0);
    }
    for(unsigned id=6;id<=7;id++){
        assert(smart.drivers[id].time_ms<stay.drivers[id].time_ms);
        assert(gp_position(&smart,id)<gp_position(&stay,id));
    }
    /* Current weather and forecast survive a qualifying save, including RNG. */
    GpSavedGame a={0},b={0};gp_race_start(&a.race,0,0,0,123);a.race.incident_rng=0;gp_weekend_start(&a.weekend,false,1);
    a.weekend.stage=GpStageFP1;a.weekend.phase=GpWeekendQualSetup;
    gp_qualifying_start(&a.weekend.qualifying,0,0,321);a.weekend.qualifying.weather=condition(80);a.race.weather=a.weekend.qualifying.weather;
    uint8_t bytes[GP_SAVE_BYTES];assert(gp_save_encode(&a,bytes));assert(gp_save_decode(bytes,sizeof(bytes),&b)==GpSaveOk);same(&a,&b);
    gp_qualifying_run(&a.weekend.qualifying);gp_qualifying_run(&b.weekend.qualifying);a.weekend.phase=b.weekend.phase=GpWeekendQualResult;same(&a,&b);
    assert(gp_save_encode(&a,bytes));
    for(unsigned offset=512;offset<528;offset++){
        if(offset%8>=4)continue; // RNG bits are unrestricted except the legacy zero state.
        uint8_t corrupt[GP_SAVE_BYTES];memcpy(corrupt,bytes,sizeof(bytes));corrupt[offset]=255;checksum(corrupt);
        assert(gp_save_decode(corrupt,sizeof(corrupt),&b)==GpSaveCorrupt);
    }
    /* A real v2 weekend has no weather block; migrate without losing its session. */
    bytes[4]=2;memset(bytes+512,0,112);checksum(bytes);
    assert(gp_save_decode(bytes,sizeof(bytes),&b)==GpSaveOk);
    assert(b.weekend.phase==GpWeekendQualResult&&b.weekend.qualifying.run==a.weekend.qualifying.run);
    assert(b.race.weather.rng==0&&b.weekend.qualifying.weather.rng==0);
    assert(!memcmp(b.weekend.qualifying.grid,a.weekend.qualifying.grid,GP_DRIVER_COUNT));
    puts("PASS: 200000 deterministic weather steps; 505 wetness/tyre save continuations; dry/rain/drying strategy improves both positions; AI and qualifying weather");
}
