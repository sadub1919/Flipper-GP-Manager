#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mock_save_io.h"
static void crc(uint8_t* b){uint32_t c=UINT32_MAX;for(unsigned i=0;i<GP_SAVE_BYTES-4;i++){c^=b[i];for(unsigned j=0;j<8;j++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}c=~c;for(unsigned i=0;i<4;i++)b[GP_SAVE_BYTES-4+i]=(uint8_t)(c>>(i*8));}
static void finish(GpRace* r,unsigned track,bool sprint,GpMode mode){
    gp_race_start(r,0,1,track,123);gp_race_set_mode(r,mode,1);r->total_laps=sprint?5:12;r->lap=r->total_laps;
    for(unsigned i=0;i<GP_DRIVER_COUNT;i++){r->ranking[i]=i;r->drivers[i].time_ms=100000+i*1000;}
}
static void season(GpSavedGame* game,GpMode mode){
    GpWeekend* w=&game->weekend;
    for(unsigned t=0;t<GP_TRACK_COUNT;t++){
        gp_qualify(&w->qualifying,t,123);w->qualifying.mode=mode;w->qualifying.controlled_slot=1;
        for(unsigned i=0;i<GP_DRIVER_COUNT;i++){w->qualifying.grid[i]=i;w->qualifying.lap_ms[i]=80000+i*1000;}
        w->stage=GpStageQual;gp_career_record_qual(w,&w->qualifying);gp_career_record_qual(w,&w->qualifying);
        if(gp_weekend_sprint(w,t)){w->stage=GpStageSprint;finish(&game->race,t,true,mode);assert(gp_weekend_record(w,&game->race));}
        w->stage=GpStageRace;w->phase=GpWeekendResult;finish(&game->race,t,false,mode);
        assert(gp_weekend_record(w,&game->race)&&!gp_weekend_record(w,&game->race));assert(gp_weekend_next(w,t));
    }
    assert(w->stage==GpStageDone&&w->driver_points[0]==623);
}
int main(void){
    for(unsigned mode=0;mode<2;mode++){
        GpSavedGame a={0},b={0};gp_weekend_start(&a.weekend,false,123);GpTestFs fs={0};GpSaveIo io=test_io(&fs);io.profile=mode;
        for(unsigned year=1;year<=8;year++){
            season(&a,mode);uint8_t score[2];gp_career_compare(&a.weekend,&a.race,score,true);assert(score[0]==23&&score[1]==0);
            gp_career_compare(&a.weekend,&a.race,score,false);assert(score[0]==23&&score[1]==0);
            assert(gp_career_stat(&a.weekend,&a.race,0,0,true)==year*23);
            assert(gp_career_stat(&a.weekend,&a.race,0,1,true)==year*23);
            assert(gp_career_stat(&a.weekend,&a.race,0,3,true)==year*23);
            assert(gp_career_stat(&a.weekend,&a.race,0,4,true)==year*623);
            assert(gp_career_stat(&a.weekend,&a.race,0,5,true)==year);
            assert(gp_career_goal_met(&a.weekend,&a.race,0)&&gp_career_goal_met(&a.weekend,&a.race,1));
            assert(gp_save_store(&io,&a)==GpSaveOk&&gp_save_load(&io,&b)==GpSaveOk);
            uint8_t x[GP_SAVE_BYTES],y[GP_SAVE_BYTES];a.sequence=b.sequence;
            assert(gp_save_encode(&a,x)&&gp_save_encode(&b,y)&&!memcmp(x,y,sizeof(x)));a=b;
            if(year==1){
                uint8_t legacy[GP_SAVE_BYTES];memcpy(legacy,x,sizeof(x));legacy[4]=7;memset(legacy+1728,0,GP_SAVE_BYTES-1728-4);crc(legacy);
                assert(gp_save_decode(legacy,sizeof(legacy),&b)==GpSaveOk&&b.weekend.career.season==1&&b.weekend.career.partial_stats);
                assert(b.weekend.driver_points[0]==623&&gp_career_stat(&b.weekend,&b.race,0,3,true)==0);
                assert(gp_career_next(&b.weekend,&b.race,321)&&b.weekend.career.partial_totals&&!b.weekend.career.partial_stats&&b.weekend.career.history[0].partial);
                unsigned offsets[]={1728,1732,1733,1734,1735,1783,1852};
                for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++){memcpy(y,x,sizeof(x));y[offsets[i]]=255;crc(y);assert(gp_save_decode(y,sizeof(y),&b)==GpSaveCorrupt);}
            }
            assert(gp_career_next(&a.weekend,&a.race,year+123));assert(!gp_career_next(&a.weekend,&a.race,999));
            assert(a.weekend.career.season==year+1&&a.weekend.rounds_done==0&&a.weekend.driver_points[0]==0);
            assert(a.weekend.career.history_count==(year<5?year:5));
            assert(a.weekend.career.history[a.weekend.career.history_count-1].season==year);
            assert(a.weekend.career.history[0].season==(year<5?1:year-4));
            assert(gp_save_store(&io,&a)==GpSaveOk&&gp_save_load(&io,&b)==GpSaveOk);a=b;
        }
        assert(gp_career_stat(&a.weekend,&a.race,0,4,true)==4984);
    }
    /* Season goals for a midfield and a weaker team. */
    GpSavedGame a={0};gp_weekend_start(&a.weekend,false,1);gp_race_start(&a.race,4,0,0,1);
    assert(gp_career_goal(&a.race)==0&&!gp_career_goal_met(&a.weekend,&a.race,0));a.weekend.driver_points[8]=1;assert(gp_career_goal_met(&a.weekend,&a.race,0));
    gp_race_start(&a.race,10,0,0,1);assert(gp_career_goal(&a.race)==2&&!gp_career_goal_met(&a.weekend,&a.race,0));
    a.weekend.stage=GpStageDone;assert(gp_career_goal_met(&a.weekend,&a.race,0));a.weekend.incomplete_history=true;assert(!gp_career_goal_met(&a.weekend,&a.race,0));
    puts("PASS career: 16 seasons, lifetime stats/titles, five-season archive rollover, teammate scores, goals, idempotent advancement, slot saves, v7 migration and corrupt fields");
}
