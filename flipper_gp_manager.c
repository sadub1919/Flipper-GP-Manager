#include <furi.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "race_engine.h"
#include "tyre_model.h"
#include "race_clock.h"
#include "gp_storage.h"
#include "qualifying.h"
#include "ru_font.h"
#include "track_shapes.h"
#include "team_logos.h"
#include "game_logo.h"

/* v0.8.0: championship, player roles and independent save profiles. */
#define GP_GAME_LAP_MS GP_LAP_MS

typedef enum {
    ScreenTeam, ScreenTrack, ScreenQualifying, ScreenGrid,
    ScreenMap, ScreenStandings, ScreenDriver, ScreenStrategy,
    ScreenResults, ScreenHelp, ScreenPause, ScreenMain, ScreenDistance, ScreenReplace, ScreenNotice, ScreenAbandon, ScreenQualResults, ScreenQualExit, ScreenWeekend, ScreenWeekendReplace, ScreenCalendar, ScreenWeather, ScreenRedFlag, ScreenEngineer, ScreenRole, ScreenSlots, ScreenChampionship, ScreenHistory, ScreenHistoryResult, ScreenChampions,
} GpScreen;

typedef struct {
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    GpRace race;
    GpRaceClock clock;
    GpQualifying qualifying;
    GpWeekend weekend;
    bool new_weekend;
    GpScreen screen;
    GpScreen help_return;
    GpScreen notice_return;
    GpScreen qual_return;
    GpScreen weather_return;
    GpScreen engineer_return;
    bool engineer_battle;
    GpSaveStatus save_status;
    GpSaveIo storage;
    bool full_distance,save_requested,save_exit,load_requested,autosave_blocked;
    GpMode chosen_mode;
    bool championship_teams,history_sprint;
    uint8_t history_track,slot_choice;
    uint8_t pause_item;
    uint8_t chosen_team,chosen_track,preferred_driver;
    uint8_t standings_top,grid_top,help_page,menu_item;
    uint32_t last_tick;
    bool running;
} GpApp;

static const char* const track_ru[GP_TRACK_COUNT] = {
    "МЕЛЬБУРН", "ШАНХАЙ", "СУЗУКА", "МАЙАМИ", "МОНРЕАЛЬ", "МОНАКО",
    "БАРСЕЛОНА", "ШПИЛЬБЕРГ", "СИЛЬВЕРСТОУН", "СПА", "ХУНГАРОРИНГ",
    "ЗАНДВОРТ", "МОНЦА", "МАДРИД", "БАКУ", "СЕПАНГ", "СИНГАПУР",
    "ОСТИН", "МЕХИКО", "ИНТЕРЛАГОС", "ЛАС-ВЕГАС", "ЛУСАИЛ", "ЯС-МАРИНА"
};

/* Layout contract: header y=0..9, divider y=11, content 13..51,
   divider y=52, footer y=54..63. No label should cross a band. */
static void txt(Canvas* c,int32_t x,int32_t y,const char* s){
    canvas_set_color(c,ColorBlack);gp_ru_text(c,x,y,s);
}
static void latin(Canvas* c,int32_t x,int32_t baseline,const char* s){
    txt(c,x,baseline-9,s);
}
static void top(Canvas* c,const char* s){
    txt(c,2,0,s);canvas_draw_line(c,0,11,127,11);
}
static void bottom(Canvas* c,const char* s){
    canvas_set_color(c,ColorBlack);canvas_draw_line(c,0,52,127,52);txt(c,2,54,s);
}
static void selection_frame(Canvas* c,int x,int y,int w,int h){
    canvas_set_color(c,ColorBlack);canvas_draw_frame(c,x,y,w,h);
}
static char tyre_char(GpTyre t){static const char letters[]="SMHIW";return (unsigned)t<GpTyreCount?letters[t]:'?';}
static const char* pace_ru(GpPace p){
    return p==GpPaceAttack?"АТАКА":(p==GpPaceConserve?"ЭКО":"НОРМА");
}
static const char* event_short(GpEvent e){
    return e==GpEventRed?"RED":e==GpEventSafetyCar?"SC":(e==GpEventVsc?"VSC":(e==GpEventYellow?"YEL":"GRN"));
}
/* The generated coordinates encode an 80:39 box. Undo that normalization
   before rotation; a single scale preserves the actual circuit proportions. */
static void fit_circuit(const GpTrackShape* source,uint8_t* xy,int x,int y,int w,int h){
    static const int16_t cs[12]={1000,966,866,707,500,259,0,-259,-500,-707,-866,-966};
    static const int16_t sn[12]={0,259,500,707,866,966,1000,966,866,707,500,259};
    int best=0;int32_t best_scale=0,bx=0,by=0,bw=0,bh=0;
    for(int angle=0;angle<12;angle++){
        int32_t minx=INT32_MAX,miny=INT32_MAX,maxx=INT32_MIN,maxy=INT32_MIN;
        for(int i=0;i<source->count;i++){
            int32_t px=source->xy[2*i]*80,py=source->xy[2*i+1]*39;
            int32_t rx=(px*cs[angle]-py*sn[angle])/1000;
            int32_t ry=(px*sn[angle]+py*cs[angle])/1000;
            if(rx<minx)minx=rx;
            if(rx>maxx)maxx=rx;
            if(ry<miny)miny=ry;
            if(ry>maxy)maxy=ry;
        }
        int32_t dx=maxx-minx,dy=maxy-miny;
        if(!dx||!dy)continue;
        int32_t scale=w*10000/dx,ys=h*10000/dy;
        if(ys<scale)scale=ys;
        if(scale>best_scale){best=angle;best_scale=scale;bx=minx;by=miny;bw=dx;bh=dy;}
    }
    for(int i=0;i<source->count;i++){
        int32_t px=source->xy[2*i]*80,py=source->xy[2*i+1]*39;
        int32_t rx=(px*cs[best]-py*sn[best])/1000;
        int32_t ry=(px*sn[best]+py*cs[best])/1000;
        xy[2*i]=x+(w-bw*best_scale/10000)/2+(rx-bx)*best_scale/10000;
        xy[2*i+1]=y+(h-bh*best_scale/10000)/2+(ry-by)*best_scale/10000;
    }
}
typedef struct {int x,y,lx,ly;uint8_t id;} GpMapMarker;
static uint32_t circuit_segment_length(const uint8_t* a,unsigned i){
    int dx=(int)a[2*i+2]-a[2*i],dy=(int)a[2*i+3]-a[2*i+1];
    uint32_t squared=(uint32_t)(dx*dx+dy*dy)*65536u,root=0;
    for(uint32_t bit=1u<<15;bit;bit>>=1){uint32_t n=root|bit;if(n*n<=squared)root=n;}
    return root;
}
static void circuit_point(const GpTrackShape* t,uint8_t phase,int* x,int* y){
    /* Stream segment lengths: the GUI callback must not put a 360-byte
       temporary array on the system GUI thread's small stack. */
    const uint8_t* a=t->xy;uint32_t total=0;
    for(uint8_t i=0;i<t->count-1;i++)total+=circuit_segment_length(a,i);
    uint32_t travel=(uint32_t)phase*total/256u;uint8_t idx=0;
    uint32_t length=circuit_segment_length(a,idx);
    while(idx<t->count-2&&travel>=length){
        travel-=length;length=circuit_segment_length(a,++idx);
    }
    uint32_t frac=length?travel*256u/length:0;
    *x=((int)a[2*idx]*(256-(int)frac)+(int)a[2*idx+2]*(int)frac)/256;
    *y=((int)a[2*idx+1]*(256-(int)frac)+(int)a[2*idx+3]*(int)frac)/256;
}
static bool rectangles_overlap(int x,int y,int w,int h,int bx,int by,int bw,int bh){
    return x<bx+bw&&x+w>bx&&y<by+bh&&y+h>by;
}
static unsigned map_markers(const GpTrackShape* t,const GpRace* race,uint8_t phase,GpMapMarker m[3]){
    if(race->drivers[race->player].status!=GpDriverRunning)return 0;
    unsigned count=1;m[0].id=race->player;uint8_t pos=gp_position(race,race->player);
    if(pos>1)m[count++].id=race->ranking[pos-2];
    if(pos<gp_running_count(race))m[count++].id=race->ranking[pos];
    int32_t lap=race->drivers[race->player].last_lap_ms;
    if(lap<1000)lap=gp_track_info(race->track)->length_m*14+9500;
    for(unsigned i=0;i<count;i++){
        int64_t delta=(int64_t)race->drivers[m[i].id].time_ms-race->drivers[race->player].time_ms;
        int32_t p=phase-(int32_t)(delta*256/lap);p=(p%256+256)%256;
        circuit_point(t,(uint8_t)p,&m[i].x,&m[i].y);
    }
    /* Search nearest free label rectangles. Labels may cover the outline, but
       never another label, a marker, or the header/footer. */
    for(unsigned i=0;i<count;i++){
        int best=INT32_MAX;m[i].lx=2;m[i].ly=13;
        for(int y=13;y<=41;y+=2)for(int x=2;x<=108;x+=2){
            bool free=true;
            for(unsigned j=0;j<i;j++)if(rectangles_overlap(x-1,y-1,20,12,m[j].lx,m[j].ly,18,10))free=false;
            for(unsigned j=0;j<count;j++)if(rectangles_overlap(x-1,y-1,20,12,m[j].x-3,m[j].y-3,7,7))free=false;
            if(!free)continue;
            int dx=x+9-m[i].x,dy=y+10-m[i].y;
            int cost=dx*dx+dy*dy+(y>=m[i].y?100:0);
            if(cost<best){best=cost;m[i].lx=x;m[i].ly=y;}
        }
    }
    return count;
}
static void draw_circuit(Canvas* c,uint8_t track,int x,int y,int w,int h,const GpRace* race,uint8_t phase){
    const GpTrackShape* source=gp_track_shape(track);
    if(source->count<3||source->count>90)return;
    uint8_t xy[180];fit_circuit(source,xy,x,y,w,h);
    GpTrackShape fitted={xy,source->count};const GpTrackShape* t=&fitted;
    x=0;y=0;w=100;h=100;
    const uint8_t* a=t->xy;canvas_set_color(c,ColorBlack);
    for(uint8_t i=1;i<t->count;++i)
        canvas_draw_line(c,x+(int)a[2*(i-1)]*w/100,y+(int)a[2*(i-1)+1]*h/100,
                         x+(int)a[2*i]*w/100,y+(int)a[2*i+1]*h/100);
    if(!race)return;
    GpMapMarker markers[3];unsigned count=map_markers(t,race,phase,markers);
    for(unsigned i=0;i<count;i++){
        GpMapMarker* m=&markers[i];
        canvas_draw_line(c,m->x,m->y,m->lx+9,m->ly+(m->ly<m->y?10:0));
    }
    for(unsigned i=0;i<count;i++){
        GpMapMarker* m=&markers[i];
        canvas_set_color(c,ColorWhite);
        for(int row=-1;row<11;row++)canvas_draw_line(c,m->lx-1,m->ly+row,m->lx+18,m->ly+row);
        txt(c,m->lx,m->ly,gp_driver_info(m->id)->code);
    }
    for(int i=(int)count-1;i>=0;i--){
        GpMapMarker* m=&markers[i];canvas_set_color(c,ColorWhite);canvas_draw_disc(c,m->x,m->y,3);
        canvas_set_color(c,ColorBlack);
        if(i==0)canvas_draw_disc(c,m->x,m->y,2);else canvas_draw_circle(c,m->x,m->y,2);
    }
}
static void draw_team(Canvas* c,const GpApp* a){
    char b[40];
    snprintf(b,sizeof(b),"КОМАНДА %02u/11",a->chosen_team+1);top(c,b);
    gp_logo_draw(c,a->chosen_team,3,15);
    latin(c,54,23,gp_team_info(a->chosen_team)->name);
    for(uint8_t slot=0;slot<2;++slot){
        uint8_t id=(uint8_t)(a->chosen_team*2+slot);
        uint8_t y=(uint8_t)(26+slot*12);
        if(a->preferred_driver==slot)selection_frame(c,53,y,74,12);
        const char* pilot=gp_driver_info(id)->short_name;
        if(strlen(pilot)>11)pilot+=2;
        snprintf(b,sizeof(b),"%s",pilot);
        latin(c,57,y+9,b);
    }
    bottom(c,"<> КОМ. ^V ПИЛОТ OK");
}
static void draw_track(Canvas* c,const GpApp* a){
    char b[40];
    snprintf(b,sizeof(b),"%02u %s",a->chosen_track+1,track_ru[a->chosen_track]);top(c,b);
    draw_circuit(c,a->chosen_track,2,14,79,36,NULL,0);
    snprintf(b,sizeof(b),"%u КР",gp_track_info(a->chosen_track)->actual_laps);txt(c,85,18,b);
    txt(c,85,32,"ДЛИНА");
    snprintf(b,sizeof(b),"%u КМ",gp_track_info(a->chosen_track)->length_m/1000);txt(c,85,42,b);
    bottom(c,"<> ТРАССА OK ДАЛЕЕ");
}
static void draw_qualifying(Canvas* c,const GpApp* a){
    const GpQualifying* q=&a->qualifying;uint8_t slot=a->preferred_driver,id=q->team*2+slot;
    char b[48];
    if(a->weekend.active&&a->weekend.stage<=GpStageFP3)snprintf(b,sizeof(b),"%s ПОПЫТКА %u/2",gp_stage_name(a->weekend.stage),q->run+1);
    else snprintf(b,sizeof(b),"%s%u ПОПЫТКА %u/2",a->weekend.active&&a->weekend.stage==GpStageSprintQual?"SQ":"Q",q->session,q->run+1);
    top(c,b);
    snprintf(b,sizeof(b),"%s %s%u%%",gp_driver_info(id)->code,
        gp_qualifying_active(q,id)?"ВЛ ":"ВЫБЫЛ ВЛ ",q->weather.wetness);txt(c,2,15,b);
    uint8_t fresh=q->soft_used[slot]<GP_QUAL_SOFT_SETS?GP_QUAL_SOFT_SETS-q->soft_used[slot]:0;
    snprintf(b,sizeof(b),"^V %c  SOFT:%u",tyre_char(q->tyre[slot]),fresh);txt(c,2,28,b);
    snprintf(b,sizeof(b),"<> %s OK: ВЫЕЗД",pace_ru(q->pace[slot]));txt(c,2,41,b);
    bottom(c,q->mode==GpModeDriver?"НАПАРНИК: ИИ":"ДЕРЖИ OK: 2-Й ПИЛОТ");
}
static void draw_qual_results(Canvas* c,const GpApp* a){
    const GpQualifying* q=&a->qualifying;char b[48];
    if(a->weekend.active&&a->weekend.stage<=GpStageFP3)snprintf(b,sizeof(b),"%s %u/2 РЕЗУЛЬТАТЫ",gp_stage_name(a->weekend.stage),q->run);
    else snprintf(b,sizeof(b),"%s%u %u/2 %s",a->weekend.active&&a->weekend.stage==GpStageSprintQual?"SQ":"Q",q->session,q->run,q->session==1?"ТОП16":(q->session==2?"ТОП10":"ПОУЛ"));
    top(c,b);
    for(uint8_t row=0;row<3;row++){
        uint8_t pos=a->grid_top+row;if(pos>=q->active_count)break;
        uint8_t id=q->grid[pos];uint32_t tm=q->lap_ms[id];
        if(id/2==q->team)selection_frame(c,1,13+row*12,126,12);
        if(q->temperature[id])snprintf(b,sizeof(b),"%02u %s %lu.%03lu %03uC",pos+1,gp_driver_info(id)->code,
            (unsigned long)(tm/1000),(unsigned long)(tm%1000),q->temperature[id]);
        else snprintf(b,sizeof(b),"%02u %s %lu.%03lu",pos+1,gp_driver_info(id)->code,
            (unsigned long)(tm/1000),(unsigned long)(tm%1000));
        txt(c,4,13+row*12,b);
    }
    bottom(c,q->complete?"^V СПИСОК OK РЕШЁТКА":(q->run<2?"^V СПИСОК OK ПОПЫТКА":"^V СПИСОК OK ДАЛЕЕ"));
}
static void draw_weather(Canvas* c,const GpApp* a){
    const GpWeather* w=a->weather_return==ScreenQualifying?&a->qualifying.weather:&a->race.weather;
    char b[48];top(c,"ПОГОДА / ИНЖЕНЕР");
    snprintf(b,sizeof(b),"%s ВЛ:%u%%",w->rain?gp_rain_name(w->rain):(w->wetness?"СОХНЕТ":"СУХО"),w->wetness);txt(c,2,15,b);
    snprintf(b,sizeof(b),"ШИНЫ: %c",tyre_char((GpTyre)gp_weather_tyre(w)));txt(c,2,28,b);
    if(w->rng)snprintf(b,sizeof(b),"ЧЕРЕЗ %u: %s",w->remaining,gp_rain_name(w->forecast));
    else snprintf(b,sizeof(b),"ПРОГНОЗ: СУХО");
    txt(c,2,41,b);
    bottom(c,a->weather_return==ScreenQualifying?"ШАГ: ВЫЕЗД BACK НАЗАД":"ШАГ: КРУГ BACK НАЗАД");
}
static void draw_weekend(Canvas* c,const GpApp* a){
    char b[48];const GpWeekend* w=&a->weekend;
    snprintf(b,sizeof(b),"ЭТАП %02u/23 %s",w->rounds_done<23?w->rounds_done+1:23,
        gp_weekend_sprint(w,a->race.track)?"СПРИНТ":"ГРАН-ПРИ");top(c,b);
    txt(c,2,15,track_ru[a->race.track]);
    txt(c,2,28,gp_stage_name(w->stage));
    txt(c,2,41,w->incomplete_history?"НЕПОЛНЫЙ ЗАЧЁТ":"> ЗАЧЁТ  ^ ИСТОРИЯ");
    bottom(c,w->stage==GpStageDone?"OK: ИТОГИ СЕЗОНА":(w->phase==GpWeekendResult?"OK: СЛЕД. СЕССИЯ":"< КАЛЕНДАРЬ OK СТАРТ"));
}
static void draw_championship(Canvas* c,const GpApp* a){
    const GpWeekend* w=&a->weekend;bool teams=a->championship_teams;char b[48];
    top(c,w->incomplete_history?"НЕПОЛНЫЙ ЗАЧЁТ":(teams?"КУБОК КОНСТРУКТОРОВ":"ЧЕМПИОНАТ ПИЛОТОВ"));
    for(unsigned row=0;row<3;row++){
        unsigned p=a->standings_top+row;if(p>=(teams?GP_TEAM_COUNT:GP_DRIVER_COUNT))break;
        unsigned id=teams?w->team_order[p]:w->driver_order[p];
        snprintf(b,sizeof(b),"%02u %s %u ОЧК",p+1,teams?gp_team_info(id)->code:gp_driver_info(id)->code,teams?w->team_points[id]:w->driver_points[id]);txt(c,4,15+row*12,b);
    }
    bottom(c,"<> ЗАЧЁТ BACK НАЗАД");
}
static void draw_champions(Canvas* c,const GpApp* a){
    const GpWeekend* w=&a->weekend;char b[48];top(c,w->incomplete_history?"ИТОГИ ЧАСТИ СЕЗОНА":"ЧЕМПИОНЫ СЕЗОНА");
    snprintf(b,sizeof(b),"ПИЛОТ: %s",gp_driver_info(w->driver_order[0])->code);txt(c,2,15,b);
    snprintf(b,sizeof(b),"КОМАНДА: %s",gp_team_info(w->team_order[0])->code);txt(c,2,28,b);
    txt(c,2,41,w->incomplete_history?"РАННИЕ ОЧКИ НЕ УЧТЕНЫ":"23 ЭТАПА ЗАВЕРШЕНЫ");bottom(c,"OK МЕНЮ BACK НАЗАД");
}
static void draw_history(Canvas* c,const GpApp* a,bool detail){
    const GpRoundResult* result=&a->weekend.results[a->history_track][a->history_sprint];char b[48];
    snprintf(b,sizeof(b),"%s %s",detail?(a->history_sprint?"S":"GP"):"ЭТАП",track_ru[a->history_track]);top(c,b);
    if(!detail){
        txt(c,2,15,"РЕЗУЛЬТАТЫ ЭТАПА");
        for(unsigned k=0;k<2;k++){
            const GpRoundResult* r=&a->weekend.results[a->history_track][k];
            snprintf(b,sizeof(b),"%s: %s",k?"СПРИНТ":"ГОНКА",r->recorded?gp_driver_info(r->finish[0]&127)->code:"НЕТ ДАННЫХ");txt(c,4,28+k*13,b);
        }
        selection_frame(c,1,a->history_sprint?39:26,126,12);bottom(c,"<> ЭТАП OK РЕЗУЛЬТАТ");
    }else{
        if(!result->recorded)txt(c,2,28,"НЕТ ДАННЫХ");
        else for(unsigned row=0;row<3;row++){
            unsigned p=a->standings_top+row;if(p>=GP_DRIVER_COUNT)break;uint8_t id=result->finish[p];
            if(id&128)snprintf(b,sizeof(b),"%02u %s DNF",p+1,gp_driver_info(id&127)->code);
            else snprintf(b,sizeof(b),"%02u %s %u ОЧК",p+1,gp_driver_info(id)->code,a->history_sprint?(p<8?8-p:0):gp_points_for_position(p+1));
            txt(c,4,15+row*12,b);
        }
        bottom(c,"^V СПИСОК BACK НАЗАД");
    }
}
static void draw_calendar(Canvas* c,const GpApp* a){
    top(c,"КАЛЕНДАРЬ: 6 СПРИНТОВ");char b[48];
    for(unsigned row=0;row<3;row++){
        unsigned position=a->standings_top+row;if(position>=GP_TRACK_COUNT)break;
        unsigned completed=a->weekend.stage==GpStageDone?22:a->weekend.rounds_done;
        unsigned first=(a->race.track+GP_TRACK_COUNT-completed)%GP_TRACK_COUNT;
        unsigned track=(first+position)%GP_TRACK_COUNT;
        snprintf(b,sizeof(b),"%02u %c %s",position+1,gp_weekend_sprint(&a->weekend,track)?'S':'-',track_ru[track]);
        txt(c,2,14+row*12,b);
    }
    bottom(c,"^V СПИСОК BACK НАЗАД");
}
static void draw_grid(Canvas* c,const GpApp* a){
    top(c,a->weekend.active&&a->weekend.stage<=GpStageFP3?gp_stage_name(a->weekend.stage):(a->weekend.active&&a->weekend.stage==GpStageSprintQual?"РЕШЁТКА СПРИНТА":"СТАРТОВАЯ РЕШЁТКА"));
    char b[44];
    for(uint8_t row=0;row<3;++row){
        uint8_t pos=(uint8_t)(a->grid_top+row);
        if(pos>=GP_DRIVER_COUNT)break;
        uint8_t id=a->qualifying.grid[pos];
        if(id/2==a->chosen_team)selection_frame(c,1,13+row*12,126,12);
        uint32_t tm=a->qualifying.lap_ms[id];
        snprintf(b,sizeof(b),"%02u %-3s  %lu.%03lu",pos+1,gp_driver_info(id)->code,
                 (unsigned long)(tm/1000),(unsigned long)(tm%1000));
        latin(c,4,22+row*12,b);
    }
    bottom(c,a->weekend.active?"^V СПИСОК OK ДАЛЕЕ":"^V СПИСОК OK ГОНКА");
}
static void draw_retired(Canvas* c,const GpRace* r){
    const GpDriver* d=&r->drivers[r->player];char b[40];
    snprintf(b,sizeof(b),"%s DNF: %s",gp_driver_info(r->player)->code,gp_retirement_name(d->status));txt(c,2,15,b);
    snprintf(b,sizeof(b),"СХОД НА КРУГЕ %u",d->retired_lap);txt(c,2,28,b);
    txt(c,2,41,r->mode==GpModeDriver?"ГОНКА ПРОДОЛЖАЕТСЯ":"ДЕРЖИ OK: 2-Й ПИЛОТ");
}
static void draw_battle(Canvas* c,const GpApp* a){
    const GpRace* r=&a->race;char b[48];const GpBattleReport* report=&r->battle[r->active_slot];
    snprintf(b,sizeof(b),"БОРЬБА: %s",gp_driver_info(r->player)->code);top(c,b);
    if(r->drivers[r->player].status!=GpDriverRunning){draw_retired(c,r);bottom(c,"<> СВОДКА OK НАЗАД");return;}
    static const char* const names[]={"НЕТ БОРЬБЫ","ОБОГНАЛ","СДЕРЖАЛ","ЗАСТРЯЛ ЗА","ПРОПУСТИЛ","ОШИБКА ЗА"};
    if(report->result)snprintf(b,sizeof(b),"%s %s КР%u",names[report->result],gp_driver_info(report->opponent)->code,report->lap);
    else snprintf(b,sizeof(b),"НЕТ СООБЩЕНИЙ");
    txt(c,2,15,b);
    uint8_t pos=gp_position(r,r->player),count=gp_running_count(r);
    if(pos>1){
        uint8_t id=r->ranking[pos-2];uint32_t gap=r->drivers[r->player].time_ms-r->drivers[id].time_ms;
        if(gap>999999)gap=999999;
        snprintf(b,sizeof(b),"ВПЕР %s +%lu.%lu",gp_driver_info(id)->code,(unsigned long)(gap/1000),(unsigned long)(gap%1000/100));
    }else snprintf(b,sizeof(b),"ВПЕРЕДИ НИКОГО");
    txt(c,2,28,b);
    if(pos<count){
        uint8_t id=r->ranking[pos];uint32_t gap=r->drivers[id].time_ms-r->drivers[r->player].time_ms;
        if(gap>999999)gap=999999;
        snprintf(b,sizeof(b),"СЗАДИ %s +%lu.%lu",gp_driver_info(id)->code,(unsigned long)(gap/1000),(unsigned long)(gap%1000/100));
    }else snprintf(b,sizeof(b),"СЗАДИ НИКОГО");
    txt(c,2,41,b);bottom(c,"<> СВОДКА OK НАЗАД");
}
static void draw_engineer(Canvas* c,const GpApp* a){
    if(a->engineer_battle){draw_battle(c,a);return;}
    const GpRace* r=&a->race;char b[48];top(c,"ГОНОЧНЫЙ ИНЖЕНЕР");
    if(r->event_remaining==1)snprintf(b,sizeof(b),"ПОСЛЕДНИЙ КРУГ %s",event_short(r->event));
    else snprintf(b,sizeof(b),"%s: ЕЩЁ %u КР",event_short(r->event),r->event_remaining);
    if(r->event==GpEventNone)snprintf(b,sizeof(b),"ТРАССА ОТКРЫТА");
    txt(c,2,15,b);
    if(r->incident_driver<GP_DRIVER_COUNT){
        const GpDriver* d=&r->drivers[r->incident_driver];
        snprintf(b,sizeof(b),"%s: %s",gp_driver_info(r->incident_driver)->code,gp_retirement_name(d->status));txt(c,2,28,b);
        snprintf(b,sizeof(b),"СХОД НА КРУГЕ %u",d->retired_lap);txt(c,2,41,b);
    }else txt(c,2,28,"ПРОИСШЕСТВИЙ НЕТ");
    bottom(c,"<> БОРЬБА OK НАЗАД");
}
static void draw_red(Canvas* c,const GpApp* a){
    const GpRace* r=&a->race;char b[48];top(c,"КРАСНЫЙ ФЛАГ");
    if(r->incident_driver<GP_DRIVER_COUNT)snprintf(b,sizeof(b),"КР%u %s: %s",r->lap,gp_driver_info(r->incident_driver)->code,gp_retirement_name(r->drivers[r->incident_driver].status));
    else snprintf(b,sizeof(b),"КРУГ %u: ОСТАНОВКА",r->lap);
    txt(c,2,15,b);
    if(r->drivers[r->player].status==GpDriverRunning)snprintf(b,sizeof(b),"%s ШИНЫ: %c",gp_driver_info(r->player)->code,tyre_char(r->pit_tyres[r->active_slot]));
    else snprintf(b,sizeof(b),"%s DNF",gp_driver_info(r->player)->code);
    txt(c,2,28,b);txt(c,2,41,"OK: РЕСТАРТ ЗА SC");bottom(c,r->mode==GpModeDriver?"^V ШИНЫ OK РЕСТАРТ":"<> ПИЛОТ ^V ШИНЫ");
}
static void draw_map(Canvas* c,const GpApp* a){
    const GpRace* r=&a->race;char b[32];
    snprintf(b,sizeof(b),"КР%02u/%02u",r->lap>=r->total_laps?r->total_laps:r->lap+1,r->total_laps);top(c,b);
    if(r->drivers[r->player].status!=GpDriverRunning){draw_retired(c,r);bottom(c,"<> ЭКРАНЫ OK ПАУЗА");return;}
    uint8_t phase=gp_clock_phase(&a->clock,furi_ms_to_ticks(GP_GAME_LAP_MS));
    draw_circuit(c,r->track,5,17,117,30,r,phase);
    if(r->event_remaining)snprintf(b,sizeof(b),"P%02u %s%u ВЛ%u%%",gp_position(r,r->player),event_short(r->event),r->event_remaining,r->weather.wetness);
    else snprintf(b,sizeof(b),"P%02u %s ВЛ%u%% ^ПОГ",gp_position(r,r->player),event_short(r->event),r->weather.wetness);
    bottom(c,b);
}
static void draw_table(Canvas* c,const GpApp* a,bool result){
    const GpRace* r=&a->race;char b[40];
    if(result){snprintf(b,sizeof(b),"%s %u КРУГОВ",a->weekend.active&&a->weekend.stage==GpStageSprint?"СПРИНТ":"ФИНИШ",r->lap);top(c,b);}
    else top(c,"ПОЗИЦИИ");
    for(uint8_t row=0;row<3;++row){
        uint8_t pos=(uint8_t)(a->standings_top+row);
        if(pos>=GP_DRIVER_COUNT)break;
        uint8_t id=r->ranking[pos];
        if(id/2==r->team)selection_frame(c,1,13+row*12,126,12);
        uint32_t gap=gp_gap_ms(r,id);
        if(r->drivers[id].status!=GpDriverRunning)
            snprintf(b,sizeof(b),"%02u %s DNF %s",pos+1,gp_driver_info(id)->code,gp_retirement_name(r->drivers[id].status));
        else if(pos==0)
            snprintf(b,sizeof(b),"%02u %-3s %-3s LEAD",pos+1,gp_driver_info(id)->code,gp_team_info(id/2)->code);
        else if(r->lap==0)
            snprintf(b,sizeof(b),"%02u %-3s %-3s GRID",pos+1,gp_driver_info(id)->code,gp_team_info(id/2)->code);
        else
            snprintf(b,sizeof(b),"%02u %-3s %-3s +%lu.%01lu",pos+1,gp_driver_info(id)->code,
                     gp_team_info(id/2)->code,(unsigned long)((gap>999999?999999:gap)/1000),(unsigned long)(((gap>999999?999999:gap)%1000)/100));
        latin(c,4,22+row*12,b);
    }
    bottom(c,result?"^V СПИСОК OK ДАЛЕЕ":"<> ЭКРАНЫ ^V СПИСОК");
}
static void draw_driver(Canvas* c,const GpApp* a){
    const GpRace* r=&a->race;uint8_t slot=r->active_slot,index=r->player;
    const GpDriver* d=&r->drivers[index];char b[40];
    top(c,"ПИЛОТ");
    if(d->status!=GpDriverRunning){draw_retired(c,r);bottom(c,r->mode==GpModeDriver?"<> ЭКРАНЫ BACK МЕНЮ":"<> ЭКРАНЫ OK ПИЛОТ");return;}
    snprintf(b,sizeof(b),"%s P%02u ПИТ %u",gp_driver_info(index)->code,gp_position(r,index),d->pits);latin(c,4,22,b);
    snprintf(b,sizeof(b),"%s %c %u%%",pace_ru(r->pace[slot]),tyre_char(d->tyre),d->wear);txt(c,3,27,b);
    int state=gp_tyre_temperature_state(d->tyre,d->temperature);
    snprintf(b,sizeof(b),"T%03uC %s",d->temperature,state<0?"ХОЛОД":(state>0?"ПЕРЕГРЕВ":"НОРМА"));txt(c,3,40,b);
    bottom(c,r->mode==GpModeDriver?"^V ТЕМП BACK МЕНЮ":"^V ТЕМП  OK ПИЛОТ");
}
static void draw_strategy(Canvas* c,const GpApp* a){
    const GpRace* r=&a->race;uint8_t slot=r->active_slot;char b[38];
    top(c,"СТРАТЕГИЯ");
    if(r->drivers[r->player].status!=GpDriverRunning){draw_retired(c,r);bottom(c,"<> ЭКРАНЫ BACK МЕНЮ");return;}
    snprintf(b,sizeof(b),"ПИТ ~P%02u %s",gp_pit_rejoin_position(r,r->player),gp_pit_traffic(r,r->player)?"ТРАФИК":"ЧИСТО");latin(c,4,22,b);
    snprintf(b,sizeof(b),"ШИНЫ:%c ВЛ:%u%%",tyre_char(r->pit_tyres[slot]),r->weather.wetness);txt(c,3,27,b);
    txt(c,3,40,r->pit_queued[slot]?"ПИТ: ЗАПЛАНИРОВАН":"ПИТ: НЕ НАЗНАЧЕН");
    bottom(c,"^V ШИНЫ OK ПИТ");
}
static void draw_help(Canvas* c,const GpApp* a){
    char b[32];snprintf(b,sizeof(b),"ПОМОЩЬ %u/19",a->help_page+1);top(c,b);
    if(a->help_page==0){
        txt(c,2,15,"<>: ЭКРАНЫ");
        txt(c,2,28,"BACK: МЕНЮ ПАУЗЫ");
        txt(c,2,41,"ДЕРЖИ >: СКОРОСТЬ");
    }else if(a->help_page==1){
        txt(c,2,15,"ПИЛОТ: ^V ТЕМП");
        txt(c,2,28,"ДЕРЖИ OK: ПИЛОТ");
        txt(c,2,41,"ШИНЫ: ^V ВЫБОР");
    }else if(a->help_page==2){
        txt(c,2,15,"СТРАТЕГИЯ: OK ПИТ");
        txt(c,2,28,"ПОЗИЦИИ: ^V СПИСОК");
        txt(c,2,41,"ДЕРЖИ <: ПОМОЩЬ");
    }else if(a->help_page==3){
        txt(c,2,15,"ПИТЫ: 1 И 2 ВВЕРХУ");
        txt(c,2,28,"ЦИФРА: ПИТ НАЗНАЧЕН");
        txt(c,2,41,"ШИНЫ: ОСТАТОК В %");
    }else if(a->help_page==4){
        txt(c,2,15,"АВТОЗАПИСЬ НА SD");
        txt(c,2,28,"ПОСЛЕ КАЖДОГО КРУГА");
        txt(c,2,41,"SD!: НЕТ АВТОЗАПИСИ");
    }
    if(a->help_page==5){
        txt(c,2,15,"Q1 > Q2 > Q3");txt(c,2,28,"^V ШИНЫ <> ТЕМП");txt(c,2,41,"OK: ВЫЕЗД ОБОИХ");
    }else if(a->help_page==6){
        txt(c,2,15,"SOFT: 4 СВЕЖИХ");txt(c,2,28,"ПОТОМ S ИЗНОШЕННЫЕ");txt(c,2,41,"M/H: ВСЕГДА СВЕЖИЕ");
    }else if(a->help_page==7){
        txt(c,2,15,"АТАКА: РИСК ОШИБКИ");txt(c,2,28,"2 ПОПЫТКИ НА СЕССИЮ");txt(c,2,41,"ЛУЧШИЙ КРУГ В ЗАЧЁТ");
    }
    if(a->help_page==8){
        txt(c,2,15,"УИК-ЭНДЫ: АВТОЗАПИСЬ");txt(c,2,28,"BACK: МЕНЮ И ВЫХОД");txt(c,2,41,"ОДИН СЛОТ НА SD");
    }else if(a->help_page==9){
        txt(c,2,15,"6 СПРИНТОВ НА СЕЗОН");txt(c,2,28,"S: СПРИНТ В КАЛЕНДАРЕ");txt(c,2,41,"ОЧКОВ СЕЗОНА ПОКА НЕТ");
    }
    if(a->help_page==10){
        txt(c,2,15,"ДЕРЖИ ^: ПОГОДА");txt(c,2,28,"ВЛ: ВЛАЖНОСТЬ ТРАССЫ");txt(c,2,41,"I: ИНТЕР W: ДОЖДЕВЫЕ");
    }else if(a->help_page==11){
        txt(c,2,15,"ТОЧКА: ТВОЙ ПИЛОТ");txt(c,2,28,"КРУГИ: ВПЕРЕДИ/СЗАДИ");txt(c,2,41,"КАРТА ПРИБЛИЗИТЕЛЬНА");
    }
    if(a->help_page==12){
        txt(c,2,15,"ХОЛОД: НУЖЕН ПРОГРЕВ");txt(c,2,28,"ПЕРЕГРЕВ: СБАВЬ ТЕМП");txt(c,2,41,"ПОСЛЕ ПИТА ШИНЫ ХОЛ.");
    }else if(a->help_page==13){
        txt(c,2,15,"РАНЬШЕ: СВЕЖИЕ ШИНЫ");txt(c,2,28,"ПОЗЖЕ: ПРОГРЕТЫЕ ШИНЫ");txt(c,2,41,"~P: МЕСТО ПОСЛЕ ПИТА");
    }
    if(a->help_page==14){
        txt(c,2,15,"ДЕРЖИ V: ИНЖЕНЕР");txt(c,2,28,"SC1: ПОСЛЕДНИЙ КРУГ");txt(c,2,41,"DNF: ПИЛОТ ВЫБЫЛ");
    }else if(a->help_page==15){
        txt(c,2,15,"КРАСНЫЙ: ГОНКА СТОИТ");txt(c,2,28,"^V ШИНЫ <> ПИЛОТ");txt(c,2,41,"OK РЕСТАРТ BACK МЕНЮ");
    }
    if(a->help_page==16){
        txt(c,2,15,"ИНЖЕНЕР: <> СВОДКИ");txt(c,2,28,"ДЕРЖИ OK: ПИЛОТ");txt(c,2,41,"АТАКА: ШАНС И РИСК");
    }
    if(a->help_page==17){
        txt(c,2,15,"СЛОТЫ: ГЛАВНОЕ МЕНЮ");txt(c,2,28,"ГОНЩИК: НАПАРНИК ИИ");txt(c,2,41,"СЕЗОН: > ЗАЧЁТ");
    }else if(a->help_page==18){
        txt(c,2,15,"^ ИСТОРИЯ ЭТАПОВ");txt(c,2,28,"ЗАЧЁТ: <> ПИЛ/КОМ");txt(c,2,41,"СЛОТ 1: СТАРЫЙ ФАЙЛ");
    }
    bottom(c,"<> СТР. OK ЗАКРЫТЬ");
}
static void draw_cb(Canvas* c,void* ctx){
    GpApp* shared=ctx;
    if(shared->mutex)furi_mutex_acquire(shared->mutex,FuriWaitForever);
    /* Canvas work only: no storage or GUI registration while this lock is held.
       Render the protected model directly, without a full GpApp stack copy.
       Main-thread view_port_update/remove calls run outside the model lock. */
    const GpApp* a=shared;
    canvas_clear(c);canvas_set_color(c,ColorBlack);
    switch(a->screen){
    case ScreenMain:{
        for(unsigned y=0;y<10;y++)for(unsigned x=0;x<34;x++)
            if(gp_game_logo[y]&(1ULL<<x))canvas_draw_dot(c,2+x,y);
        char title[24];snprintf(title,sizeof(title),"GP 0.8.0 C%u",a->storage.profile+1);txt(c,42,0,title);canvas_draw_line(c,0,11,127,11);
        static const char* const items[]={"ПРОДОЛЖИТЬ","НОВЫЙ СЕЗОН","НОВАЯ ГОНКА","УПРАВЛЕНИЕ","ВЫХОД","СЛОТЫ"};
        uint8_t first=a->menu_item>2?a->menu_item-2:0;
        for(uint8_t row=0;row<3;row++)txt(c,8,15+row*13,items[first+row]);
        selection_frame(c,2,13+(a->menu_item-first)*13,124,13);
        bottom(c,"^V ВЫБОР OK ОТКРЫТЬ");break;}
    case ScreenRole:
        top(c,"РЕЖИМ ИГРЫ");txt(c,8,15,"ЗА КОМАНДУ");txt(c,8,28,"ЗА ГОНЩИКА");
        txt(c,2,41,a->chosen_mode==GpModeDriver?"НАПАРНИК: ИИ":"ОБА ПИЛОТА ВРУЧНУЮ");
        selection_frame(c,2,a->chosen_mode==GpModeDriver?26:13,124,12);bottom(c,"^V ВЫБОР OK ДАЛЕЕ");break;
    case ScreenSlots:{
        top(c,"СЛОТ СОХРАНЕНИЯ");char b[32];
        for(unsigned i=0;i<GP_SAVE_PROFILES;i++){snprintf(b,sizeof(b),"СЛОТ %u%s",i+1,i==a->storage.profile?" *":"");txt(c,8,15+i*13,b);}
        selection_frame(c,2,13+a->slot_choice*13,124,12);bottom(c,"^V ВЫБОР OK ПРИНЯТЬ");break;}
    case ScreenChampionship:draw_championship(c,a);break;
    case ScreenChampions:draw_champions(c,a);break;
    case ScreenHistory:draw_history(c,a,false);break;
    case ScreenHistoryResult:draw_history(c,a,true);break;
    case ScreenDistance:{
        top(c,"ДИСТАНЦИЯ");char b[40];
        txt(c,8,16,"КОРОТКАЯ: 12 КР");
        snprintf(b,sizeof(b),"ПОЛНАЯ: %u КР",gp_track_info(a->chosen_track)->actual_laps);txt(c,8,33,b);
        selection_frame(c,2,a->full_distance?31:14,124,14);
        bottom(c,"^V ВЫБОР OK ДАЛЕЕ");break;}
    case ScreenRedFlag:draw_red(c,a);break;
    case ScreenEngineer:draw_engineer(c,a);break;
    case ScreenWeather:draw_weather(c,a);break;
    case ScreenWeekend:draw_weekend(c,a);break;
    case ScreenCalendar:draw_calendar(c,a);break;
    case ScreenWeekendReplace:{
        char b[32];top(c,"НОВЫЙ СЕЗОН");snprintf(b,sizeof(b),"ЗАМЕНИТ СЛОТ %u",a->storage.profile+1);txt(c,2,16,b);txt(c,2,32,"23 ЭТАПА / 6 СПРИНТОВ");
        bottom(c,"OK СТАРТ BACK НАЗАД");break;}
    case ScreenQualExit:
        top(c,"ПРЕРВАТЬ КВАЛУ?");txt(c,2,16,"РЕЗУЛЬТАТЫ ПРОПАДУТ");txt(c,2,32,"ВОЗВРАТ К ДИСТАНЦИИ");
        bottom(c,"OK ВЫЙТИ BACK НАЗАД");break;
    case ScreenReplace:{
        char b[32];top(c,"НОВАЯ ГОНКА");snprintf(b,sizeof(b),"ЗАМЕНИТ СЛОТ %u",a->storage.profile+1);txt(c,2,16,b);txt(c,2,32,"ПРЕЖНЕГО ПРОГРЕССА");
        bottom(c,"OK СТАРТ BACK НАЗАД");break;}
    case ScreenAbandon:
        top(c,"ВЫЙТИ БЕЗ ЗАПИСИ?");txt(c,2,16,"НОВЫЙ ПРОГРЕСС");txt(c,2,32,"НЕ БУДЕТ СОХРАНЁН");
        bottom(c,"OK ВЫЙТИ BACK НАЗАД");break;
    case ScreenNotice:
        top(c,a->save_status==GpSaveRecovered?"ВОССТАНОВЛЕНО":"СОХРАНЕНИЕ");
        if(a->save_status==GpSaveNoCard){txt(c,2,16,"НЕТ ДОСТУПА К SD");txt(c,2,32,"ПРОВЕРЬ КАРТУ");}
        else if(a->save_status==GpSaveMissing){txt(c,2,16,"НЕТ СОХРАНЕНИЯ");txt(c,2,32,"НАЧНИ НОВУЮ ГОНКУ");}
        else if(a->save_status==GpSaveCorrupt){txt(c,2,16,"ФАЙЛЫ ПОВРЕЖДЕНЫ");txt(c,2,32,"ЗАГРУЗКА НЕВОЗМОЖНА");}
        else if(a->save_status==GpSaveVersion){txt(c,2,16,"ДРУГАЯ ВЕРСИЯ ФАЙЛА");txt(c,2,32,"НУЖНА ДРУГАЯ ВЕРСИЯ");}
        else if(a->save_status==GpSaveRecovered){txt(c,2,16,"ИЗ РЕЗЕРВНОЙ КОПИИ");txt(c,2,32,"ПРОВЕРЬ КРУГ И ПИТЫ");}
        else{txt(c,2,16,"ОШИБКА ЧТЕНИЯ/ЗАПИСИ");txt(c,2,32,"ПРОВЕРЬ КАРТУ SD");}
        bottom(c,"OK ИЛИ BACK: ЗАКРЫТЬ");break;
    case ScreenTeam:draw_team(c,a);break;
    case ScreenTrack:draw_track(c,a);break;
    case ScreenQualifying:draw_qualifying(c,a);break;
    case ScreenGrid:draw_grid(c,a);break;
    case ScreenQualResults:draw_qual_results(c,a);break;
    case ScreenMap:draw_map(c,a);break;
    case ScreenStandings:draw_table(c,a,false);break;
    case ScreenDriver:draw_driver(c,a);break;
    case ScreenStrategy:draw_strategy(c,a);break;
    case ScreenResults:draw_table(c,a,true);break;
    case ScreenHelp:draw_help(c,a);break;
    case ScreenPause:
        top(c,"МЕНЮ ПАУЗЫ");txt(c,8,15,"ПРОДОЛЖИТЬ");txt(c,8,28,"СОХРАНИТЬ И ВЫЙТИ");txt(c,8,41,"ВЫЙТИ БЕЗ ЗАПИСИ");
        selection_frame(c,2,13+a->pause_item*13,124,13);
        bottom(c,"^V ВЫБОР BACK НАЗАД");break;
    }
    if(a->screen>=ScreenMap&&a->screen<=ScreenStrategy){
        char status[12];
        txt(c,62,0,gp_driver_info(a->race.player)->code);
        snprintf(status,sizeof(status),"%c%c",a->race.pit_queued[0]?'1':'-',a->race.pit_queued[1]?'2':'-');
        txt(c,84,0,status);
        snprintf(status,sizeof(status),a->clock.paused?"СТОП":(a->autosave_blocked?"SD!":"x%lu"),(unsigned long)gp_clock_multiplier(&a->clock));
        txt(c,102,0,status);
    }
    if(shared->mutex)furi_mutex_release(shared->mutex);
}
static void input_cb(InputEvent* e,void* ctx){
    GpApp* a=ctx;
    if(e->type==InputTypeShort||e->type==InputTypeLong||e->type==InputTypeRepeat)
        furi_message_queue_put(a->queue,e,0);
}
static bool race_screen(GpScreen s){
    return s==ScreenMap||s==ScreenStandings||s==ScreenDriver||s==ScreenStrategy;
}
static void start_race(GpApp* a){
    GpWeather weather=a->weekend.active?a->race.weather:a->qualifying.weather;
    if(!weather.rng)gp_weather_start(&weather,furi_get_tick());
    gp_race_start(&a->race,a->chosen_team,a->preferred_driver,a->chosen_track,furi_get_tick());
    gp_race_set_mode(&a->race,a->chosen_mode,a->preferred_driver);
    a->race.weather=weather;
    GpTyre tyre=(GpTyre)gp_weather_tyre(&weather);
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){a->race.drivers[i].tyre=tyre;a->race.drivers[i].temperature=gp_tyre_optimal(tyre);}
    a->race.pit_tyres[0]=a->race.pit_tyres[1]=tyre;
    gp_race_apply_grid(&a->race,a->qualifying.grid);
    a->race.total_laps=a->weekend.active&&a->weekend.stage==GpStageSprint?gp_sprint_laps(a->chosen_track,a->full_distance):(a->full_distance?gp_track_info(a->chosen_track)->actual_laps:GP_TOTAL_LAPS);
    if(a->weekend.active)a->weekend.phase=GpWeekendRacing;
    a->save_requested=true;a->save_exit=false;a->autosave_blocked=false;
    gp_clock_reset(&a->clock);a->screen=ScreenMap;a->standings_top=0;a->last_tick=furi_get_tick();
}
static void weekend_next(GpApp* a){
    gp_weekend_record(&a->weekend,&a->race);
    GpStage previous=a->weekend.stage;GpWeather weather=a->race.weather;
    if(!gp_weekend_next(&a->weekend,a->race.track))return;
    if(previous==GpStageRace&&a->weekend.stage!=GpStageDone){
        a->chosen_track=(a->race.track+1)%GP_TRACK_COUNT;
        memset(&a->qualifying,0,sizeof(a->qualifying));
    }
    if(a->weekend.stage==GpStageQual||a->weekend.stage==GpStageFP1){
        gp_race_start(&a->race,a->chosen_team,a->preferred_driver,a->chosen_track,furi_get_tick());
    gp_race_set_mode(&a->race,a->chosen_mode,a->preferred_driver);
        if(previous!=GpStageRace)a->race.weather=weather;
        memset(&a->qualifying,0,sizeof(a->qualifying));
        gp_clock_reset(&a->clock);
    }
    a->screen=ScreenWeekend;a->save_requested=true;
}
static void weekend_begin(GpApp* a){
    gp_weekend_start(&a->weekend,a->full_distance,furi_hal_random_get());
    memset(&a->qualifying,0,sizeof(a->qualifying));
    gp_race_start(&a->race,a->chosen_team,a->preferred_driver,a->chosen_track,furi_get_tick());
    gp_race_set_mode(&a->race,a->chosen_mode,a->preferred_driver);
    gp_clock_reset(&a->clock);a->screen=ScreenWeekend;
    a->save_requested=true;a->autosave_blocked=false;
}
static void standings_scroll(GpApp* a,InputKey key){
    if(key==InputKeyUp&&a->standings_top>0)--a->standings_top;
    if(key==InputKeyDown&&a->standings_top+3<GP_DRIVER_COUNT)++a->standings_top;
}
static void grid_scroll(GpApp* a,InputKey key){
    if(key==InputKeyUp&&a->grid_top>0)--a->grid_top;
    if(key==InputKeyDown&&a->grid_top+3<GP_DRIVER_COUNT)++a->grid_top;
}
static void show_help(GpApp* a){
    a->help_return=a->screen;a->help_page=0;a->screen=ScreenHelp;
}
static void handle_input(GpApp* a,const InputEvent* e){
    if(a->screen==ScreenRole){
        if(e->type!=InputTypeShort)return;
        if(e->key==InputKeyUp)a->chosen_mode=GpModeTeam;
        else if(e->key==InputKeyDown)a->chosen_mode=GpModeDriver;
        else if(e->key==InputKeyOk)a->screen=ScreenTeam;
        else if(e->key==InputKeyBack)a->screen=ScreenMain;
        return;
    }
    if(a->screen==ScreenSlots){
        if(e->type!=InputTypeShort)return;
        if(e->key==InputKeyUp&&a->slot_choice)a->slot_choice--;
        else if(e->key==InputKeyDown&&a->slot_choice+1u<GP_SAVE_PROFILES)a->slot_choice++;
        else if(e->key==InputKeyOk){a->storage.profile=a->slot_choice;a->screen=ScreenMain;a->menu_item=0;}
        else if(e->key==InputKeyBack)a->screen=ScreenMain;
        return;
    }
    if(a->screen==ScreenChampions){
        if(e->type==InputTypeShort&&e->key==InputKeyOk){a->screen=ScreenMain;a->menu_item=0;}
        else if(e->type==InputTypeShort&&e->key==InputKeyBack)a->screen=ScreenWeekend;
        return;
    }
    if(a->screen==ScreenChampionship){
        if(e->type!=InputTypeShort&&e->type!=InputTypeRepeat)return;
        unsigned count=a->championship_teams?GP_TEAM_COUNT:GP_DRIVER_COUNT;
        if(e->key==InputKeyUp&&a->standings_top)a->standings_top--;
        else if(e->key==InputKeyDown&&a->standings_top+3u<count)a->standings_top++;
        else if(e->type==InputTypeShort&&(e->key==InputKeyLeft||e->key==InputKeyRight)){a->championship_teams=!a->championship_teams;a->standings_top=0;}
        else if(e->type==InputTypeShort&&(e->key==InputKeyBack||e->key==InputKeyOk))a->screen=ScreenWeekend;
        return;
    }
    if(a->screen==ScreenHistory||a->screen==ScreenHistoryResult){
        if(e->type!=InputTypeShort&&e->type!=InputTypeRepeat)return;
        if(a->screen==ScreenHistoryResult){
            standings_scroll(a,e->key);
            if(e->type==InputTypeShort&&(e->key==InputKeyBack||e->key==InputKeyOk))a->screen=ScreenHistory;
        }else if(e->type==InputTypeShort){
            if(e->key==InputKeyLeft)a->history_track=(a->history_track+GP_TRACK_COUNT-1)%GP_TRACK_COUNT;
            else if(e->key==InputKeyRight)a->history_track=(a->history_track+1)%GP_TRACK_COUNT;
            else if(e->key==InputKeyUp||e->key==InputKeyDown)a->history_sprint=!a->history_sprint;
            else if(e->key==InputKeyOk){a->screen=ScreenHistoryResult;a->standings_top=0;}
            else if(e->key==InputKeyBack)a->screen=ScreenWeekend;
        }
        return;
    }
    if(a->screen==ScreenEngineer){
        if(e->type==InputTypeShort&&(e->key==InputKeyLeft||e->key==InputKeyRight))a->engineer_battle=!a->engineer_battle;
        if(e->type==InputTypeLong&&e->key==InputKeyOk)gp_select_driver(&a->race,1-a->race.active_slot);
        if(e->type==InputTypeShort&&(e->key==InputKeyOk||e->key==InputKeyBack)){
            a->race.incident_pending=false;a->screen=a->engineer_return;
            if(!a->autosave_blocked)a->save_requested=true;
        }
        return;
    }
    if(a->screen==ScreenRedFlag){
        if(e->type!=InputTypeShort)return;
        if(e->key==InputKeyLeft||e->key==InputKeyRight)gp_select_driver(&a->race,1-a->race.active_slot);
        else if(e->key==InputKeyBack){a->help_return=ScreenRedFlag;a->pause_item=0;a->screen=ScreenPause;return;}
        else if(e->key==InputKeyOk){
            if(gp_restart_race(&a->race)){a->clock.progress_ticks=0;a->clock.paused=false;a->last_tick=furi_get_tick();a->screen=ScreenMap;}
        }else if(a->race.drivers[a->race.player].status==GpDriverRunning){
            uint8_t slot=a->race.active_slot;
            if(e->key==InputKeyUp)a->race.pit_tyres[slot]=(GpTyre)((a->race.pit_tyres[slot]+GpTyreCount-1)%GpTyreCount);
            if(e->key==InputKeyDown)a->race.pit_tyres[slot]=(GpTyre)((a->race.pit_tyres[slot]+1)%GpTyreCount);
        }
        if(!a->autosave_blocked)a->save_requested=true;
        return;
    }
    if(e->type==InputTypeLong&&e->key==InputKeyDown&&race_screen(a->screen)){
        a->engineer_return=a->screen;a->engineer_battle=true;a->screen=ScreenEngineer;return;
    }
    if(a->screen==ScreenWeather){
        if(e->type==InputTypeShort&&(e->key==InputKeyBack||e->key==InputKeyOk))a->screen=a->weather_return;
        return;
    }
    if(e->type==InputTypeLong&&e->key==InputKeyUp&&(race_screen(a->screen)||a->screen==ScreenQualifying)){
        a->weather_return=a->screen==ScreenQualifying?ScreenQualifying:a->screen;a->screen=ScreenWeather;return;
    }
    if(a->screen==ScreenWeekendReplace){
        if(e->type==InputTypeShort&&e->key==InputKeyOk)weekend_begin(a);
        else if(e->type==InputTypeShort&&e->key==InputKeyBack)a->screen=ScreenDistance;
        return;
    }
    if(a->screen==ScreenCalendar){
        if(e->type==InputTypeShort||e->type==InputTypeRepeat){
            if(e->key==InputKeyUp&&a->standings_top)--a->standings_top;
            else if(e->key==InputKeyDown&&a->standings_top+3<GP_TRACK_COUNT)++a->standings_top;
            else if(e->type==InputTypeShort&&(e->key==InputKeyBack||e->key==InputKeyOk))a->screen=ScreenWeekend;
        }
        return;
    }
    if(a->screen==ScreenWeekend){
        if(e->type!=InputTypeShort)return;
        if(e->key==InputKeyBack){a->help_return=a->screen;a->pause_item=0;a->screen=ScreenPause;}
        else if(e->key==InputKeyLeft){a->screen=ScreenCalendar;a->standings_top=0;}
        else if(e->key==InputKeyRight){a->screen=ScreenChampionship;a->championship_teams=false;a->standings_top=0;}
        else if(e->key==InputKeyUp){a->screen=ScreenHistory;a->history_track=a->race.track;a->history_sprint=false;}
        else if(e->key==InputKeyOk){
            GpWeekend* w=&a->weekend;
            if(w->stage==GpStageDone)a->screen=ScreenChampions;
            else if(w->phase==GpWeekendResult)weekend_next(a);
            else if(w->stage==GpStageRace||w->stage==GpStageSprint)start_race(a);
            else{
                gp_qualifying_start(&a->qualifying,a->race.track,a->race.team,furi_get_tick());
                a->qualifying.mode=a->race.mode;a->qualifying.controlled_slot=a->race.controlled_slot;
                a->qualifying.weather=a->race.weather;
                if(!a->qualifying.weather.rng)gp_weather_start(&a->qualifying.weather,furi_get_tick());
                GpTyre tyre=(GpTyre)gp_weather_tyre(&a->qualifying.weather);
                a->qualifying.tyre[0]=a->qualifying.tyre[1]=tyre>=GpTyreIntermediate?tyre:GpTyreSoft;
                w->phase=GpWeekendQualSetup;a->screen=ScreenQualifying;
            }
        }
        return;
    }
    if(a->screen==ScreenQualExit){
        if(e->type==InputTypeShort&&e->key==InputKeyOk)a->screen=ScreenDistance;
        else if(e->type==InputTypeShort&&e->key==InputKeyBack)a->screen=a->qual_return;
        return;
    }
    if(a->screen==ScreenAbandon){
        if(e->type==InputTypeShort&&e->key==InputKeyOk){a->screen=ScreenMain;a->menu_item=0;}
        else if(e->type==InputTypeShort&&e->key==InputKeyBack)a->screen=ScreenPause;
        return;
    }
    if(a->screen==ScreenNotice){
        if(e->type==InputTypeShort&&(e->key==InputKeyOk||e->key==InputKeyBack))a->screen=a->notice_return;
        return;
    }
    if(a->screen==ScreenReplace){
        if(e->type==InputTypeShort&&e->key==InputKeyOk)start_race(a);
        else if(e->type==InputTypeShort&&e->key==InputKeyBack)a->screen=ScreenGrid;
        return;
    }
    if(a->screen==ScreenDistance){
        if(e->type!=InputTypeShort)return;
        if(e->key==InputKeyUp)a->full_distance=false;
        else if(e->key==InputKeyDown)a->full_distance=true;
        else if(e->key==InputKeyOk){
            if(a->new_weekend)a->screen=ScreenWeekendReplace;
            else{
                memset(&a->weekend,0,sizeof(a->weekend));
                gp_qualifying_start(&a->qualifying,a->chosen_track,a->chosen_team,furi_get_tick());
                a->qualifying.mode=a->chosen_mode;a->qualifying.controlled_slot=a->preferred_driver;
                a->screen=ScreenQualifying;
            }
        }
        else if(e->key==InputKeyBack)a->screen=ScreenTrack;
        return;
    }
    if(a->screen==ScreenMain){
        if(e->type!=InputTypeShort)return;
        if(e->key==InputKeyUp&&a->menu_item>0)--a->menu_item;
        else if(e->key==InputKeyDown&&a->menu_item<5)++a->menu_item;
        else if(e->key==InputKeyBack)a->running=false;
        else if(e->key==InputKeyOk){
            if(a->menu_item==0)a->load_requested=true;
            else if(a->menu_item==1||a->menu_item==2){a->new_weekend=a->menu_item==1;a->screen=ScreenRole;}
            else if(a->menu_item==3)show_help(a);
            else if(a->menu_item==5){a->screen=ScreenSlots;a->slot_choice=a->storage.profile;}
            else a->running=false;
        }
        return;
    }
    /* Repeats only scroll lists: held controls must not cycle a decision. */
    if(e->type==InputTypeRepeat&&a->screen!=ScreenStandings&&a->screen!=ScreenGrid&&a->screen!=ScreenQualResults&&a->screen!=ScreenResults)return;

    if(a->screen==ScreenPause){
        if(e->type!=InputTypeShort)return;
        if(e->key==InputKeyUp&&a->pause_item>0)--a->pause_item;
        else if(e->key==InputKeyDown&&a->pause_item<2)++a->pause_item;
        else if(e->key==InputKeyBack)a->screen=a->help_return;
        else if(e->key==InputKeyOk){
            if(a->pause_item==0)a->screen=a->help_return;
            else if(a->pause_item==1){a->save_requested=true;a->save_exit=true;}
            else a->screen=ScreenAbandon;
        }
        return;
    }
    if(a->screen==ScreenHelp){
        if(e->type!=InputTypeShort)return;
        if(e->key==InputKeyRight)a->help_page=(a->help_page+1)%19;
        else if(e->key==InputKeyLeft)a->help_page=(a->help_page+18)%19;
        else if(e->key==InputKeyOk||e->key==InputKeyBack)a->screen=a->help_return;
        return;
    }
    if(e->type==InputTypeLong){
        if(a->screen==ScreenQualifying&&e->key==InputKeyOk&&a->chosen_mode==GpModeTeam){
            a->preferred_driver=1-a->preferred_driver;
            if(a->weekend.active){gp_select_driver(&a->race,a->preferred_driver);a->save_requested=true;}
        }
        else if(e->key==InputKeyLeft)show_help(a);
        else if(race_screen(a->screen)&&e->key==InputKeyRight)
            a->clock.speed_index=(a->clock.speed_index+1)%3;
        else if(race_screen(a->screen)&&e->key==InputKeyOk)
            gp_select_driver(&a->race,1-a->race.active_slot);
        return;
    }
    if(e->type!=InputTypeShort&&e->type!=InputTypeRepeat)return;
    if(e->key==InputKeyBack&&e->type==InputTypeShort&&a->weekend.active&&
       (a->screen==ScreenQualifying||a->screen==ScreenQualResults||a->screen==ScreenGrid||a->screen==ScreenResults)){
        a->help_return=a->screen;a->pause_item=0;a->screen=ScreenPause;return;
    }
    if(e->key==InputKeyBack&&e->type==InputTypeShort){
        if(a->screen==ScreenTeam)a->screen=ScreenRole;
        else if(a->screen==ScreenTrack)a->screen=ScreenTeam;
        else if(a->screen==ScreenQualifying){a->qual_return=a->screen;a->screen=ScreenQualExit;}
        else if(a->screen==ScreenGrid)a->screen=ScreenQualResults;
        else if(a->screen==ScreenQualResults){
            if(a->qualifying.run<2)a->screen=ScreenQualifying;
            else {a->qual_return=a->screen;a->screen=ScreenQualExit;}
        }
        else if(a->screen==ScreenResults)a->screen=ScreenMain;
        else if(race_screen(a->screen)){a->help_return=a->screen;a->pause_item=0;a->screen=ScreenPause;}
        return;
    }
    if(a->screen==ScreenTeam){
        if(e->key==InputKeyLeft&&e->type==InputTypeShort)
            a->chosen_team=(a->chosen_team+GP_TEAM_COUNT-1)%GP_TEAM_COUNT;
        else if(e->key==InputKeyRight&&e->type==InputTypeShort)
            a->chosen_team=(a->chosen_team+1)%GP_TEAM_COUNT;
        else if((e->key==InputKeyUp||e->key==InputKeyDown)&&e->type==InputTypeShort)
            a->preferred_driver=1-a->preferred_driver;
        else if(e->key==InputKeyOk&&e->type==InputTypeShort)a->screen=ScreenTrack;
    }else if(a->screen==ScreenTrack){
        if(e->key==InputKeyLeft&&e->type==InputTypeShort)
            a->chosen_track=(a->chosen_track+GP_TRACK_COUNT-1)%GP_TRACK_COUNT;
        else if(e->key==InputKeyRight&&e->type==InputTypeShort)
            a->chosen_track=(a->chosen_track+1)%GP_TRACK_COUNT;
        else if(e->key==InputKeyOk&&e->type==InputTypeShort)a->screen=ScreenDistance;
    }else if(a->screen==ScreenQualifying){
        GpQualifying* q=&a->qualifying;uint8_t slot=a->preferred_driver;
        bool active=gp_qualifying_active(q,q->team*2+slot);
        if(e->key==InputKeyUp&&active)q->tyre[slot]=(GpTyre)((q->tyre[slot]+GpTyreCount-1)%GpTyreCount);
        else if(e->key==InputKeyDown&&active)q->tyre[slot]=(GpTyre)((q->tyre[slot]+1)%GpTyreCount);
        else if(e->key==InputKeyLeft&&active)q->pace[slot]=(GpPace)((q->pace[slot]+2)%3);
        else if(e->key==InputKeyRight&&active)q->pace[slot]=(GpPace)((q->pace[slot]+1)%3);
        else if(e->key==InputKeyOk&&gp_qualifying_run(q)){
            uint8_t p=gp_grid_position(q,q->team*2+slot);
            a->grid_top=p>2?p-2:0;
            if(a->grid_top+3>q->active_count)a->grid_top=q->active_count-3;
            a->screen=ScreenQualResults;
        }
    }else if(a->screen==ScreenQualResults){
        GpQualifying* q=&a->qualifying;
        if(e->key==InputKeyUp&&a->grid_top)--a->grid_top;
        else if(e->key==InputKeyDown&&a->grid_top+3<q->active_count)++a->grid_top;
        else if(e->key==InputKeyOk&&e->type==InputTypeShort){
            if(a->weekend.active&&a->weekend.stage<=GpStageFP3&&q->run==2){
                a->weekend.phase=GpWeekendResult;weekend_next(a);
            }else if(q->complete){a->screen=ScreenGrid;a->grid_top=0;}
            else{
                if(q->run==2)gp_qualifying_next(q);
                a->screen=ScreenQualifying;
            }
        }
    }else if(a->screen==ScreenGrid){
        if(e->key==InputKeyUp||e->key==InputKeyDown)grid_scroll(a,e->key);
        else if(e->key==InputKeyOk&&e->type==InputTypeShort){
            if(a->weekend.active){a->weekend.phase=GpWeekendResult;weekend_next(a);}
            else a->screen=ScreenReplace;
        }
    }else if(race_screen(a->screen)){
        if(e->key==InputKeyRight&&e->type==InputTypeShort)
            a->screen=(a->screen==ScreenStrategy)?ScreenMap:(GpScreen)(a->screen+1);
        else if(e->key==InputKeyLeft&&e->type==InputTypeShort)
            a->screen=(a->screen==ScreenMap)?ScreenStrategy:(GpScreen)(a->screen-1);
        else if(a->screen==ScreenStandings)standings_scroll(a,e->key);
        else if(a->screen==ScreenMap&&e->key==InputKeyOk&&e->type==InputTypeShort)
            a->clock.paused=!a->clock.paused;
        else if(a->screen==ScreenMap&&e->type==InputTypeShort&&(e->key==InputKeyUp||e->key==InputKeyDown))
            gp_select_driver(&a->race,1-a->race.active_slot);
        else if(a->screen==ScreenDriver){
            uint8_t s=a->race.active_slot;
            if(e->key==InputKeyOk&&e->type==InputTypeShort)gp_select_driver(&a->race,1-s);
            else if(e->key==InputKeyUp&&a->race.drivers[a->race.player].status==GpDriverRunning&&a->race.pace[s]<GpPaceAttack)++a->race.pace[s];
            else if(e->key==InputKeyDown&&a->race.drivers[a->race.player].status==GpDriverRunning&&a->race.pace[s]>GpPaceConserve)--a->race.pace[s];
        }else if(a->screen==ScreenStrategy){
            if(a->race.drivers[a->race.player].status!=GpDriverRunning)return;
            uint8_t s=a->race.active_slot;
            if(e->key==InputKeyOk&&e->type==InputTypeShort)
                a->race.pit_queued[s]=!a->race.pit_queued[s];
            else if(e->key==InputKeyUp)
                a->race.pit_tyres[s]=(GpTyre)((a->race.pit_tyres[s]+GpTyreCount-1)%GpTyreCount);
            else if(e->key==InputKeyDown)
                a->race.pit_tyres[s]=(GpTyre)((a->race.pit_tyres[s]+1)%GpTyreCount);
        }
    }else if(a->screen==ScreenResults){
        standings_scroll(a,e->key);
        if(e->key==InputKeyOk&&e->type==InputTypeShort){
            if(a->weekend.active){a->weekend.phase=GpWeekendResult;weekend_next(a);}
            else a->screen=ScreenMain;
        }
    }
}
static void weekend_sync(GpApp* a){
    if(!a->weekend.active)return;
    if(a->screen==ScreenQualifying)a->weekend.phase=GpWeekendQualSetup;
    else if(a->screen==ScreenQualResults)a->weekend.phase=GpWeekendQualResult;
    else if(a->screen==ScreenGrid&&a->weekend.stage>GpStageFP3)a->weekend.phase=GpWeekendGrid;
    else if(a->screen==ScreenResults){a->weekend.phase=GpWeekendResult;gp_weekend_record(&a->weekend,&a->race);}
    a->weekend.qualifying=a->qualifying;
    if(a->qualifying.session&&(a->weekend.stage<=GpStageFP3||a->weekend.stage==GpStageQual||a->weekend.stage==GpStageSprintQual))
        a->race.weather=a->qualifying.weather;
}
static void input(GpApp* a,const InputEvent* e){
    GpScreen previous=a->screen;GpQualifying old=a->qualifying;
    handle_input(a,e);weekend_sync(a);
    bool persistent=a->screen==ScreenWeekend||a->screen==ScreenQualifying||a->screen==ScreenQualResults||
        a->screen==ScreenGrid||a->screen==ScreenResults||race_screen(a->screen);
    if(a->weekend.active&&!a->autosave_blocked&&persistent&&
       (previous!=a->screen||memcmp(&old,&a->qualifying,sizeof(old))))a->save_requested=true;
}
static GpScreen weekend_screen(const GpWeekend* w){
    if(w->phase==GpWeekendQualSetup)return ScreenQualifying;
    if(w->phase==GpWeekendQualResult)return ScreenQualResults;
    if(w->phase==GpWeekendGrid)return ScreenGrid;
    if(w->phase==GpWeekendRacing)return ScreenMap;
    if(w->phase==GpWeekendResult){
        if(w->stage==GpStageRace||w->stage==GpStageSprint)return ScreenResults;
        if(w->stage<=GpStageFP3)return ScreenGrid;
    }
    return ScreenWeekend;
}
/* Only the application thread performs storage I/O; GUI stays responsive.
   The app owns mutations; commit holds the model lock, SD operations do not. */
static void process_storage(GpApp* a){
    if(!a->save_requested&&!a->load_requested)return;
    GpSavedGame* game=calloc(1,sizeof(*game));
    bool loading=a->load_requested,exit_after=a->save_exit;
    GpSaveStatus status=GpSaveIoError;
    if(game){
        if(loading)status=gp_save_load(&a->storage,game);
        else{
            game->race=a->race;game->clock=a->clock;game->weekend=a->weekend;
            if(game->weekend.active)game->weekend.qualifying=a->qualifying;
            game->clock.progress_ticks=(uint32_t)((uint64_t)a->clock.progress_ticks*GP_LAP_MS/furi_ms_to_ticks(GP_LAP_MS));
            status=gp_save_store(&a->storage,game);
        }
    }
    if(a->mutex)furi_mutex_acquire(a->mutex,FuriWaitForever);
    a->save_requested=false;a->save_exit=false;a->load_requested=false;a->save_status=status;
    if(loading&&(status==GpSaveOk||status==GpSaveRecovered)){
        a->race=game->race;a->clock=game->clock;a->weekend=game->weekend;
        if(a->weekend.active)a->qualifying=a->weekend.qualifying;
        a->clock.progress_ticks=(uint32_t)((uint64_t)game->clock.progress_ticks*furi_ms_to_ticks(GP_LAP_MS)/GP_LAP_MS);
        a->clock.paused=true;a->chosen_team=a->race.team;a->chosen_track=a->race.track;
        a->preferred_driver=a->race.active_slot;a->chosen_mode=a->race.mode;a->full_distance=a->race.total_laps!=GP_TOTAL_LAPS;
        a->screen=a->weekend.active?weekend_screen(&a->weekend):(gp_race_finished(&a->race)?ScreenResults:ScreenMap);
        if(a->weekend.active)a->full_distance=a->weekend.full_distance;
        if(a->screen==ScreenMap){
            if(a->race.event==GpEventRed)a->screen=ScreenRedFlag;
            else if(a->race.incident_pending){a->engineer_return=ScreenMap;a->engineer_battle=false;a->screen=ScreenEngineer;}
        }
        a->grid_top=0;
        a->standings_top=0;a->autosave_blocked=false;
    }else if(!loading&&status==GpSaveOk){
        a->autosave_blocked=false;
        if(exit_after){a->screen=ScreenMain;a->menu_item=0;}
    }
    if(status!=GpSaveOk){
        a->notice_return=a->screen;
        if(!loading){a->autosave_blocked=true;a->clock.paused=true;}
        a->screen=ScreenNotice;
    }
    a->last_tick=furi_get_tick(); /* SD latency never advances the simulation. */
    if(a->mutex)furi_mutex_release(a->mutex);
    free(game);
}
static void advance_time(GpApp* a,uint32_t now){
    uint32_t dt=now-a->last_tick;a->last_tick=now;
        if(race_screen(a->screen)){
            if(a->race.event==GpEventRed){a->clock.progress_ticks=0;a->screen=ScreenRedFlag;return;}
            uint8_t n=gp_clock_step(&a->clock,dt,furi_ms_to_ticks(GP_GAME_LAP_MS));
            for(uint8_t i=0;i<n;++i){
                bool pending=a->race.incident_pending;
                gp_advance_lap(&a->race);
                if(!a->autosave_blocked)a->save_requested=true;
                if(gp_race_finished(&a->race)){
                    uint8_t pos=gp_position(&a->race,a->race.player);
                    a->standings_top=pos>2?pos-2:0;
                    if(a->standings_top+3>GP_DRIVER_COUNT)a->standings_top=GP_DRIVER_COUNT-3;
                    a->screen=ScreenResults;weekend_sync(a);break;
                }
                if(a->race.event==GpEventRed){a->clock.progress_ticks=0;a->screen=ScreenRedFlag;break;}
                if(a->race.incident_pending&&!pending){a->engineer_return=a->screen;a->engineer_battle=false;a->clock.progress_ticks=0;a->screen=ScreenEngineer;break;}
            }
        }
}
int32_t flipper_gp_manager_app(void* p){
    UNUSED(p);
    GpApp* a=malloc(sizeof(GpApp));if(!a)return -1;
    *a=(GpApp){0};a->screen=ScreenMain;a->help_return=ScreenMain;a->running=true;
    a->queue=furi_message_queue_alloc(12,sizeof(InputEvent));
    if(!a->queue){free(a);return -1;}
    a->mutex=furi_mutex_alloc(FuriMutexTypeNormal);
    if(!a->mutex){furi_message_queue_free(a->queue);free(a);return -1;}
    ViewPort* vp=view_port_alloc();
    if(!vp){furi_mutex_free(a->mutex);furi_message_queue_free(a->queue);free(a);return -1;}
    view_port_draw_callback_set(vp,draw_cb,a);
    view_port_input_callback_set(vp,input_cb,a);
    Gui* gui=furi_record_open(RECORD_GUI);
    a->storage=gp_storage_open();
    a->last_tick=furi_get_tick();
    gui_add_view_port(gui,vp,GuiLayerFullscreen);
    while(a->running){
        InputEvent e;
        bool received=furi_message_queue_get(a->queue,&e,furi_ms_to_ticks(50))==FuriStatusOk;
        furi_mutex_acquire(a->mutex,FuriWaitForever);
        if(received)input(a,&e);
        advance_time(a,furi_get_tick());
        furi_mutex_release(a->mutex);
        process_storage(a);
        if(a->running)view_port_update(vp);
    }
    gui_remove_view_port(gui,vp);view_port_free(vp);furi_record_close(RECORD_GUI);
    gp_storage_close(&a->storage);
    furi_mutex_free(a->mutex);furi_message_queue_free(a->queue);free(a);return 0;
}
