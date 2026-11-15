#include "../team_logos.h"
#include "../race_engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    assert(GP_TEAM_COUNT==11);
    assert(GP_TEAM_LOGO_W==48 && GP_TEAM_LOGO_H==32);
    for(uint8_t t=0;t<GP_TEAM_COUNT;++t){
        const uint8_t* image=gp_logo_data(t);
        int count=0;
        for(int y=0;y<GP_TEAM_LOGO_H;++y){
            for(int x=0;x<GP_TEAM_LOGO_W;++x){
                if(image[y*GP_TEAM_LOGO_ROW+x/8]&(1u<<(x%8)))++count;
            }
            /* 48 pixels is byte-aligned: no padding bits. */
        }
        assert(count>=45 && count<=GP_TEAM_LOGO_W*GP_TEAM_LOGO_H);
        for(uint8_t u=0;u<t;++u)assert(memcmp(image,gp_logo_data(u),GP_TEAM_LOGO_BYTES)!=0);
    }
    puts("PASS: 11 distinct 48x32 logos, valid bitmap bounds, nonempty");
}
