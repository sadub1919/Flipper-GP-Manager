#include <assert.h>
#include "../flipper_gp_manager.c"
static const char* output;
static unsigned frames;
static void save(GpApp* a,const char* name,int variant){
    Canvas c={0};draw_cb(&c,a);
    for(int x=0;x<128;x++){assert(c.pixels[11][x]);assert(c.pixels[52][x]);}
    char path[1024];snprintf(path,sizeof(path),"%s/%s_%03d.pbm",output,name,variant);
    FILE* f=fopen(path,"wb");assert(f);fprintf(f,"P4\n128 64\n");
    for(int y=0;y<64;y++)for(int x=0;x<128;x+=8){uint8_t byte=0;for(int k=0;k<8;k++)byte|=c.pixels[y][x+k]<<(7-k);fputc(byte,f);}
    assert(!ferror(f));fclose(f);frames++;
}
int main(int argc,char** argv){
    assert(argc==2);output=argv[1];GpApp a={0};
    a.screen=ScreenMain;for(int i=0;i<5;i++){a.menu_item=i;save(&a,"main",i);}
    a.screen=ScreenTeam;for(int t=0;t<GP_TEAM_COUNT;t++)for(int d=0;d<2;d++){a.chosen_team=t;a.preferred_driver=d;save(&a,"team",t*2+d);}
    for(int t=0;t<GP_TRACK_COUNT;t++){
        a.chosen_track=t;a.screen=ScreenTrack;save(&a,"track",t);
        gp_qualifying_start(&a.qualifying,t,a.chosen_team,2345);a.screen=ScreenQualifying;save(&a,"qualifying",t);
        gp_qualify(&a.qualifying,t,2345);a.screen=ScreenGrid;
        for(int top=0;top<=19;top++){a.grid_top=top;save(&a,"grid",t*20+top);}
        gp_race_start(&a.race,3,0,t,2345);gp_race_apply_grid(&a.race,a.qualifying.grid);
        a.screen=ScreenMap;
        for(int phase=0;phase<8;phase++){a.clock.progress_ticks=phase*750;save(&a,"map",t*8+phase);}
    }
    for(int team=0;team<GP_TEAM_COUNT;team++){
        a.chosen_team=team;gp_qualifying_start(&a.qualifying,5,team,1234);
        for(int session=1;session<=3;session++){
            for(int run=0;run<2;run++){
                a.screen=ScreenQualifying;
                for(int slot=0;slot<2;slot++)for(int tyre=0;tyre<GpTyreCount;tyre++)for(int pace=0;pace<3;pace++){
                    a.preferred_driver=slot;a.qualifying.tyre[slot]=tyre;a.qualifying.pace[slot]=pace;
                    save(&a,"qual_setup",team*180+(session-1)*60+run*30+slot*15+tyre*3+pace);
                }
                gp_qualifying_run(&a.qualifying);a.screen=ScreenQualResults;
                for(int top=0;top<=a.qualifying.active_count-3;top++){
                    a.grid_top=top;save(&a,"qual_result",team*120+(session-1)*40+run*20+top);
                }
            }
            gp_qualifying_next(&a.qualifying);
        }
    }
    gp_race_start(&a.race,3,0,0,2345);
    for(int screen=ScreenMap;screen<=ScreenStrategy;screen++)for(int paused=0;paused<2;paused++)for(int slot=0;slot<2;slot++)for(int pits=0;pits<4;pits++){
        a.screen=screen;a.clock.paused=paused;gp_select_driver(&a.race,slot);
        a.race.pit_queued[0]=pits&1;a.race.pit_queued[1]=pits&2;
        save(&a,"race_status",(screen-ScreenMap)*16+paused*8+slot*4+pits);
    }
    a.screen=ScreenMap;
    for(int e=0;e<4;e++)for(int speed=0;speed<3;speed++)for(int paused=0;paused<2;paused++){
        a.race.event=e;a.clock.speed_index=speed;a.clock.paused=paused;
        save(&a,"event_speed",e*6+speed*2+paused);
    }
    a.screen=ScreenDriver;
    for(int tyre=0;tyre<GpTyreCount;tyre++)for(int pace=0;pace<3;pace++)for(int wear=0;wear<=100;wear+=100){
        a.race.drivers[a.race.player].tyre=tyre;a.race.drivers[a.race.player].wear=wear;a.race.pace[a.race.active_slot]=pace;
        save(&a,"driver",tyre*6+pace*2+wear/100);
    }
    a.screen=ScreenStrategy;for(int tyre=0;tyre<GpTyreCount;tyre++){a.race.pit_tyres[a.race.active_slot]=tyre;save(&a,"strategy",tyre);}
    a.screen=ScreenStandings;a.standings_top=0;save(&a,"start",0);
    a.race.lap=12;for(int i=0;i<GP_DRIVER_COUNT;i++){a.race.ranking[i]=i;a.race.drivers[i].time_ms=i*1000000u;}
    for(int screen=0;screen<2;screen++){a.screen=screen?ScreenResults:ScreenStandings;for(int top=0;top<=19;top++){a.standings_top=top;save(&a,screen?"results":"large_gap",top);}}
    a.screen=ScreenHelp;for(int i=0;i<16;i++){a.help_page=i;save(&a,"help",i);}
    a.screen=ScreenPause;for(int i=0;i<3;i++){a.pause_item=i;save(&a,"pause",i);}
    for(int t=0;t<GP_TRACK_COUNT;t++){
        a.chosen_track=t;a.screen=ScreenDistance;
        for(int full=0;full<2;full++){a.full_distance=full;save(&a,"distance",t*2+full);}
        gp_race_start(&a.race,3,1,t,123);a.race.total_laps=gp_track_info(t)->actual_laps;
        a.race.lap=a.race.total_laps-1;a.screen=ScreenMap;save(&a,"full_last",t);
        a.race.lap=a.race.total_laps;a.screen=ScreenResults;save(&a,"full_result",t);
    }
    for(int status=GpSaveRecovered;status<=GpSaveIoError;status++){a.screen=ScreenNotice;a.save_status=status;save(&a,"notice",status);}
    a.screen=ScreenQualExit;save(&a,"qual_exit",0);
    a.screen=ScreenQualifying;gp_qualifying_start(&a.qualifying,0,0,1);a.qualifying.soft_used[0]=4;a.preferred_driver=0;save(&a,"qual_used",0);
    a.screen=ScreenReplace;save(&a,"replace",0);a.screen=ScreenAbandon;save(&a,"abandon",0);
    a.screen=ScreenMap;a.autosave_blocked=true;a.clock.paused=false;save(&a,"sd_warning",0);
    a=(GpApp){0};gp_race_start(&a.race,0,0,5,123);gp_weekend_start(&a.weekend,false,123);
    a.screen=ScreenWeekendReplace;save(&a,"weekend_replace",0);
    for(int top=0;top<=20;top++){a.screen=ScreenCalendar;a.standings_top=top;save(&a,"calendar",top);}
    for(int stage=GpStageFP1;stage<=GpStageDone;stage++){
        a.weekend.stage=stage;a.weekend.rounds_done=stage==GpStageDone?23:0;
        if(stage==GpStageSprintQual||stage==GpStageSprint){
            uint32_t old=a.weekend.sprint_mask;
            if(!(old&(1u<<a.race.track)))a.weekend.sprint_mask=(old&(old-1))|(1u<<a.race.track);
        }
        a.screen=ScreenWeekend;save(&a,"weekend",stage*2);
        a.weekend.phase=GpWeekendResult;save(&a,"weekend",stage*2+1);a.weekend.phase=GpWeekendReady;
        if(stage<=GpStageFP3){
            gp_qualifying_start(&a.qualifying,5,0,123);
            for(int run=0;run<2;run++){
                a.screen=ScreenQualifying;save(&a,"practice_setup",stage*2+run);gp_qualifying_run(&a.qualifying);
                a.screen=ScreenQualResults;a.grid_top=0;save(&a,"practice_result",stage*2+run);
            }
        }
    }
    a.weekend.stage=GpStageSprintQual;gp_qualifying_start(&a.qualifying,5,0,123);
    a.screen=ScreenQualifying;save(&a,"sprint_setup",0);gp_qualifying_run(&a.qualifying);
    a.screen=ScreenQualResults;a.grid_top=0;save(&a,"sprint_qual_result",0);
    gp_qualify(&a.qualifying,5,123);a.screen=ScreenGrid;save(&a,"sprint_grid",0);
    a.weekend.stage=GpStageSprint;a.race.total_laps=5;a.race.lap=5;a.screen=ScreenResults;a.standings_top=0;save(&a,"sprint_finish",0);
    a=(GpApp){0};gp_race_start(&a.race,3,0,5,123);
    for(unsigned rain=0;rain<3;rain++)for(unsigned wet=0;wet<=100;wet+=25){
        a.race.weather.rain=rain;a.race.weather.forecast=(rain+1)%3;a.race.weather.wetness=wet;
        a.screen=ScreenWeather;a.weather_return=ScreenMap;save(&a,"weather",rain*5+wet/25);
        a.screen=ScreenStrategy;for(int tyre=0;tyre<GpTyreCount;tyre++){a.race.pit_tyres[0]=tyre;save(&a,"wet_strategy",rain*25+wet/5+tyre);}
    }
    for(unsigned selected=0;selected<22;selected++){
        gp_race_start(&a.race,selected/2,selected%2,5,123);
        for(unsigned i=0;i<22;i++){a.race.ranking[i]=i;a.race.drivers[i].time_ms=0;}
        a.screen=ScreenMap;save(&a,"packed_markers",selected);
    }
    a=(GpApp){0};gp_race_start(&a.race,3,0,5,123);a.screen=ScreenDriver;
    for(unsigned slot=0;slot<2;slot++)for(unsigned tyre=0;tyre<GpTyreCount;tyre++)for(unsigned state=0;state<3;state++){
        gp_select_driver(&a.race,slot);GpDriver* d=&a.race.drivers[a.race.player];d->tyre=tyre;
        d->temperature=state==0?gp_tyre_blanket(tyre):(state==1?gp_tyre_optimal(tyre):140);
        save(&a,"temperature",slot*15+tyre*3+state);
    }
    for(unsigned event=0;event<4;event++){
        gp_race_start(&a.race,0,1,5,123);a.race.event=event;a.screen=ScreenStrategy;
        for(unsigned i=0;i<22;i++){a.race.drivers[i].time_ms=i*10000;a.race.ranking[i]=i;}
        save(&a,"pit_estimate",event*2);
        a.race.drivers[0].time_ms=a.race.drivers[1].time_ms+gp_pit_loss_ms(&a.race)-500;
        save(&a,"pit_estimate",event*2+1);
    }
    a=(GpApp){0};gp_race_start(&a.race,0,0,0,123);gp_advance_lap(&a.race);
    gp_retire_driver(&a.race,2,GpDriverAccident);gp_set_event(&a.race,GpEventRed,0);
    a.screen=ScreenRedFlag;
    for(unsigned slot=0;slot<2;slot++)for(unsigned tyre=0;tyre<GpTyreCount;tyre++){
        gp_select_driver(&a.race,slot);a.race.pit_tyres[slot]=tyre;save(&a,"red",slot*5+tyre);
    }
    for(unsigned flag=GpEventVsc;flag<=GpEventSafetyCar;flag++)for(unsigned duration=1;duration<=4;duration++){
        gp_set_event(&a.race,flag,duration);a.screen=ScreenEngineer;save(&a,"engineer",flag*4+duration-1);
        a.screen=ScreenMap;save(&a,"neutral",flag*4+duration-1);
    }
    gp_set_event(&a.race,GpEventNone,0);a.screen=ScreenEngineer;save(&a,"engineer",0);
    gp_select_driver(&a.race,0);gp_retire_driver(&a.race,0,GpDriverFailure);
    for(unsigned screen=ScreenMap;screen<=ScreenStrategy;screen++){
        a.screen=screen;a.standings_top=19;save(&a,"dnf",screen-ScreenMap);
    }
    a.screen=ScreenRedFlag;save(&a,"red_dnf",0);
    printf("PASS: %u bitmap snapshots from real UI, fonts, badges and track data\n",frames);
}
