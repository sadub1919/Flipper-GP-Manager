#include "qualifying.h"
#include "tyre_model.h"
#include <string.h>
static uint32_t grng(uint32_t* s){uint32_t x=*s;x^=x<<13;x^=x>>17;x^=x<<5;return *s=x;}
void gp_qualifying_start(GpQualifying* q,uint8_t track,uint8_t team,uint32_t seed){
    memset(q,0,sizeof(*q));
    q->track=track%GP_TRACK_COUNT;q->team=team%GP_TEAM_COUNT;
    q->session=1;q->active_count=GP_DRIVER_COUNT;q->rng_state=seed?seed:0x20260061u;
    q->pace[0]=q->pace[1]=GpPaceNormal;
    gp_weather_start(&q->weather,seed^0x700700u);
    GpTyre initial=(GpTyre)gp_weather_tyre(&q->weather);
    q->tyre[0]=q->tyre[1]=initial>=GpTyreIntermediate?initial:GpTyreSoft;
    for(uint8_t i=0;i<GP_DRIVER_COUNT;i++)q->grid[i]=i;
}
bool gp_qualifying_active(const GpQualifying* q,uint8_t driver){
    if(!q->session||q->complete)return false;
    for(uint8_t p=0;p<q->active_count;p++)if(q->grid[p]==driver)return true;
    return false;
}
bool gp_qualifying_run(GpQualifying* q){
    if(!q->session||q->complete||q->run>=GP_QUAL_RUNS)return false;
    const GpTrackInfo* t=gp_track_info(q->track);
    /* Iterate driver IDs so tyre choices cannot change rivals' random draws. */
    for(uint8_t i=0;i<GP_DRIVER_COUNT;i++){
        if(!gp_qualifying_active(q,i))continue;
        const GpDriverInfo* d=gp_driver_info(i);
        GpTyre tyre=(GpTyre)gp_weather_tyre(&q->weather);if(tyre<GpTyreIntermediate)tyre=q->session==1?GpTyreMedium:GpTyreSoft;GpPace pace=GpPaceNormal;uint32_t used_penalty=0;
        if(d->team==q->team){
            uint8_t slot=i%2;tyre=q->tyre[slot];pace=q->pace[slot];
            if(tyre==GpTyreSoft){
                if(q->soft_used[slot]>=GP_QUAL_SOFT_SETS)
                    used_penalty=1200u*(q->soft_used[slot]-GP_QUAL_SOFT_SETS+1u);
                q->soft_used[slot]++;
            }
        }
        uint32_t variation=grng(&q->rng_state)%1600u;
        uint32_t mistake=grng(&q->rng_state)%100u;
        uint32_t lap=(uint32_t)t->length_m*13u+12000u+
            (100u-gp_team_info(d->team)->rating)*260u+(100u-d->rating)*170u;
        lap+=variation+used_penalty+(tyre==GpTyreMedium?850u:(tyre==GpTyreHard?1600u:0u));
        GpTyreThermal outlap=gp_tyre_thermal_step(tyre,gp_tyre_blanket(tyre),pace,t,&q->weather,GpEventNone);
        GpTyreThermal flying=gp_tyre_thermal_step(tyre,outlap.temperature,pace,t,&q->weather,GpEventNone);
        lap+=(uint32_t)gp_weather_penalty(&q->weather,tyre)+flying.loss_ms;
        if(pace==GpPaceConserve)lap+=500u;
        if(pace==GpPaceAttack){lap-=550u;if(mistake<18u)lap+=3500u;}
        /* Track evolution rewards a second attempt, best valid lap is retained. */
        if(q->run)lap-=250u;
        if(!q->lap_ms[i]||lap<q->lap_ms[i]){q->lap_ms[i]=lap;q->temperature[i]=flying.temperature;}
    }
    for(uint8_t i=1;i<q->active_count;i++){
        uint8_t key=q->grid[i],j=i;
        while(j&&q->lap_ms[key]<q->lap_ms[q->grid[j-1]]){q->grid[j]=q->grid[j-1];--j;}
        q->grid[j]=key;
    }
    q->run++;
    gp_weather_step(&q->weather);
    if(q->session==3&&q->run==GP_QUAL_RUNS)q->complete=true;
    return true;
}
bool gp_qualifying_next(GpQualifying* q){
    if(q->complete||q->run!=GP_QUAL_RUNS||q->session>=3||!q->session)return false;
    q->session++;q->run=0;q->active_count=q->session==2?16:10;
    for(uint8_t p=0;p<q->active_count;p++){q->lap_ms[q->grid[p]]=0;q->temperature[q->grid[p]]=0;}
    return true;
}
void gp_qualify(GpQualifying* q,uint8_t track,uint32_t seed){
    gp_qualifying_start(q,track,0,seed);
    do {while(gp_qualifying_run(q)){} } while(gp_qualifying_next(q));
}
uint8_t gp_grid_position(const GpQualifying* q,uint8_t driver_id){
    for(uint8_t i=0;i<GP_DRIVER_COUNT;i++)if(q->grid[i]==driver_id)return i+1;
    return GP_DRIVER_COUNT;
}
