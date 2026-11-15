#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
/* This headless test compiles the REAL ui source against narrow SDK mocks and
   checks all text bounding boxes for overlap and screen overflow. */
#define GP_TEST_MUTEX_TRACK
#include "../flipper_gp_manager.c"
unsigned gp_test_mutex_depth;
static bool require_draw_lock;
#include "mock_save_io.h"
typedef struct {int x,y,w,h;char text[80];} TextRect;
static TextRect labels[64];
static int count;
static int frames;
static int checked_labels;
static void verify(void){
    for(int i=0;i<count;++i){
        const TextRect* a=&labels[i];
        if(a->x<0||a->y<0||a->x+a->w>128||a->y+a->h>64){
            fprintf(stderr,"TEXT OUT OF SCREEN: '%s' x%d y%d w%d h%d\n",a->text,a->x,a->y,a->w,a->h);exit(1);
        }
        for(int j=0;j<i;++j){
            const TextRect* b=&labels[j];
            if(a->x<b->x+b->w&&a->x+a->w>b->x &&
               a->y<b->y+b->h&&a->y+a->h>b->y){
                fprintf(stderr,"OVERLAP: '%s' with '%s'\n",a->text,b->text);exit(2);
            }
        }
    }
    checked_labels+=count;++frames;
}
void mock_text(int x,int y,int w,int h,const char* s){
    if(require_draw_lock)assert(gp_test_mutex_depth==1);
    assert(count<64);labels[count]=(TextRect){.x=x,.y=y,.w=w,.h=h};
    snprintf(labels[count].text,sizeof(labels[count].text),"%s",s);++count;
}
/* Built-in font advances every codepoint by six pixels. */
static int width_of(const char* s){
    int w=0;const unsigned char* p=(const unsigned char*)s;
    while(*p){if((*p&0xc0)!=0x80)w+=6;++p;}return w;
}
void gp_ru_text(Canvas* c,int32_t x,int32_t y,const char* s){
    (void)c;mock_text(x,y,width_of(s),10,s);
}
int32_t gp_ru_width(const char* s){return width_of(s);}
static const uint8_t stubxy[]={5,5,90,5,90,90,5,90,5,5};
static const GpTrackShape stubshape={stubxy,5};
const GpTrackShape* gp_track_shape(uint8_t track){(void)track;return &stubshape;}
static void render(GpApp* a){Canvas c={0};count=0;require_draw_lock=true;a->mutex=furi_mutex_alloc(FuriMutexTypeNormal);draw_cb(&c,a);assert(!gp_test_mutex_depth);require_draw_lock=false;verify();}
static void event(GpApp* a,InputKey key,InputType t){InputEvent e={key,t};input(a,&e);}
int main(void){
    GpApp a={0};a.running=true;
    for(uint8_t t=0;t<GP_TEAM_COUNT;++t){
        a.screen=ScreenTeam;a.chosen_team=t;
        for(uint8_t pilot=0;pilot<2;++pilot){a.preferred_driver=pilot;render(&a);}
    }
    for(uint8_t t=0;t<GP_TRACK_COUNT;++t){
        a.chosen_track=t;a.screen=ScreenTrack;render(&a);
        gp_qualifying_start(&a.qualifying,t,a.chosen_team,123);a.screen=ScreenQualifying;render(&a);
        gp_qualify(&a.qualifying,t,56789u+t);
        a.screen=ScreenGrid;
        for(uint8_t top=0;top<=GP_DRIVER_COUNT-3;++top){a.grid_top=top;render(&a);}
        gp_race_start(&a.race,0,0,t,12345u+t);
        gp_race_apply_grid(&a.race,a.qualifying.grid);
        for(uint8_t step=0;step<GP_TOTAL_LAPS;++step){
            for(uint8_t active=0;active<2;++active){
                gp_select_driver(&a.race,active);
                a.screen=ScreenMap;render(&a);
                a.screen=ScreenDriver;
                for(int pace=0;pace<3;++pace){a.race.pace[active]=(GpPace)pace;render(&a);}
                a.screen=ScreenStrategy;
                for(int tyres=0;tyres<GpTyreCount;++tyres){
                    a.race.pit_tyres[active]=(GpTyre)tyres;
                    for(int pit=0;pit<2;++pit){a.race.pit_queued[active]=pit;render(&a);}
                }
            }
            a.screen=ScreenStandings;
            for(uint8_t top=0;top<=GP_DRIVER_COUNT-3;++top){a.standings_top=top;render(&a);}
            gp_advance_lap(&a.race);
        }
        a.screen=ScreenResults;render(&a);
    }
    for(uint8_t help_page=0;help_page<14;++help_page){a.screen=ScreenHelp;a.help_page=help_page;render(&a);}
    a.screen=ScreenMap;event(&a,InputKeyLeft,InputTypeShort);assert(a.screen==ScreenStrategy);render(&a);
    event(&a,InputKeyRight,InputTypeShort);assert(a.screen==ScreenMap);render(&a);
    event(&a,InputKeyOk,InputTypeShort);assert(a.clock.paused);
    event(&a,InputKeyOk,InputTypeShort);assert(!a.clock.paused);
    /* Navigating Map <-> Strategy <-> Driver must not erase race state. */
    uint8_t before_lap=a.race.lap;
    uint8_t before_team=a.race.team;
    a.screen=ScreenMap;
    event(&a,InputKeyLeft,InputTypeShort);
    assert(a.screen==ScreenStrategy);
    event(&a,InputKeyLeft,InputTypeShort);
    assert(a.screen==ScreenDriver);
    event(&a,InputKeyRight,InputTypeShort);
    assert(a.screen==ScreenStrategy);
    assert(a.race.lap==before_lap && a.race.team==before_team);
    a.screen=ScreenMap;event(&a,InputKeyBack,InputTypeShort);assert(a.screen==ScreenPause);render(&a);
    event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenMap);
    event(&a,InputKeyBack,InputTypeShort);event(&a,InputKeyBack,InputTypeLong);assert(a.screen==ScreenPause);
    a.screen=ScreenTeam;a.chosen_team=0;event(&a,InputKeyLeft,InputTypeShort);assert(a.chosen_team==10);
    event(&a,InputKeyRight,InputTypeShort);assert(a.chosen_team==0);
    event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenTrack);
    event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenDistance);render(&a);
    event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenQualifying);
    for(int session=1;session<=3;session++){
        assert(a.qualifying.session==session);
        for(int run=0;run<2;run++){
            for(int slot=0;slot<2;slot++){
                uint8_t before=a.preferred_driver;
                event(&a,InputKeyOk,InputTypeLong);assert(a.preferred_driver==1-before);
                for(int tyre=0;tyre<3;tyre++)for(int pace=0;pace<3;pace++){
                    event(&a,InputKeyDown,InputTypeShort);event(&a,InputKeyRight,InputTypeShort);render(&a);
                }
            }
            event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenQualResults);render(&a);
            event(&a,InputKeyOk,InputTypeRepeat);assert(a.screen==ScreenQualResults);
            event(&a,InputKeyOk,InputTypeShort);
        }
    }
    assert(a.screen==ScreenGrid);
    event(&a,InputKeyBack,InputTypeShort);assert(a.screen==ScreenQualResults);
    event(&a,InputKeyBack,InputTypeShort);assert(a.screen==ScreenQualExit);render(&a);
    event(&a,InputKeyBack,InputTypeShort);assert(a.screen==ScreenQualResults);
    event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenGrid);
    event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenReplace);render(&a);
    event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenMap);
    /* Every modal returns to its caller without changing race decisions. */
    for(int screen=ScreenMap;screen<=ScreenStrategy;screen++){
        a.screen=(GpScreen)screen;a.clock.paused=true;
        event(&a,InputKeyLeft,InputTypeLong);assert(a.screen==ScreenHelp);
        event(&a,InputKeyBack,InputTypeShort);assert(a.screen==(GpScreen)screen&&a.clock.paused);
        event(&a,InputKeyBack,InputTypeShort);assert(a.screen==ScreenPause);
        event(&a,InputKeyBack,InputTypeRepeat);assert(a.screen==ScreenPause);
        event(&a,InputKeyBack,InputTypeShort);assert(a.screen==(GpScreen)screen);
        uint8_t slot=a.race.active_slot;
        event(&a,InputKeyOk,InputTypeLong);assert(a.race.active_slot==1-slot);
        event(&a,InputKeyOk,InputTypeRepeat);assert(a.race.active_slot==1-slot);
        uint8_t speed=a.clock.speed_index;
        event(&a,InputKeyRight,InputTypeLong);assert(a.clock.speed_index==(speed+1)%3);
        event(&a,InputKeyRight,InputTypeRepeat);assert(a.clock.speed_index==(speed+1)%3);
        render(&a);
    }
    a.screen=ScreenStrategy;uint8_t slot=a.race.active_slot;
    GpTyre tyre=a.race.pit_tyres[slot];bool pit=a.race.pit_queued[slot];
    event(&a,InputKeyDown,InputTypeRepeat);event(&a,InputKeyOk,InputTypeRepeat);
    assert(a.race.pit_tyres[slot]==tyre&&a.race.pit_queued[slot]==pit);
    event(&a,InputKeyOk,InputTypeShort);assert(a.race.pit_queued[slot]!=pit);
    event(&a,InputKeyOk,InputTypeShort);assert(a.race.pit_queued[slot]==pit);
    a.screen=ScreenMain;
    for(int i=0;i<5;i++){a.menu_item=i;render(&a);}
    a.menu_item=3;event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenHelp);
    event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenMain);
    a.menu_item=2;event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenTeam);
    event(&a,InputKeyBack,InputTypeShort);assert(a.screen==ScreenMain);
    a.menu_item=4;event(&a,InputKeyOk,InputTypeShort);assert(!a.running);
    a.race.lap=12;
    for(int i=0;i<GP_DRIVER_COUNT;i++){a.race.ranking[i]=i;a.race.drivers[i].time_ms=i*1000000u;}
    for(int screen=0;screen<2;screen++)for(int top=0;top<=19;top++){
        a.screen=screen?ScreenResults:ScreenStandings;a.standings_top=top;render(&a);
    }
    /* Storage actions through the actual UI controller. */
    a=(GpApp){0};a.storage=gp_storage_open();a.screen=ScreenMain;
    event(&a,InputKeyOk,InputTypeShort);process_storage(&a);
    assert(a.screen==ScreenNotice&&a.save_status==GpSaveMissing);render(&a);
    event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenMain);
    a.chosen_track=5;a.full_distance=true;gp_qualify(&a.qualifying,5,123);start_race(&a);process_storage(&a);
    assert(a.race.total_laps==78&&a.save_status==GpSaveOk);
    gp_advance_lap(&a.race);a.clock.progress_ticks=2345;a.race.pit_queued[0]=a.race.pit_queued[1]=true;
    event(&a,InputKeyBack,InputTypeShort);event(&a,InputKeyDown,InputTypeShort);render(&a);
    event(&a,InputKeyOk,InputTypeShort);assert(a.save_requested&&a.save_exit);process_storage(&a);assert(a.screen==ScreenMain);
    memset(&a.race,0,sizeof(a.race));event(&a,InputKeyOk,InputTypeShort);process_storage(&a);
    assert(a.screen==ScreenMap&&a.clock.paused&&a.clock.progress_ticks==2345&&a.race.lap==1&&a.race.total_laps==78);
    assert(a.race.pit_queued[0]&&a.race.pit_queued[1]);render(&a);
    gp_test_fs.no_card=true;event(&a,InputKeyBack,InputTypeShort);event(&a,InputKeyDown,InputTypeShort);event(&a,InputKeyOk,InputTypeShort);process_storage(&a);
    assert(a.screen==ScreenNotice&&a.save_status==GpSaveNoCard&&a.autosave_blocked);render(&a);
    event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenPause);
    event(&a,InputKeyDown,InputTypeShort);render(&a);event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenAbandon);render(&a);
    event(&a,InputKeyBack,InputTypeShort);assert(a.screen==ScreenPause);
    gp_test_fs.no_card=false;a.pause_item=1;event(&a,InputKeyOk,InputTypeShort);process_storage(&a);
    assert(a.screen==ScreenMain&&!a.autosave_blocked);
    a.chosen_track=0;a.full_distance=false;start_race(&a);process_storage(&a);
    for(int lap=1;lap<=GP_TOTAL_LAPS;lap++){
        advance_time(&a,a.last_tick+GP_LAP_MS);assert(a.save_requested&&a.race.lap==lap);
        process_storage(&a);assert(a.save_status==GpSaveOk);
        GpSavedGame saved={0};assert(gp_save_load(&a.storage,&saved)==GpSaveOk);assert(saved.race.lap==lap);
    }
    assert(a.screen==ScreenResults);a.screen=ScreenMain;a.menu_item=0;
    event(&a,InputKeyOk,InputTypeShort);process_storage(&a);assert(a.screen==ScreenResults);
    uint8_t final_lap=a.race.lap;advance_time(&a,a.last_tick+GP_LAP_MS);assert(a.race.lap==final_lap);
    for(int status=GpSaveRecovered;status<=GpSaveIoError;status++){a.screen=ScreenNotice;a.save_status=status;render(&a);}
    for(int t=0;t<GP_TRACK_COUNT;t++){
        a.chosen_track=t;a.screen=ScreenDistance;
        for(int full=0;full<2;full++){a.full_distance=full;render(&a);}
        gp_race_start(&a.race,3,1,t,123);a.race.total_laps=gp_track_info(t)->actual_laps;
        a.race.lap=a.race.total_laps-1;a.screen=ScreenMap;render(&a);
        a.race.lap=a.race.total_laps;a.screen=ScreenResults;render(&a);
    }
    a=(GpApp){0};gp_race_start(&a.race,0,0,0,123);gp_weekend_start(&a.weekend,false,123);
    a.screen=ScreenWeekendReplace;render(&a);
    for(int top=0;top<=20;top++){a.screen=ScreenCalendar;a.standings_top=top;render(&a);}
    for(int stage=GpStageFP1;stage<=GpStageDone;stage++){
        a.weekend.stage=stage;a.screen=ScreenWeekend;render(&a);
        a.weekend.phase=GpWeekendResult;render(&a);a.weekend.phase=GpWeekendReady;
        if(stage<=GpStageFP3){
            gp_qualifying_start(&a.qualifying,0,0,123);
            for(int run=0;run<2;run++){
                a.screen=ScreenQualifying;render(&a);gp_qualifying_run(&a.qualifying);
                a.screen=ScreenQualResults;a.grid_top=0;render(&a);
            }
        }
    }
    a=(GpApp){0};gp_race_start(&a.race,0,0,0,123);a.screen=ScreenMap;
    event(&a,InputKeyUp,InputTypeLong);assert(a.screen==ScreenWeather);render(&a);
    uint8_t lap_before=a.race.lap;advance_time(&a,a.last_tick+GP_LAP_MS*2);assert(a.race.lap==lap_before);
    event(&a,InputKeyBack,InputTypeShort);assert(a.screen==ScreenMap);
    gp_qualifying_start(&a.qualifying,0,0,123);a.screen=ScreenQualifying;
    event(&a,InputKeyUp,InputTypeLong);assert(a.screen==ScreenWeather);render(&a);
    event(&a,InputKeyOk,InputTypeShort);assert(a.screen==ScreenQualifying);
    for(int tyre=0;tyre<GpTyreCount;tyre++){
        a.qualifying.tyre[0]=tyre;render(&a);event(&a,InputKeyDown,InputTypeShort);
        assert(a.qualifying.tyre[0]==(GpTyre)((tyre+1)%GpTyreCount));
    }
    for(unsigned rain=0;rain<3;rain++)for(unsigned wet=0;wet<=100;wet+=25){
        a.screen=ScreenWeather;a.weather_return=ScreenMap;a.race.weather.rain=rain;a.race.weather.forecast=rain;a.race.weather.wetness=wet;render(&a);
    }
    gp_race_start(&a.race,0,0,0,123);a.screen=ScreenDriver;
    for(unsigned slot=0;slot<2;slot++)for(unsigned tyre=0;tyre<GpTyreCount;tyre++)for(unsigned temp=20;temp<=140;temp+=10){
        gp_select_driver(&a.race,slot);a.race.drivers[a.race.player].tyre=tyre;a.race.drivers[a.race.player].temperature=temp;render(&a);
    }
    a.screen=ScreenStrategy;render(&a);
    a.race.drivers[0].time_ms=a.race.drivers[1].time_ms+gp_pit_loss_ms(&a.race)-500;render(&a);
    gp_qualifying_start(&a.qualifying,0,0,123);gp_qualifying_run(&a.qualifying);a.screen=ScreenQualResults;a.grid_top=0;
    for(unsigned i=0;i<22;i++){a.qualifying.lap_ms[i]=1000000;a.qualifying.temperature[i]=140;}
    render(&a); // largest accepted time and temperature still fit together
    printf("PASS: %d UI frames, %d text labels, zero overlaps/offscreen text; left navigation, qualification flow\n",frames,checked_labels);
}
