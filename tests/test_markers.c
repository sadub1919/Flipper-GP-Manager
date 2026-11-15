#include <assert.h>
#include "../flipper_gp_manager.c"
int main(void){
    unsigned cases=0;
    for(unsigned track=0;track<GP_TRACK_COUNT;track++){
        uint8_t xy[180];const GpTrackShape* source=gp_track_shape(track);fit_circuit(source,xy,5,17,117,30);GpTrackShape fitted={xy,source->count};
        for(unsigned selected=0;selected<GP_DRIVER_COUNT;selected++)for(unsigned clustered=0;clustered<2;clustered++)for(unsigned phase=0;phase<256;phase+=32){
            GpRace race;gp_race_start(&race,selected/2,selected%2,track,123);
            for(unsigned i=0;i<GP_DRIVER_COUNT;i++){race.ranking[i]=i;race.drivers[i].time_ms=clustered?0:i*13000u;race.drivers[i].last_lap_ms=80000;}
            GpMapMarker m[3];unsigned n=map_markers(&fitted,&race,phase,m);
            assert(n==((selected==0||selected==21)?2:3));assert(m[0].id==selected);
            unsigned index=1;if(selected)assert(m[index++].id==selected-1);if(selected<21)assert(m[index].id==selected+1);
            for(unsigned i=0;i<n;i++){
                assert(m[i].x>=5&&m[i].x<=122&&m[i].y>=17&&m[i].y<=47);
                assert(m[i].lx>=2&&m[i].lx+18<=126&&m[i].ly>=13&&m[i].ly+10<=51);
                for(unsigned j=0;j<i;j++)assert(!rectangles_overlap(m[i].lx,m[i].ly,18,10,m[j].lx,m[j].ly,18,10));
                for(unsigned j=0;j<n;j++)assert(!rectangles_overlap(m[i].lx,m[i].ly,18,10,m[j].x-3,m[j].y-3,7,7));
            }
            cases++;
        }
    }
    printf("PASS: %u marker layouts, all 23 tracks/22 positions, coincident and lapped cars; correct neighbours, no clipped or overlapping labels\n",cases);
}
