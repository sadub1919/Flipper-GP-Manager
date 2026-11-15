#include <assert.h>
#include "../flipper_gp_manager.c"
#include "mock_save_io.h"
static void press(GpApp* a,InputKey key){InputEvent e={key,InputTypeShort};input(a,&e);}
int main(void){
    GpApp a={0};a.storage=gp_storage_open();gp_qualify(&a.qualifying,0,123);start_race(&a);a.clock.speed_index=2;
    GpRace initial=a.race;assert(a.animation==GpAnimationStart);
    for(unsigned phase=0;phase<=6;phase++){
        if(phase)advance_time(&a,a.last_tick+650);
        assert(!memcmp(&initial,&a.race,sizeof(initial))&&a.clock.progress_ticks==0);
        Canvas c={0};draw_cb(&c,&a);
        for(unsigned light=0;light<5;light++)assert(c.pixels[27][20+light*22]==(phase<6&&light<phase));
    }
    advance_time(&a,a.last_tick+400);assert(a.animation==GpAnimationNone&&a.race.lap==0);
    a.clock.speed_index=0;a.race.incident_rng=0;advance_time(&a,a.last_tick+GP_LAP_MS);assert(a.race.lap==1);
    start_race(&a);advance_time(&a,a.last_tick+650);press(&a,InputKeyBack);assert(a.screen==ScreenPause);
    advance_time(&a,a.last_tick+10000);assert(a.animation_ticks==650&&a.race.lap==0);
    press(&a,InputKeyBack);press(&a,InputKeyOk);assert(a.screen==ScreenMap&&a.animation==GpAnimationNone&&a.race.lap==0);
    process_storage(&a);assert(a.save_status==GpSaveOk);a.load_requested=true;process_storage(&a);
    assert(a.animation==GpAnimationNone&&a.clock.paused);a.clock.paused=false;a.race.incident_rng=0;
    gp_advance_lap(&a.race);gp_set_event(&a.race,GpEventSafetyCar,2);
    advance_time(&a,a.last_tick+GP_LAP_MS);assert(a.race.event_remaining==1&&strstr(a.race_notice,"ПОСЛЕДНИЙ"));
    assert(a.screen==ScreenMap);press(&a,InputKeyRight);assert(a.screen==ScreenStandings);press(&a,InputKeyLeft);
    advance_time(&a,a.last_tick+GP_LAP_MS);assert(a.race.event==GpEventNone&&strstr(a.race_notice,"ЗЕЛЁНЫЙ"));
    gp_set_event(&a.race,GpEventYellow,1);race_messages(&a,GpEventNone,0,0,100,a.race.incident_serial);assert(strstr(a.race_notice,"ЖЁЛТЫЙ"));
    gp_set_event(&a.race,GpEventVsc,2);race_messages(&a,GpEventNone,0,0,100,a.race.incident_serial);assert(strstr(a.race_notice,"VSC"));
    notify_race(&a,"ДОЖДЬ",false);assert(strstr(a.race_notice,"VSC")); // cooldown suppresses low-priority spam
    gp_set_event(&a.race,GpEventNone,0);a.notice_cooldown=0;a.race.weather.forecast=1;
    race_messages(&a,GpEventNone,0,0,100,a.race.incident_serial);assert(strstr(a.race_notice,"ДОЖДЬ"));
    a.notice_cooldown=0;a.race.drivers[a.race.player].wear=24;
    race_messages(&a,GpEventNone,0,1,25,a.race.incident_serial);assert(strstr(a.race_notice,"25%"));
    a.notice_cooldown=0;a.race.battle[a.race.active_slot]=(GpBattleReport){GpBattlePassed,4,a.race.lap};
    race_messages(&a,GpEventNone,0,1,24,a.race.incident_serial);assert(strstr(a.race_notice,"ОБГОН"));
    gp_set_event(&a.race,GpEventRed,0);advance_time(&a,a.last_tick+GP_LAP_MS);assert(a.screen==ScreenRedFlag);
    unsigned lap=a.race.lap;advance_time(&a,a.last_tick+10000);assert(a.race.lap==lap);
    a.save_requested=true;process_storage(&a);assert(a.save_status==GpSaveOk);a.load_requested=true;process_storage(&a);assert(a.screen==ScreenRedFlag);
    press(&a,InputKeyOk);assert(a.screen==ScreenMap&&a.race.event==GpEventSafetyCar&&strstr(a.race_notice,"РЕСТАРТ"));
    a.race.lap=a.race.total_laps-1;advance_time(&a,a.last_tick+GP_LAP_MS);
    assert(a.screen==ScreenResults&&a.animation==GpAnimationFinish&&gp_race_finished(&a.race));
    GpRace ended=a.race;Canvas first={0},second={0};draw_cb(&first,&a);advance_time(&a,a.last_tick+180);draw_cb(&second,&a);
    assert(memcmp(first.pixels,second.pixels,sizeof(first.pixels))&&!memcmp(&ended,&a.race,sizeof(ended)));
    press(&a,InputKeyOk);assert(a.screen==ScreenResults&&a.animation==GpAnimationNone);press(&a,InputKeyOk);assert(a.screen==ScreenMain);
    /* Career navigation and confirmation retain the selected driver and slot. */
    a=(GpApp){0};a.storage=gp_storage_open();a.storage.profile=2;a.chosen_mode=GpModeDriver;a.preferred_driver=1;weekend_begin(&a);
    for(unsigned t=0;t<GP_TRACK_COUNT;t++)for(unsigned k=0;k<2;k++){
        GpRoundResult* r=&a.weekend.results[t][k];r->recorded=k==0||gp_weekend_sprint(&a.weekend,t);
        if(r->recorded)for(unsigned i=0;i<GP_DRIVER_COUNT;i++)r->finish[i]=i;
    }
    gp_championship_rebuild(&a.weekend);a.weekend.stage=GpStageDone;a.weekend.rounds_done=23;
    press(&a,InputKeyDown);assert(a.screen==ScreenCareer);press(&a,InputKeyRight);assert(a.career_page==1);press(&a,InputKeyBack);
    press(&a,InputKeyOk);assert(a.screen==ScreenChampions);press(&a,InputKeyRight);assert(a.screen==ScreenNextSeason);
    press(&a,InputKeyBack);assert(a.weekend.career.season==1&&a.screen==ScreenChampions);
    press(&a,InputKeyRight);press(&a,InputKeyOk);assert(a.weekend.career.season==2&&a.screen==ScreenWeekend&&a.race.active_slot==1&&a.storage.profile==2);
    assert(a.weekend.career.history_count==1&&a.weekend.career.totals[0][4]==623);
    process_storage(&a);assert(a.save_status==GpSaveOk);a.load_requested=true;process_storage(&a);assert(a.weekend.career.season==2&&a.chosen_mode==GpModeDriver);
    puts("PASS presentation: five lights/off/skip/pause, real-time animation, nonblocking notices and priority, red save/restart, animated finish, career navigation/confirmation/reload");
}
