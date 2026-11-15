#include "weekend.h"
#include "tyre_model.h"
#include <string.h>
static uint32_t random_next(uint32_t* s){uint32_t x=*s;x^=x<<13;x^=x>>17;x^=x<<5;return *s=x;}
uint32_t gp_sprint_calendar(uint32_t seed){
    uint32_t rng=seed?seed:0x62026u,mask=0;uint8_t pool[GP_TRACK_COUNT];
    for(unsigned i=0;i<GP_TRACK_COUNT;i++)pool[i]=i;
    for(unsigned i=0;i<GP_SPRINT_COUNT;i++){
        unsigned remaining=GP_TRACK_COUNT-i;
        uint32_t draw,limit=UINT32_MAX-UINT32_MAX%remaining;
        do{draw=random_next(&rng);}while(draw>=limit);
        unsigned j=i+draw%remaining;uint8_t tmp=pool[i];pool[i]=pool[j];pool[j]=tmp;
        mask|=1u<<pool[i];
    }
    return mask;
}
bool gp_weekend_sprint(const GpWeekend* w,uint8_t track){return track<GP_TRACK_COUNT&&(w->sprint_mask&(1u<<track));}
void gp_weekend_start(GpWeekend* w,bool full,uint32_t seed){
    memset(w,0,sizeof(*w));w->active=true;w->full_distance=full;w->sprint_mask=gp_sprint_calendar(seed);
}
bool gp_weekend_next(GpWeekend* w,uint8_t track){
    if(!w->active||w->phase!=GpWeekendResult||w->stage==GpStageDone)return false;
    switch(w->stage){
    case GpStageFP1:w->stage=gp_weekend_sprint(w,track)?GpStageSprintQual:GpStageFP2;break;
    case GpStageFP2:w->stage=GpStageFP3;break;
    case GpStageFP3:w->stage=GpStageQual;break;
    case GpStageSprintQual:w->stage=GpStageSprint;break;
    case GpStageSprint:w->stage=GpStageQual;break;
    case GpStageQual:w->stage=GpStageRace;break;
    case GpStageRace:w->rounds_done++;w->stage=w->rounds_done==GP_TRACK_COUNT?GpStageDone:GpStageFP1;break;
    default:return false;
    }
    w->phase=GpWeekendReady;return true;
}
uint8_t gp_sprint_laps(uint8_t track,bool full){return full?(100000u+gp_track_info(track)->length_m-1u)/gp_track_info(track)->length_m:5;}
const char* gp_stage_name(GpStage s){
    static const char* const names[]={"FP1","FP2","FP3","КВАЛА СПРИНТА","СПРИНТ","КВАЛИФИКАЦИЯ","ГОНКА","СЕЗОН ЗАВЕРШЁН"};
    return (unsigned)s<=GpStageDone?names[s]:"?";
}
bool gp_weekend_valid(const GpWeekend* w,const GpRace* r){
    if(!w->active)return true;
    if((unsigned)w->stage>GpStageDone||(unsigned)w->phase>GpWeekendResult||w->rounds_done>GP_TRACK_COUNT)return false;
    if((w->stage==GpStageDone)!=(w->rounds_done==GP_TRACK_COUNT))return false;
    if(w->sprint_mask>>GP_TRACK_COUNT)return false;
    unsigned n=0;for(unsigned i=0;i<GP_TRACK_COUNT;i++)n+=(w->sprint_mask>>i)&1;
    if(n!=GP_SPRINT_COUNT)return false;
    bool sprint=gp_weekend_sprint(w,r->track);
    if((w->stage==GpStageSprintQual||w->stage==GpStageSprint)&&!sprint)return false;
    if((w->stage==GpStageFP2||w->stage==GpStageFP3)&&sprint)return false;
    bool practice=w->stage<=GpStageFP3;
    bool qualifying=w->stage==GpStageQual||w->stage==GpStageSprintQual;
    if(w->phase>=GpWeekendQualSetup&&w->phase<=GpWeekendGrid&&!qualifying&&!practice)return false;
    if(w->phase==GpWeekendRacing&&w->stage!=GpStageRace&&w->stage!=GpStageSprint)return false;
    if(w->stage==GpStageDone&&w->phase!=GpWeekendReady)return false;
    if((qualifying||practice)&&w->phase!=GpWeekendReady){
        const GpQualifying* q=&w->qualifying;
        if(q->track!=r->track||q->team!=r->team||!q->rng_state||q->session<1||q->session>3||q->run>2)return false;
        if(q->active_count!=(q->session==1?22:(q->session==2?16:10)))return false;
        if(q->complete!=(q->session==3&&q->run==2))return false;
        if(practice&&q->session!=1)return false;
        if(w->phase==GpWeekendQualSetup&&(q->complete||q->run==2))return false;
        if(w->phase==GpWeekendQualResult&&!q->run)return false;
        if(qualifying&&(w->phase==GpWeekendGrid||w->phase==GpWeekendResult)&&!q->complete)return false;
    }
    if(w->stage==GpStageRace||w->stage==GpStageSprint){
        if(!w->qualifying.complete||w->qualifying.team!=r->team||w->qualifying.track!=r->track)return false;
        if(w->phase==GpWeekendResult&&!gp_race_finished(r))return false;
        if(w->phase==GpWeekendRacing&&gp_race_finished(r))return false;
    }
    /* A retained grid is needed between qualifying and its race. */
    const GpQualifying* q=&w->qualifying;
    if(q->session){
        if(q->session>3||q->run>2||q->track>=GP_TRACK_COUNT||q->team>=GP_TEAM_COUNT||!q->rng_state)return false;
        if(q->active_count!=(q->session==1?22:(q->session==2?16:10)))return false;
        if(q->complete!=(q->session==3&&q->run==2))return false;
        bool seen[GP_DRIVER_COUNT]={0};
        for(unsigned i=0;i<GP_DRIVER_COUNT;i++){
            if(q->temperature[i]&&(q->temperature[i]<GP_TEMPERATURE_MIN||q->temperature[i]>GP_TEMPERATURE_MAX))return false;
            uint8_t id=q->grid[i];if(id>=GP_DRIVER_COUNT||seen[id]||q->lap_ms[id]>1000000u)return false;seen[id]=true;
            if(q->run&&i<q->active_count){
                if(q->lap_ms[id]<1000)return false;
                if(i&&q->lap_ms[q->grid[i-1]]>q->lap_ms[id])return false;
            }
        }
        for(unsigned i=0;i<2;i++)if((unsigned)q->tyre[i]>=GpTyreCount||(unsigned)q->pace[i]>GpPaceAttack||q->soft_used[i]>6)return false;
    }else if(w->phase!=GpWeekendReady)return false;
    return true;
}
