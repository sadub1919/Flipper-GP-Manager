#include <assert.h>
#include "../flipper_gp_manager.c"
#include "mock_save_io.h"
static unsigned checkpoints;
static void press(GpApp* a,InputKey k){InputEvent e={k,InputTypeShort};input(a,&e);}
static void checkpoint(GpApp* a){
    assert(a->save_requested);process_storage(a);
    if(a->save_status!=GpSaveOk){fprintf(stderr,"save failed: stage %u phase %u screen %u q%u run%u\n",a->weekend.stage,a->weekend.phase,a->screen,a->qualifying.session,a->qualifying.run);abort();}
    GpSavedGame expected={0},actual={0};assert(gp_save_load(&a->storage,&expected)==GpSaveOk);
    GpScreen screen=a->screen;uint32_t mask=a->weekend.sprint_mask;
    GpApp loaded={0};loaded.storage=a->storage;loaded.screen=ScreenMain;loaded.load_requested=true;process_storage(&loaded);
    assert(loaded.save_status==GpSaveOk&&loaded.screen==screen);
    assert(loaded.weekend.sprint_mask==mask);
    actual.race=loaded.race;actual.clock=loaded.clock;actual.weekend=loaded.weekend;actual.sequence=expected.sequence;
    expected.clock.paused=true;
    uint8_t x[GP_SAVE_BYTES],y[GP_SAVE_BYTES];assert(gp_save_encode(&expected,x)&&gp_save_encode(&actual,y));assert(!memcmp(x,y,sizeof(x)));
    *a=loaded;checkpoints++;
}
int main(void){
    uint32_t first=gp_sprint_calendar(1);bool differs=false;unsigned appearances[GP_TRACK_COUNT]={0};
    for(unsigned seed=0;seed<10000;seed++){
        uint32_t mask=gp_sprint_calendar(seed);assert(mask==gp_sprint_calendar(seed));assert(!(mask>>GP_TRACK_COUNT));
        unsigned n=0;for(unsigned i=0;i<GP_TRACK_COUNT;i++)if(mask&(1u<<i)){n++;appearances[i]++;}
        assert(n==6);if(mask!=first)differs=true;
    }
    assert(differs);for(unsigned i=0;i<GP_TRACK_COUNT;i++)assert(appearances[i]>0);
    for(int full=0;full<2;full++)for(int mode=GpModeTeam;mode<=GpModeDriver;mode++){
        memset(&gp_test_fs,0,sizeof(gp_test_fs));GpApp a={0};a.storage=gp_storage_open();a.screen=ScreenMain;a.menu_item=1;
        press(&a,InputKeyOk);assert(a.screen==ScreenRole);if(mode==GpModeDriver)press(&a,InputKeyDown);press(&a,InputKeyOk);assert(a.screen==ScreenTeam&&a.new_weekend);if(mode==GpModeDriver)press(&a,InputKeyUp);
        press(&a,InputKeyOk);assert(a.screen==ScreenTrack);a.chosen_track=5;
        press(&a,InputKeyOk);assert(a.screen==ScreenDistance);a.full_distance=full;
        press(&a,InputKeyOk);assert(a.screen==ScreenWeekendReplace);
        press(&a,InputKeyOk);assert(a.screen==ScreenWeekend);checkpoint(&a);
        uint32_t mask=a.weekend.sprint_mask;unsigned races=0,sprints=0,practice=0,qual=0;
        /* Starting another mode must not overwrite the existing weekend. */
        GpApp navigation=a;navigation.screen=ScreenMain;navigation.menu_item=2;press(&navigation,InputKeyOk);assert(!navigation.save_requested);
        for(unsigned steps=0;a.weekend.stage!=GpStageDone;steps++){
            assert(steps<10000&&a.weekend.sprint_mask==mask);assert(a.race.mode==(GpMode)mode);if(mode==GpModeDriver)assert(a.race.active_slot==1);
            switch(a.screen){
            case ScreenWeekend:
                if(a.weekend.stage<=GpStageFP3)practice++;
                else if(a.weekend.stage==GpStageQual||a.weekend.stage==GpStageSprintQual)qual++;
                press(&a,InputKeyOk);break;
            case ScreenQualifying:
                press(&a,InputKeyDown); // exercise persisted compound selection
                if(a.save_requested)checkpoint(&a);
                press(&a,InputKeyOk);break;
            case ScreenQualResults:case ScreenGrid:press(&a,InputKeyOk);break;
            case ScreenEngineer:case ScreenRedFlag:press(&a,InputKeyOk);break;
            case ScreenMap:
                a.clock.paused=false;advance_time(&a,a.last_tick+GP_LAP_MS);
                if(a.weekend.stage==GpStageSprint)assert(a.race.total_laps==gp_sprint_laps(a.race.track,full));
                break;
            case ScreenResults:
                if(a.weekend.stage==GpStageSprint)sprints++;else races++;
                press(&a,InputKeyOk);break;
            default:fprintf(stderr,"unexpected screen %d\n",a.screen);abort();
            }
            checkpoint(&a);
        }
        assert(a.weekend.rounds_done==23&&races==23&&sprints==6&&practice==57&&qual==29);
        assert(a.screen==ScreenWeekend);
        unsigned archived=0;for(unsigned t=0;t<GP_TRACK_COUNT;t++)for(unsigned k=0;k<2;k++)archived+=a.weekend.results[t][k].recorded;
        assert(archived==29);uint16_t total=0;for(unsigned t=0;t<GP_TEAM_COUNT;t++)total+=a.weekend.team_points[t];assert(total>0);
        press(&a,InputKeyRight);assert(a.screen==ScreenChampionship);press(&a,InputKeyRight);assert(a.championship_teams);press(&a,InputKeyBack);
        press(&a,InputKeyUp);assert(a.screen==ScreenHistory);press(&a,InputKeyOk);assert(a.screen==ScreenHistoryResult);press(&a,InputKeyBack);press(&a,InputKeyBack);
        press(&a,InputKeyOk);assert(a.screen==ScreenChampions);press(&a,InputKeyBack);
        /* Save and exit / missing SD use the same guarded pause path. */
        press(&a,InputKeyBack);assert(a.screen==ScreenPause);press(&a,InputKeyDown);
        gp_test_fs.no_card=true;press(&a,InputKeyOk);process_storage(&a);
        assert(a.screen==ScreenNotice&&a.save_status==GpSaveNoCard);
        press(&a,InputKeyOk);assert(a.screen==ScreenPause);gp_test_fs.no_card=false;
        press(&a,InputKeyOk);process_storage(&a);assert(a.screen==ScreenMain&&a.save_status==GpSaveOk);
        press(&a,InputKeyOk);process_storage(&a);assert(a.screen==ScreenWeekend&&a.weekend.stage==GpStageDone);
    }
    printf("PASS: 10000 six-sprint calendars; four complete 23-round seasons, team/driver modes, 57 practices/29 qualifying sessions/29 races each; %u save/reload checkpoints\n",checkpoints);
}
