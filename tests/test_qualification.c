#include "../qualifying.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    unsigned long scenarios=0;
    for(uint8_t track=0;track<GP_TRACK_COUNT;++track){
        for(uint32_t seed=1;seed<=20;++seed){
            GpQualifying q;gp_qualify(&q,track,seed*123u);
            int seen[GP_DRIVER_COUNT]={0};
            for(int p=0;p<GP_DRIVER_COUNT;++p){
                uint8_t id=q.grid[p];assert(id<GP_DRIVER_COUNT);
                assert(!seen[id]);seen[id]=1;
                assert(gp_grid_position(&q,id)==p+1);
                assert(q.lap_ms[id]>=1000);
                if(p>0&&p!=10&&p!=16)assert(q.lap_ms[q.grid[p-1]]<=q.lap_ms[id]);
            }
            GpRace r;
            gp_race_start(&r,(uint8_t)(seed%GP_TEAM_COUNT),0,track,seed);
            gp_race_apply_grid(&r,q.grid);
            for(int p=0;p<GP_DRIVER_COUNT;++p)assert(r.ranking[p]==q.grid[p]);
            uint8_t first=r.ranking[0];
            gp_advance_lap(&r);
            assert(r.lap==1 && gp_position(&r,first)>=1 && gp_position(&r,first)<=GP_DRIVER_COUNT);
            ++scenarios;
        }
    }
    for(uint32_t seed=1;seed<=300;seed++){
        GpQualifying q;gp_qualifying_start(&q,seed%GP_TRACK_COUNT,seed%GP_TEAM_COUNT,seed);
        uint8_t frozen[GP_DRIVER_COUNT];uint32_t times[GP_DRIVER_COUNT];
        for(int session=1;session<=3;session++){
            assert(q.session==session&&q.run==0);
            uint8_t count=q.active_count;
            assert(count==(session==1?22:(session==2?16:10)));
            for(int p=count;p<GP_DRIVER_COUNT;p++)assert(q.grid[p]==frozen[p]&&q.lap_ms[q.grid[p]]==times[q.grid[p]]);
            assert(gp_qualifying_run(&q));memcpy(times,q.lap_ms,sizeof(times));
            assert(gp_qualifying_run(&q));assert(!gp_qualifying_run(&q));
            for(int p=0;p<count;p++)assert(q.lap_ms[q.grid[p]]<=times[q.grid[p]]);
            memcpy(frozen,q.grid,sizeof(frozen));memcpy(times,q.lap_ms,sizeof(times));
            assert(gp_qualifying_next(&q)==(session<3));
        }
        assert(q.complete&&!gp_qualifying_next(&q));
        /* Same seed: compound differences are exact and rivals are unaffected. */
        GpQualifying soft,medium,hard;
        gp_qualifying_start(&soft,0,0,seed);soft.tyre[0]=GpTyreSoft;medium=hard=soft;
        medium.tyre[0]=GpTyreMedium;hard.tyre[0]=GpTyreHard;
        gp_qualifying_run(&soft);gp_qualifying_run(&medium);gp_qualifying_run(&hard);
        assert(medium.lap_ms[0]>soft.lap_ms[0]);
        assert(hard.lap_ms[0]>medium.lap_ms[0]);
        for(int i=1;i<GP_DRIVER_COUNT;i++)assert(soft.lap_ms[i]==medium.lap_ms[i]);
        GpQualifying reused;gp_qualifying_start(&reused,0,0,seed);reused.soft_used[0]=4;reused.tyre[0]=GpTyreSoft;
        gp_qualifying_run(&reused);assert(reused.lap_ms[0]==soft.lap_ms[0]+1200);
        GpQualifying slow,attack;gp_qualifying_start(&slow,0,0,seed);slow.tyre[0]=GpTyreSoft;attack=slow;
        slow.pace[0]=GpPaceConserve;attack.pace[0]=GpPaceAttack;
        gp_qualifying_run(&slow);gp_qualifying_run(&attack);
        assert(slow.lap_ms[0]>soft.lap_ms[0]);
        assert(attack.lap_ms[0]!=soft.lap_ms[0]);
    }
    printf("PASS: %lu qualifying sessions, real grid used at race start\n",scenarios);
}
