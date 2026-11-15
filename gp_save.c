#include "gp_save.h"
#include "tyre_model.h"
#include <string.h>
#include <limits.h>
#define SAVE_VERSION 6u /* Binary layout AND simulation rules version. */
static void put32(uint8_t* p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static uint32_t get32(const uint8_t* p){uint32_t n=0;for(unsigned i=0;i<4;i++)n|=(uint32_t)p[i]<<(8*i);return n;}
static uint32_t crc32(const uint8_t* p,size_t n){
    uint32_t crc=UINT32_MAX;
    for(size_t i=0;i<n;i++){crc^=p[i];for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}
    return ~crc;
}
static bool valid(const GpSavedGame* s){
    const GpRace* r=&s->race;
    if(r->team>=GP_TEAM_COUNT||r->track>=GP_TRACK_COUNT||r->active_slot>1||r->player!=r->team*2+r->active_slot)return false;
    if(!gp_weather_valid(&r->weather)||!gp_weather_valid(&s->weekend.qualifying.weather))return false;
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){
        unsigned temperature=s->weekend.qualifying.temperature[i];
        if(temperature&&(temperature<GP_TEMPERATURE_MIN||temperature>GP_TEMPERATURE_MAX))return false;
    }
    if(!gp_weekend_valid(&s->weekend,r))return false;
    bool sprint=s->weekend.active&&s->weekend.stage==GpStageSprint;
    if(r->total_laps!=GP_TOTAL_LAPS&&r->total_laps!=gp_track_info(r->track)->actual_laps&&
       !(sprint&&r->total_laps==gp_sprint_laps(r->track,s->weekend.full_distance)))return false;
    if(r->lap>r->total_laps||(unsigned)r->event>GpEventRed||!r->rng_state)return false;
    if(r->event_remaining>4)return false;
    if((r->event==GpEventNone||r->event==GpEventRed)?r->event_remaining!=0:r->event_remaining==0)return false;
    if(r->event==GpEventRed&&gp_race_finished(r))return false;
    if(r->incident_driver>GP_DRIVER_COUNT||r->incident_serial>GP_DRIVER_COUNT)return false;
    if(r->incident_driver==GP_DRIVER_COUNT){if(r->incident_pending||r->incident_serial)return false;}
    else if(!r->incident_serial||r->drivers[r->incident_driver].status==GpDriverRunning)return false;
    for(unsigned i=0;i<2;i++){
        const GpBattleReport* b=&r->battle[i];
        if(b->result>GpBattleMistake)return false;
        if(b->result==GpBattleNone){if(b->opponent||b->lap)return false;}
        else if(!r->battle_rng||b->opponent>=GP_DRIVER_COUNT||b->opponent==r->team*2+i||!b->lap||b->lap>r->lap)return false;
    }
    unsigned retired=0;
    if(s->clock.progress_ticks>=GP_LAP_MS||s->clock.speed_index>2)return false;
    for(unsigned i=0;i<2;i++)if((unsigned)r->pace[i]>GpPaceAttack||(unsigned)r->pit_tyres[i]>=GpTyreCount)return false;
    bool seen[GP_DRIVER_COUNT]={0};
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){
        const GpDriver* d=&r->drivers[i];
        if((unsigned)d->status>GpDriverFailure)return false;
        if(d->status==GpDriverRunning){if(d->retired_lap)return false;}
        else{if(!d->retired_lap||d->retired_lap>r->lap||d->pits>d->retired_lap)return false;retired++;}
        if(d->temperature<GP_TEMPERATURE_MIN||d->temperature>GP_TEMPERATURE_MAX)return false;
        if(d->driver_index!=i||d->wear>100||d->wear_fraction>=r->total_laps||(unsigned)d->tyre>=GpTyreCount||d->pits>r->lap)return false;
        if(d->time_ms>100000000u||d->last_lap_ms>1000000u)return false;
        unsigned id=r->ranking[i];if(id>=GP_DRIVER_COUNT||seen[id])return false;seen[id]=true;
        if(i&&gp_driver_precedes(&r->drivers[id],&r->drivers[r->ranking[i-1]]))return false;
    }
    if(retired!=r->incident_serial)return false;
    for(unsigned i=0;i<2;i++)if(r->drivers[r->team*2+i].status!=GpDriverRunning&&r->pit_queued[i])return false;
    return true;
}
bool gp_save_encode(const GpSavedGame* s,uint8_t out[GP_SAVE_BYTES]){
    if(!valid(s))return false;
    memset(out,0,GP_SAVE_BYTES);memcpy(out,"GPM6",4);put32(out+4,SAVE_VERSION);put32(out+8,s->sequence);
    size_t n=12;const GpRace* r=&s->race;
#define BYTE(x) out[n++]=(uint8_t)(x)
#define WORD(x) do{put32(out+n,(x));n+=4;}while(0)
    BYTE(r->team);BYTE(r->track);BYTE(r->active_slot);BYTE(r->lap);BYTE(r->total_laps);
    BYTE(r->event);BYTE(r->event_serial);WORD(r->rng_state);
    for(unsigned i=0;i<2;i++){BYTE(r->pace[i]);BYTE(r->pit_tyres[i]);BYTE(r->pit_queued[i]);}
    BYTE(s->clock.speed_index);BYTE(s->clock.paused);WORD(s->clock.progress_ticks);
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){
        const GpDriver* d=&r->drivers[i];BYTE(d->wear);BYTE(d->wear_fraction);BYTE(d->tyre);BYTE(d->pits);WORD(d->time_ms);WORD(d->last_lap_ms);
    }
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++)BYTE(r->ranking[i]);
    const GpWeekend* w=&s->weekend;
    BYTE(w->active);
    if(w->active){
        BYTE(w->full_distance);BYTE(w->rounds_done);BYTE(w->stage);BYTE(w->phase);WORD(w->sprint_mask);
        const GpQualifying* q=&w->qualifying;
        BYTE(q->track);BYTE(q->team);BYTE(q->session);BYTE(q->run);BYTE(q->active_count);WORD(q->rng_state);BYTE(q->complete);
        for(unsigned i=0;i<2;i++){BYTE(q->tyre[i]);BYTE(q->pace[i]);BYTE(q->soft_used[i]);}
        for(unsigned i=0;i<GP_DRIVER_COUNT;i++){BYTE(q->grid[i]);WORD(q->lap_ms[i]);}
    }
    n=512;
    const GpWeather* weather[2]={&s->race.weather,&s->weekend.qualifying.weather};
    for(unsigned i=0;i<2;i++){
        const GpWeather* v=weather[i];BYTE(v->wetness);BYTE(v->rain);BYTE(v->forecast);BYTE(v->remaining);WORD(v->rng);
    }
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++)BYTE(s->race.drivers[i].temperature);
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++)BYTE(s->weekend.qualifying.temperature[i]);
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){BYTE(r->drivers[i].status);BYTE(r->drivers[i].retired_lap);}
    WORD(r->incident_rng);BYTE(r->event_remaining);BYTE(r->incident_driver);BYTE(r->incident_serial);BYTE(r->incident_pending);
    WORD(r->battle_rng);
    for(unsigned i=0;i<2;i++){BYTE(r->battle[i].result);BYTE(r->battle[i].opponent);BYTE(r->battle[i].lap);}
#undef BYTE
#undef WORD
    put32(out+GP_SAVE_BYTES-4,crc32(out,GP_SAVE_BYTES-4));return true;
}
GpSaveStatus gp_save_decode(const uint8_t* data,size_t len,GpSavedGame* out){
    if((len!=GP_SAVE_BYTES&&len!=384u)||memcmp(data,"GPM6",4))return GpSaveCorrupt;
    if(get32(data+len-4)!=crc32(data,len-4))return GpSaveCorrupt;
    uint32_t version=get32(data+4);
    if(version!=1u&&version!=2u&&version!=3u&&version!=4u&&version!=5u&&version!=SAVE_VERSION)return GpSaveVersion;
    if((version==1u&&len!=384u)||(version>=2u&&len!=GP_SAVE_BYTES))return GpSaveCorrupt;
    GpSavedGame s={0};s.sequence=get32(data+8);GpRace* r=&s.race;size_t n=12;
#define BYTE() data[n++]
#define WORD(x) do{(x)=get32(data+n);n+=4;}while(0)
    r->team=BYTE();r->track=BYTE();r->active_slot=BYTE();r->lap=BYTE();r->total_laps=BYTE();
    r->event=BYTE();r->event_serial=BYTE();WORD(r->rng_state);
    for(unsigned i=0;i<2;i++){
        r->pace[i]=BYTE();r->pit_tyres[i]=BYTE();uint8_t flag=BYTE();if(flag>1)return GpSaveCorrupt;r->pit_queued[i]=flag;
    }
    s.clock.speed_index=BYTE();uint8_t flag=BYTE();if(flag>1)return GpSaveCorrupt;s.clock.paused=flag;WORD(s.clock.progress_ticks);
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){
        GpDriver* d=&r->drivers[i];d->driver_index=i;d->wear=BYTE();d->wear_fraction=BYTE();d->tyre=BYTE();d->pits=BYTE();WORD(d->time_ms);WORD(d->last_lap_ms);
    }
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++)r->ranking[i]=BYTE();
    if(version>=2u){
        GpWeekend* w=&s.weekend;uint8_t active=BYTE();if(active>1)return GpSaveCorrupt;w->active=active;
        if(w->active){
            uint8_t full=BYTE();if(full>1)return GpSaveCorrupt;w->full_distance=full;
            w->rounds_done=BYTE();w->stage=BYTE();w->phase=BYTE();WORD(w->sprint_mask);
            GpQualifying* q=&w->qualifying;
            q->track=BYTE();q->team=BYTE();q->session=BYTE();q->run=BYTE();q->active_count=BYTE();WORD(q->rng_state);
            uint8_t complete=BYTE();if(complete>1)return GpSaveCorrupt;q->complete=complete;
            for(unsigned i=0;i<2;i++){q->tyre[i]=BYTE();q->pace[i]=BYTE();q->soft_used[i]=BYTE();}
            for(unsigned i=0;i<GP_DRIVER_COUNT;i++){q->grid[i]=BYTE();WORD(q->lap_ms[i]);}
        }
    }
    if(version>=3u){
        for(;n<512;n++)if(data[n])return GpSaveCorrupt;
        GpWeather* weather[2]={&s.race.weather,&s.weekend.qualifying.weather};
        for(unsigned i=0;i<2;i++){
            GpWeather* v=weather[i];v->wetness=BYTE();v->rain=BYTE();v->forecast=BYTE();v->remaining=BYTE();WORD(v->rng);
        }
    }
    if(version>=4u){
        for(unsigned i=0;i<GP_DRIVER_COUNT;i++)r->drivers[i].temperature=BYTE();
        for(unsigned i=0;i<GP_DRIVER_COUNT;i++)s.weekend.qualifying.temperature[i]=BYTE();
    }else{
        /* Old sessions did not model heat: start them in the working window. */
        for(unsigned i=0;i<GP_DRIVER_COUNT;i++)r->drivers[i].temperature=gp_tyre_optimal(r->drivers[i].tyre);
    }
    if(version>=5u){
        for(unsigned i=0;i<GP_DRIVER_COUNT;i++){r->drivers[i].status=BYTE();r->drivers[i].retired_lap=BYTE();}
        WORD(r->incident_rng);r->event_remaining=BYTE();r->incident_driver=BYTE();r->incident_serial=BYTE();
        uint8_t pending=BYTE();if(pending>1)return GpSaveCorrupt;r->incident_pending=pending;
    }else{
        if(r->event==GpEventRed)return GpSaveCorrupt;
        r->incident_driver=GP_DRIVER_COUNT;
        r->event_remaining=r->event==GpEventNone?0:1;
    }
    if(version>=6u){
        WORD(r->battle_rng);
        for(unsigned i=0;i<2;i++){r->battle[i].result=BYTE();r->battle[i].opponent=BYTE();r->battle[i].lap=BYTE();}
    }
#undef BYTE
#undef WORD
    for(;n<len-4;n++)if(data[n])return GpSaveCorrupt;
    r->player=r->team*2+r->active_slot;
    if(!valid(&s))return GpSaveCorrupt;
    *out=s;return GpSaveOk;
}
static GpSaveStatus read_slot(const GpSaveIo* io,unsigned slot,GpSavedGame* out){
    uint8_t data[GP_SAVE_BYTES];size_t n=0;
    GpIoResult r=io->read(io->context,slot,data,sizeof(data),&n);
    if(r==GpIoMissing)return GpSaveMissing;
    if(r!=GpIoOk)return GpSaveIoError;
    return gp_save_decode(data,n,out);
}
/* Use heap workspace: application and GUI threads have small stacks on Flipper. */
#include <stdlib.h>
typedef struct {GpSavedGame games[2];GpSaveStatus status[2];uint8_t data[GP_SAVE_BYTES],verify[GP_SAVE_BYTES];} Slots;
static int newest(const Slots* s){
    bool a=s->status[0]==GpSaveOk,b=s->status[1]==GpSaveOk;
    if(a&&b)return s->games[1].sequence>s->games[0].sequence?1:0;
    return a?0:(b?1:-1);
}
GpSaveStatus gp_save_load(const GpSaveIo* io,GpSavedGame* out){
    if(!io->ready(io->context))return GpSaveNoCard;
    Slots* s=calloc(1,sizeof(*s));if(!s)return GpSaveIoError;
    for(unsigned i=0;i<2;i++)s->status[i]=read_slot(io,i,&s->games[i]);
    int k=newest(s);GpSaveStatus result;
    if(k>=0){*out=s->games[k];GpSaveStatus other=s->status[1-k];result=(other!=GpSaveOk&&other!=GpSaveMissing)?GpSaveRecovered:GpSaveOk;}
    else if(s->status[0]==GpSaveVersion||s->status[1]==GpSaveVersion)result=GpSaveVersion;
    else if(s->status[0]==GpSaveIoError||s->status[1]==GpSaveIoError)result=GpSaveIoError;
    else if(s->status[0]==GpSaveMissing&&s->status[1]==GpSaveMissing)result=GpSaveMissing;
    else result=GpSaveCorrupt;
    free(s);return result;
}
GpSaveStatus gp_save_store(const GpSaveIo* io,const GpSavedGame* game){
    if(!io->ready(io->context))return GpSaveNoCard;
    Slots* s=calloc(1,sizeof(*s));if(!s)return GpSaveIoError;
    for(unsigned i=0;i<2;i++)s->status[i]=read_slot(io,i,&s->games[i]);
    /* Never overwrite on a transient read error, nor silently downgrade future saves. */
    for(unsigned i=0;i<2;i++)if(s->status[i]==GpSaveIoError||s->status[i]==GpSaveVersion){GpSaveStatus r=s->status[i];free(s);return r;}
    int k=newest(s);uint32_t seq=k>=0?s->games[k].sequence:0;
    if(seq==UINT32_MAX){free(s);return GpSaveIoError;}
    unsigned target=k>=0?(unsigned)(1-k):0;
    s->games[target]=*game;s->games[target].sequence=seq+1;
    uint8_t* data=s->data;bool ok=gp_save_encode(&s->games[target],data);
    if(ok)ok=io->write(io->context,2,data,GP_SAVE_BYTES);
    if(ok){
        GpSaveStatus status=read_slot(io,2,&s->games[target]);
        uint8_t* verify=s->verify;
        ok=status==GpSaveOk&&gp_save_encode(&s->games[target],verify)&&!memcmp(data,verify,GP_SAVE_BYTES);
    }
    if(ok)ok=io->replace(io->context,2,target);
    free(s);return ok?GpSaveOk:GpSaveIoError;
}
