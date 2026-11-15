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
    memset(w,0,sizeof(*w));w->career.season=1;w->active=true;w->full_distance=full;w->sprint_mask=gp_sprint_calendar(seed);gp_championship_rebuild(w);
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
static unsigned finishes(const GpWeekend* w,unsigned id,unsigned position,bool team){
    unsigned n=0;
    for(unsigned t=0;t<GP_TRACK_COUNT;t++)if(w->results[t][0].recorded){
        uint8_t driver=w->results[t][0].finish[position];
        if(!(driver&128)&&(team?driver/2:driver)==id)n++;
    }
    return n;
}
static bool championship_ahead(const GpWeekend* w,unsigned a,unsigned b,bool team){
    const uint16_t* points=team?w->team_points:w->driver_points;
    if(points[a]!=points[b])return points[a]>points[b];
    for(unsigned p=0;p<GP_DRIVER_COUNT;p++){
        unsigned ac=finishes(w,a,p,team),bc=finishes(w,b,p,team);if(ac!=bc)return ac>bc;
    }
    return a<b; /* Fully equal records use roster order, a stable game tie-break. */
}
void gp_championship_rebuild(GpWeekend* w){
    memset(w->driver_points,0,sizeof(w->driver_points));memset(w->team_points,0,sizeof(w->team_points));
    bool any=false;
    for(unsigned t=0;t<GP_TRACK_COUNT;t++)for(unsigned sprint=0;sprint<2;sprint++){
        const GpRoundResult* result=&w->results[t][sprint];if(!result->recorded)continue;any=true;
        for(unsigned p=0;p<GP_DRIVER_COUNT;p++){
            uint8_t id=result->finish[p];if(id&128||id>=GP_DRIVER_COUNT)continue;
            unsigned points=sprint?(p<8?8-p:0):gp_points_for_position(p+1);
            w->driver_points[id]+=points;w->team_points[id/2]+=points;
        }
    }
    for(unsigned team=0;team<2;team++){
        uint8_t* order=team?w->team_order:w->driver_order;unsigned count=team?GP_TEAM_COUNT:GP_DRIVER_COUNT;
        for(unsigned i=0;i<count;i++)order[i]=i;
        if(!any)continue;
        for(unsigned i=1;i<count;i++){
            uint8_t id=order[i];unsigned j=i;
            while(j&&championship_ahead(w,id,order[j-1],team)){order[j]=order[j-1];j--;}
            order[j]=id;
        }
    }
}
bool gp_weekend_record(GpWeekend* w,const GpRace* r){
    if(!w->active||!gp_race_finished(r)||(w->stage!=GpStageRace&&w->stage!=GpStageSprint))return false;
    GpRoundResult* result=&w->results[r->track][w->stage==GpStageSprint];if(result->recorded)return false;
    result->recorded=true;
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){uint8_t id=r->ranking[i];result->finish[i]=id|(r->drivers[id].status==GpDriverRunning?0:128);}
    gp_championship_rebuild(w);return true;
}
void gp_career_record_qual(GpWeekend* w,const GpQualifying* q){
    if(!w->active||w->stage!=GpStageQual||!q->complete||w->career.pole[q->track])return;
    for(unsigned slot=0;slot<2;slot++)w->career.qual_pos[q->track][slot]=gp_grid_position(q,q->team*2+slot);
    w->career.pole[q->track]=q->grid[0]+1;
}
uint32_t gp_career_stat(const GpWeekend* w,const GpRace* r,uint8_t slot,uint8_t stat,bool lifetime){
    if(slot>1||stat>=GP_CAREER_STATS)return 0;
    uint32_t value=0;unsigned id=r->team*2+slot;
    if(stat==4)value=w->driver_points[id];
    else if(stat==5)value=w->stage==GpStageDone&&!w->incomplete_history&&w->driver_order[0]==id;
    else for(unsigned t=0;t<GP_TRACK_COUNT;t++){
        if(stat==3){value+=w->career.pole[t]==id+1;continue;}
        const GpRoundResult* result=&w->results[t][0];if(!result->recorded)continue;
        if(stat==0){value++;continue;}
        for(unsigned pos=0;pos<(stat==1?1u:3u);pos++)value+=result->finish[pos]==id;
    }
    if(stat==0&&w->stage==GpStageRace&&w->phase==GpWeekendRacing&&!w->results[r->track][0].recorded)value++;
    return value+(lifetime?w->career.totals[slot][stat]:0);
}
void gp_career_compare(const GpWeekend* w,const GpRace* r,uint8_t out[2],bool qualifying){
    out[0]=out[1]=0;
    for(unsigned t=0;t<GP_TRACK_COUNT;t++){
        unsigned p0=0,p1=0;
        if(qualifying){p0=w->career.qual_pos[t][0];p1=w->career.qual_pos[t][1];}
        else if(w->results[t][0].recorded)for(unsigned p=0;p<GP_DRIVER_COUNT;p++){
            unsigned id=w->results[t][0].finish[p]&127;
            if(id==r->team*2)p0=p+1;
            if(id==r->team*2u+1u)p1=p+1;
        }
        if(p0&&p1&&p0!=p1)out[p0<p1?0:1]++;
    }
}
uint8_t gp_career_goal(const GpRace* r){unsigned rating=gp_team_info(r->team)->rating;return rating>=90?1:(rating>=82?0:2);}
bool gp_career_goal_met(const GpWeekend* w,const GpRace* r,uint8_t slot){
    unsigned goal=gp_career_goal(r);
    if(goal<2)return gp_career_stat(w,r,slot,goal==0?4:2,false)>0;
    if(w->stage!=GpStageDone||w->incomplete_history)return false;
    for(unsigned p=0;p<GP_DRIVER_COUNT;p++)if(w->driver_order[p]/2==r->team)return w->driver_order[p]%2==slot;
    return false;
}
bool gp_career_next(GpWeekend* w,const GpRace* r,uint32_t seed){
    if(!w->active||w->stage!=GpStageDone||w->career.season>=65535)return false;
    GpCareer* c=&w->career;
    if(c->history_count==GP_CAREER_HISTORY){memmove(c->history,c->history+1,sizeof(c->history[0])*(GP_CAREER_HISTORY-1));c->history_count--;}
    GpSeasonSummary* h=&c->history[c->history_count++];memset(h,0,sizeof(*h));h->season=c->season;h->partial=w->incomplete_history||c->partial_stats;
    gp_career_compare(w,r,h->qualifying,true);
    for(unsigned slot=0;slot<2;slot++){
        h->points[slot]=gp_career_stat(w,r,slot,4,false);h->wins[slot]=gp_career_stat(w,r,slot,1,false);
        h->podiums[slot]=gp_career_stat(w,r,slot,2,false);h->poles[slot]=gp_career_stat(w,r,slot,3,false);
        for(unsigned p=0;p<GP_DRIVER_COUNT;p++)if(w->driver_order[p]==r->team*2+slot)h->position[slot]=p+1;
        for(unsigned stat=0;stat<GP_CAREER_STATS;stat++)c->totals[slot][stat]+=gp_career_stat(w,r,slot,stat,false);
    }
    c->partial_totals|=h->partial;c->partial_stats=false;c->season++;
    memset(c->qual_pos,0,sizeof(c->qual_pos));memset(c->pole,0,sizeof(c->pole));
    memset(w->results,0,sizeof(w->results));memset(&w->qualifying,0,sizeof(w->qualifying));
    w->incomplete_history=false;w->rounds_done=0;w->stage=GpStageFP1;w->phase=GpWeekendReady;w->sprint_mask=gp_sprint_calendar(seed);
    gp_championship_rebuild(w);return true;
}
bool gp_career_valid(const GpWeekend* w,const GpRace* r){
    const GpCareer* c=&w->career;
    if(w->active?(c->season<1||c->season>65535):c->season!=0)return false;
    unsigned completed=c->season?c->season-1:0;
    if(c->history_count!=(completed<GP_CAREER_HISTORY?completed:GP_CAREER_HISTORY))return false;
    if(!w->active&&(c->partial_stats||c->partial_totals))return false;
    for(unsigned slot=0;slot<2;slot++){
        for(unsigned stat=0;stat<GP_CAREER_STATS;stat++)if(c->totals[slot][stat]>completed*(stat==4?623u:(stat==5?1u:23u)))return false;
        if(c->totals[slot][1]>c->totals[slot][2]||c->totals[slot][2]>c->totals[slot][0])return false;
    }
    for(unsigned t=0;t<GP_TRACK_COUNT;t++){
        unsigned p0=c->qual_pos[t][0],p1=c->qual_pos[t][1],pole=c->pole[t];
        if(p0>22||p1>22||pole>22)return false;
        if(!pole){if(p0||p1)return false;}
        else if(!w->active||!p0||!p1||p0==p1)return false;
        for(unsigned slot=0;slot<2;slot++)if((c->qual_pos[t][slot]==1)!=(pole==r->team*2+slot+1))return false;
    }
    for(unsigned i=0;i<GP_CAREER_HISTORY;i++){
        const GpSeasonSummary* h=&c->history[i];bool used=i<c->history_count;
        if(h->season!=(used?c->season-c->history_count+i:0)||(!used&&h->partial))return false;
        for(unsigned slot=0;slot<2;slot++){
            if(h->points[slot]>(used?623u:0u)||h->position[slot]>(used?22u:0u)||(used&&!h->position[slot]))return false;
            if(h->wins[slot]>h->podiums[slot]||h->podiums[slot]>(used?23u:0u)||h->poles[slot]>(used?23u:0u)||h->qualifying[slot]>(used?23u:0u))return false;
        }
        if(h->qualifying[0]+h->qualifying[1]>23)return false;
    }
    return true;
}
bool gp_weekend_valid(const GpWeekend* w,const GpRace* r){
    if(!gp_career_valid(w,r))return false;
    for(unsigned t=0;t<GP_TRACK_COUNT;t++)for(unsigned sprint=0;sprint<2;sprint++){
        const GpRoundResult* result=&w->results[t][sprint];uint32_t seen=0;bool retired=false;
        if(result->recorded&&(!w->active||(sprint&&!gp_weekend_sprint(w,t))))return false;
        for(unsigned p=0;p<GP_DRIVER_COUNT;p++){
            uint8_t value=result->finish[p],id=value&127;
            if(!result->recorded){if(value)return false;continue;}
            if(id>=GP_DRIVER_COUNT||(seen&(1u<<id)))return false;
            seen|=1u<<id;
            if(value&128)retired=true;else if(retired)return false;
        }
    }
    if(!w->active)return !w->incomplete_history;
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
        if(q->mode!=r->mode||q->controlled_slot!=r->controlled_slot)return false;
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
