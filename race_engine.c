#include "race_engine.h"
#include "tyre_model.h"

#include <string.h>

/* Game roster. Ratings are fictional gameplay parameters. */
static const GpTeamInfo gp_teams[GP_TEAM_COUNT] = {
    {"MCLAREN", "MCL", 93},
    {"MERCEDES", "MER", 95},
    {"RED BULL", "RBR", 94},
    {"FERRARI", "FER", 94},
    {"WILLIAMS", "WIL", 84},
    {"RACING BULLS", "RB", 82},
    {"ASTON MARTIN", "AST", 79},
    {"HAAS", "HAA", 80},
    {"AUDI", "AUD", 82},
    {"ALPINE", "ALP", 78},
    {"CADILLAC", "CAD", 77},
};

static const GpDriverInfo gp_names[GP_DRIVER_COUNT] = {
    {"Lando Norris", "L.NORRIS", "NOR", 0, 94},
    {"Oscar Piastri", "O.PIASTRI", "PIA", 0, 94},
    {"George Russell", "G.RUSSELL", "RUS", 1, 95},
    {"Kimi Antonelli", "K.ANTONELLI", "ANT", 1, 93},
    {"Max Verstappen", "M.VERSTAPPEN", "VER", 2, 97},
    {"Isack Hadjar", "I.HADJAR", "HAD", 2, 86},
    {"Charles Leclerc", "C.LECLERC", "LEC", 3, 96},
    {"Lewis Hamilton", "L.HAMILTON", "HAM", 3, 94},
    {"Carlos Sainz", "C.SAINZ", "SAI", 4, 90},
    {"Alexander Albon", "A.ALBON", "ALB", 4, 88},
    {"Liam Lawson", "L.LAWSON", "LAW", 5, 84},
    {"Arvid Lindblad", "A.LINDBLAD", "LIN", 5, 82},
    {"Fernando Alonso", "F.ALONSO", "ALO", 6, 92},
    {"Lance Stroll", "L.STROLL", "STR", 6, 80},
    {"Rafael Camara", "R.CAMARA", "CAM", 7, 82},
    {"Oliver Bearman", "O.BEARMAN", "BEA", 7, 87},
    {"Nico Hulkenberg", "N.HULKENBERG", "HUL", 8, 87},
    {"Gabriel Bortoleto", "G.BORTOLETO", "BOR", 8, 84},
    {"Pierre Gasly", "P.GASLY", "GAS", 9, 88},
    {"Franco Colapinto", "F.COLAPINTO", "COL", 9, 82},
    {"Sergio Perez", "S.PEREZ", "PER", 10, 86},
    {"Valtteri Bottas", "V.BOTTAS", "BOT", 10, 86},
};

/* Bundled track catalogue. Full-distance races use actual_laps;
   this data is not a live feed of the official championship calendar. */
static const GpTrackInfo gp_tracks[GP_TRACK_COUNT] = {
    {"MELBOURNE", "Albert Park Circuit", 58, 5278, 10, 3},
    {"SHANGHAI", "Shanghai International Circuit", 56, 5451, 9, 3},
    {"SUZUKA", "Suzuka International Racing Course", 53, 5807, 11, 5},
    {"MIAMI", "Miami International Autodrome", 57, 5412, 8, 3},
    {"MONTREAL", "Circuit Gilles Villeneuve", 70, 4361, 9, 3},
    {"MONACO", "Circuit de Monaco", 78, 3337, 11, 5},
    {"BARCELONA", "Circuit de Barcelona-Catalunya", 66, 4657, 11, 4},
    {"SPIELBERG", "Red Bull Ring", 71, 4318, 9, 3},
    {"SILVERSTONE", "Silverstone Circuit", 52, 5891, 10, 4},
    {"SPA", "Circuit de Spa-Francorchamps", 44, 7004, 12, 5},
    {"HUNGARORING", "Hungaroring", 70, 4381, 12, 4},
    {"ZANDVOORT", "Circuit Zandvoort", 72, 4259, 12, 4},
    {"MONZA", "Autodromo Nazionale Monza", 53, 5793, 7, 2},
    {"MADRID", "Madring", 57, 5474, 10, 4},
    {"BAKU", "Baku City Circuit", 51, 6003, 9, 4},
    {"SEPANG", "Sepang International Circuit", 56, 5543, 12, 4},
    {"SINGAPORE", "Marina Bay Street Circuit", 62, 4940, 11, 5},
    {"AUSTIN", "Circuit of the Americas", 56, 5513, 11, 4},
    {"MEXICO", "Autodromo Hermanos Rodriguez", 71, 4304, 8, 3},
    {"INTERLAGOS", "Autodromo Jose Carlos Pace", 71, 4309, 11, 4},
    {"LAS VEGAS", "Las Vegas Strip Circuit", 50, 6201, 8, 3},
    {"LUSAIL", "Lusail International Circuit", 57, 5419, 11, 4},
    {"YAS MARINA", "Yas Marina Circuit", 58, 5281, 9, 3},
};

static uint32_t gp_random(GpRace* race) {
    uint32_t x=race->rng_state;
    x^=x<<13; x^=x>>17; x^=x<<5;
    race->rng_state=x;
    return x;
}

const GpTeamInfo* gp_team_info(uint8_t team) {return &gp_teams[team%GP_TEAM_COUNT];}
const GpDriverInfo* gp_driver_info(uint8_t id) {return &gp_names[id%GP_DRIVER_COUNT];}
const GpTrackInfo* gp_track_info(uint8_t id) {return &gp_tracks[id%GP_TRACK_COUNT];}
const char* gp_pace_name(GpPace p) {return p==GpPaceConserve?"SAVE":(p==GpPaceAttack?"ATTACK":"NORMAL");}
const char* gp_tyre_name(GpTyre t) {static const char* const names[]={"SOFT","MEDIUM","HARD","INTER","WET"};return (unsigned)t<GpTyreCount?names[t]:"?";}
const char* gp_event_name(GpEvent e) {return e==GpEventRed?"RED":e==GpEventYellow?"YELLOW":(e==GpEventVsc?"VSC":(e==GpEventSafetyCar?"SAFETY CAR":"GREEN"));}

void gp_select_driver(GpRace* race,uint8_t slot) {
    race->active_slot=slot%2;
    race->player=(uint8_t)(race->team*2+race->active_slot);
}

const char* gp_retirement_name(GpDriverStatus s){return s==GpDriverAccident?"АВАРИЯ":(s==GpDriverFailure?"ОТКАЗ":"В ГОНКЕ");}
uint8_t gp_running_count(const GpRace* r){uint8_t n=0;for(unsigned i=0;i<GP_DRIVER_COUNT;i++)if(r->drivers[i].status==GpDriverRunning)n++;return n;}
bool gp_race_finished(const GpRace* r){return r->lap>=r->total_laps||!gp_running_count(r);}
bool gp_driver_precedes(const GpDriver* a,const GpDriver* b){
    if((a->status==GpDriverRunning)!=(b->status==GpDriverRunning))return a->status==GpDriverRunning;
    if(a->status!=GpDriverRunning&&a->retired_lap!=b->retired_lap)return a->retired_lap>b->retired_lap;
    return a->time_ms<b->time_ms;
}
static void gp_rank(GpRace* race) {
    for(uint8_t i=1;i<GP_DRIVER_COUNT;++i) {
        uint8_t id=race->ranking[i];
        uint8_t j=i;
        while(j>0 && gp_driver_precedes(&race->drivers[id],&race->drivers[race->ranking[j-1]])) {
            race->ranking[j]=race->ranking[j-1]; --j;
        }
        race->ranking[j]=id;
    }
}

void gp_race_start(GpRace* race,uint8_t team,uint8_t slot,uint8_t track,uint32_t seed) {
    memset(race,0,sizeof(*race));
    race->total_laps=GP_TOTAL_LAPS;
    race->team=team%GP_TEAM_COUNT;
    race->track=track%GP_TRACK_COUNT;
    race->pace[0]=race->pace[1]=GpPaceNormal;
    race->pit_tyres[0]=race->pit_tyres[1]=GpTyreMedium;
    race->rng_state=seed?seed:0xF1002026u;
    race->incident_rng=seed^0x7202026u;if(!race->incident_rng)race->incident_rng=1;
    race->incident_driver=GP_DRIVER_COUNT;
    gp_select_driver(race,slot);
    for(uint8_t i=0;i<GP_DRIVER_COUNT;++i) {
        GpDriver* d=&race->drivers[i];
        d->driver_index=i; d->wear=100; d->tyre=GpTyreMedium;
        d->time_ms=(100u-gp_teams[i/2].rating)*60u+
                   (100u-gp_names[i].rating)*30u+gp_random(race)%2800u;
        race->ranking[i]=i;
    }
    gp_rank(race);
    gp_weather_start(&race->weather,seed^0x700700u);
    GpTyre tyre=(GpTyre)gp_weather_tyre(&race->weather);
    for(uint8_t i=0;i<GP_DRIVER_COUNT;i++){race->drivers[i].tyre=tyre;race->drivers[i].temperature=gp_tyre_optimal(tyre);}
    race->pit_tyres[0]=race->pit_tyres[1]=tyre;
}

/* Use qualification order to set starting positions; random race dynamics still apply. */
void gp_race_apply_grid(GpRace* race, const uint8_t grid[GP_DRIVER_COUNT]) {
    if(!grid) return;
    bool seen[GP_DRIVER_COUNT]={0};
    for(uint8_t pos=0;pos<GP_DRIVER_COUNT;++pos) {
        if(grid[pos]>=GP_DRIVER_COUNT || seen[grid[pos]])return;
        seen[grid[pos]]=true;
    }
    for(uint8_t pos=0;pos<GP_DRIVER_COUNT;++pos) {
        race->drivers[grid[pos]].time_ms=(uint32_t)pos*850u;
        race->ranking[pos]=grid[pos];
    }
}

uint8_t gp_position(const GpRace* race,uint8_t driver_index) {
    for(uint8_t i=0;i<GP_DRIVER_COUNT;++i) if(race->ranking[i]==driver_index)return i+1;
    return GP_DRIVER_COUNT;
}
uint32_t gp_gap_ms(const GpRace* race,uint8_t id) {
    if(race->lap==0||race->drivers[id].status!=GpDriverRunning)return 0;
    uint32_t time=race->drivers[id].time_ms;
    uint32_t leader=race->drivers[race->ranking[0]].time_ms;
    return time>leader?time-leader:0;
}
uint8_t gp_points_for_position(uint8_t pos) {
    static const uint8_t points[]={25,18,15,12,10,8,6,4,2,1};
    return (pos>=1&&pos<=10)?points[pos-1]:0;
}

bool gp_set_event(GpRace* race,GpEvent event,uint8_t laps){
    if((unsigned)event>GpEventRed||gp_race_finished(race))return false;
    if(event!=GpEventNone&&event!=GpEventRed&&(laps<1||laps>4))return false;
    if(race->event!=event)race->event_serial++;
    race->event=event;race->event_remaining=(event==GpEventNone||event==GpEventRed)?0:laps;return true;
}
bool gp_retire_driver(GpRace* race,uint8_t id,GpDriverStatus cause){
    if(id>=GP_DRIVER_COUNT||cause==GpDriverRunning||(unsigned)cause>GpDriverFailure||!race->lap||gp_race_finished(race)||race->drivers[id].status!=GpDriverRunning)return false;
    race->drivers[id].status=cause;race->drivers[id].retired_lap=race->lap;
    if(id/2==race->team)race->pit_queued[id%2]=false;
    race->incident_driver=id;race->incident_serial++;race->incident_pending=true;gp_rank(race);
    if(!gp_running_count(race)){race->event=GpEventNone;race->event_remaining=0;}
    return true;
}
bool gp_restart_race(GpRace* race){
    if(race->event!=GpEventRed||gp_race_finished(race))return false;
    uint32_t leader=race->drivers[race->ranking[0]].time_ms;unsigned position=0;
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){
        GpDriver* d=&race->drivers[race->ranking[i]];if(d->status!=GpDriverRunning)continue;
        d->tyre=d->driver_index/2==race->team?race->pit_tyres[d->driver_index%2]:(GpTyre)gp_weather_tyre(&race->weather);
        d->wear=100;d->wear_fraction=0;d->temperature=gp_tyre_blanket(d->tyre);
        d->time_ms=leader+position++*700u;
    }
    race->pit_queued[0]=race->pit_queued[1]=false;race->incident_pending=false;
    return gp_set_event(race,GpEventSafetyCar,1);
}
static uint32_t incident_random(GpRace* race){uint32_t x=race->incident_rng;x^=x<<13;x^=x>>17;x^=x<<5;return race->incident_rng=x;}
static void incidents(GpRace* race){
    if(!race->incident_rng||race->event!=GpEventNone||race->lap<2||race->lap>=race->total_laps-1)return;
    uint8_t id=race->ranking[incident_random(race)%gp_running_count(race)];
    GpDriver* d=&race->drivers[id];GpPace pace=id/2==race->team?race->pace[id%2]:GpPaceNormal;
    unsigned risk=35000;
    if(pace==GpPaceAttack)risk+=25000;
    if(race->weather.wetness>=75)risk+=15000;
    if(d->tyre<=GpTyreHard&&race->weather.wetness>35)risk+=40000;
    if(d->wear<15)risk+=30000;
    unsigned chance=incident_random(race)%100000u,crash=risk/race->total_laps,failure=18000u/race->total_laps;
    if(chance<crash){
        gp_retire_driver(race,id,GpDriverAccident);
        unsigned flag=incident_random(race)%4;
        gp_set_event(race,flag==0?GpEventRed:(flag==1?GpEventVsc:GpEventSafetyCar),2+incident_random(race)%3);
        if(race->event==GpEventRed)for(unsigned slot=0;slot<2;slot++)race->pit_tyres[slot]=race->drivers[race->team*2+slot].tyre;
    }else if(chance<crash+failure){
        gp_retire_driver(race,id,GpDriverFailure);gp_set_event(race,GpEventVsc,2);
    }else if(chance<crash+failure+12000u/race->total_laps){
        gp_set_event(race,GpEventYellow,1);
    }
}

uint32_t gp_pit_loss_ms(const GpRace* race){
    uint32_t loss=21000u+(uint32_t)gp_track_info(race->track)->difficulty*520u;
    if(race->event==GpEventVsc)loss-=7500;
    if(race->event==GpEventSafetyCar)loss-=10500;
    return loss;
}
uint8_t gp_pit_rejoin_position(const GpRace* race,uint8_t driver){
    if(race->drivers[driver].status!=GpDriverRunning)return 0;
    uint64_t rejoin=(uint64_t)race->drivers[driver].time_ms+gp_pit_loss_ms(race);
    uint8_t position=1;
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++)if(i!=driver&&race->drivers[i].status==GpDriverRunning&&race->drivers[i].time_ms<=rejoin)position++;
    return position;
}
static bool pit_traffic(const GpRace* race,uint8_t driver,const uint32_t* previous){
    if(race->drivers[driver].status!=GpDriverRunning)return false;
    uint64_t rejoin=(uint64_t)(previous?previous[driver]:race->drivers[driver].time_ms)+gp_pit_loss_ms(race);
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++)if(i!=driver&&race->drivers[i].status==GpDriverRunning){
        uint32_t time=previous?previous[i]:race->drivers[i].time_ms;
        if(time<=rejoin&&rejoin-time<1800u)return true;
    }
    return false;
}
bool gp_pit_traffic(const GpRace* race,uint8_t driver){return pit_traffic(race,driver,NULL);}

static void gp_apply_one(GpRace* race,uint8_t index,const uint32_t previous[GP_DRIVER_COUNT]) {
    GpDriver* d=&race->drivers[index];
    if(d->status!=GpDriverRunning)return;
    const GpTrackInfo* track=gp_track_info(race->track);
    const GpDriverInfo* info=gp_driver_info(index);
    const GpTeamInfo* team=gp_team_info(info->team);
    bool own=info->team==race->team;
    uint8_t slot=index%2;
    GpPace pace=own?race->pace[slot]:GpPaceNormal;
    bool neutral=race->event==GpEventSafetyCar||race->event==GpEventVsc;
    if(neutral)pace=GpPaceConserve;
    bool pit=false;
    GpTyre next=d->tyre;

    if(own && race->pit_queued[slot]) {
        pit=true; next=race->pit_tyres[slot]; race->pit_queued[slot]=false;
    } else if(!own && race->lap<race->total_laps-2) {
        bool traffic=pit_traffic(race,index,previous);
        bool early=!traffic&&d->wear<40;
        bool cheap=race->event==GpEventSafetyCar&&d->wear<55;
        if(d->wear<18||early||cheap){
            pit=true;next=(race->lap>race->total_laps-5)?GpTyreSoft:GpTyreMedium;
        }
    }
    if(!own){
        GpTyre suitable=(GpTyre)gp_weather_tyre(&race->weather);
        if(gp_weather_penalty(&race->weather,d->tyre)-gp_weather_penalty(&race->weather,suitable)>4500){pit=true;next=suitable;}
        else if(pit&&suitable>=GpTyreIntermediate)next=suitable;
    }
    /* Stops occur at the start of the simulated lap, so chosen tyres affect it. */
    if(pit){d->tyre=next;d->wear=100;d->wear_fraction=0;d->temperature=gp_tyre_blanket(next);++d->pits;}
    GpTyreThermal thermal=gp_tyre_thermal_step(d->tyre,d->temperature,pace,track,&race->weather,race->event);
    /* Lower team/driver rating makes slower times. Local strategy matters. */
    int32_t laptime=(int32_t)track->length_m*14+9500;
    laptime+=(100-(int32_t)info->rating)*95;
    laptime+=(100-(int32_t)team->rating)*135;
    laptime+=(int32_t)(gp_random(race)%2201u)-1100;
    if(pace==GpPaceConserve)laptime+=1650;
    if(pace==GpPaceAttack)laptime-=1700;
    if(d->tyre==GpTyreSoft)laptime-=1400;
    if(d->tyre==GpTyreHard)laptime+=550;
    laptime+=gp_weather_penalty(&race->weather,d->tyre)+thermal.loss_ms;
    if(d->wear<65)laptime+=(65-d->wear)*27;
    if(d->wear<35)laptime+=(35-d->wear)*95;
    if(d->wear<15)laptime+=(15-d->wear)*140;
    /* Small additional penalty for nearby traffic in the classification. */
    uint8_t pos=gp_position(race,index);
    if(!neutral && !pit && pos>1 && race->lap>0) {
        uint8_t ahead=race->ranking[pos-2];
        uint32_t their_time=previous[ahead];
        if(d->time_ms>=their_time && d->time_ms-their_time<1600u)
            laptime+=400+((uint32_t)(track->difficulty)*105u);
    }
    if(race->event==GpEventYellow)laptime+=4300;
    if(race->event==GpEventVsc)laptime+=9200;
    if(race->event==GpEventSafetyCar)laptime+=14500;

    if(neutral)laptime=(int32_t)track->length_m*18+12000;
    if(pit){
        laptime+=(int32_t)gp_pit_loss_ms(race);
        if(!neutral&&pit_traffic(race,index,previous))laptime+=600+track->difficulty*300;
    }
    int wear=(int)track->tyre_wear;
    if(d->tyre==GpTyreSoft)wear+=5;
    if(d->tyre==GpTyreHard)wear-=3;
    wear+=(int)gp_weather_wear(&race->weather,d->tyre)+thermal.wear_extra;
    d->temperature=thermal.temperature;
    if(pace==GpPaceAttack)wear+=4;
    if(pace==GpPaceConserve)wear-=3;
    if(race->event==GpEventSafetyCar)wear-=3;
    if(wear<1)wear=1;
    unsigned scaled=(unsigned)wear*GP_TOTAL_LAPS+d->wear_fraction;
    wear=(int)(scaled/race->total_laps);
    d->wear_fraction=(uint8_t)(scaled%race->total_laps);
    d->wear=(d->wear>(uint8_t)wear)?(uint8_t)(d->wear-wear):0;
    d->last_lap_ms=(laptime>1000)?(uint32_t)laptime:1000u;
    d->time_ms+=d->last_lap_ms;
}

void gp_advance_lap(GpRace* race) {
    if(gp_race_finished(race)||race->event==GpEventRed)return;
    GpEvent active=race->event;
    uint32_t previous[GP_DRIVER_COUNT];
    for(uint8_t i=0;i<GP_DRIVER_COUNT;++i)previous[i]=race->drivers[i].time_ms;
    for(uint8_t i=0;i<GP_DRIVER_COUNT;++i)gp_apply_one(race,i,previous);
    ++race->lap;gp_weather_step(&race->weather);gp_rank(race);
    if(active==GpEventSafetyCar){
        uint32_t lead=race->drivers[race->ranking[0]].time_ms;
        for(unsigned i=1;i<gp_running_count(race);i++){
            GpDriver* d=&race->drivers[race->ranking[i]];uint32_t gap=d->time_ms-lead,target=i*700u;
            if(gap>target)d->time_ms=lead+(gap+target)/2;
        }
    }
    if(gp_race_finished(race)){race->event=GpEventNone;race->event_remaining=0;return;}
    if(active!=GpEventNone){
        if(race->event_remaining)race->event_remaining--;
        if(!race->event_remaining)gp_set_event(race,GpEventNone,0);
    }else incidents(race);
}
