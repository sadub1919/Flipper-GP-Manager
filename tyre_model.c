#include "tyre_model.h"
uint8_t gp_tyre_optimal(GpTyre tyre){
    static const uint8_t optimum[]={95,95,100,75,60};
    return (unsigned)tyre<GpTyreCount?optimum[tyre]:95;
}
uint8_t gp_tyre_blanket(GpTyre tyre){return tyre==GpTyreWet?35:(tyre==GpTyreIntermediate?45:60);}
int gp_tyre_temperature_state(GpTyre tyre,uint8_t temperature){
    int optimum=gp_tyre_optimal(tyre);
    return temperature<optimum-10?-1:(temperature>optimum+10?1:0);
}
GpTyreThermal gp_tyre_thermal_step(GpTyre tyre,uint8_t temperature,GpPace pace,const GpTrackInfo* track,const GpWeather* weather,GpEvent event){
    static const uint8_t heating[]={20,14,10,14,12};
    int optimum=gp_tyre_optimal(tyre),target=optimum+(track->tyre_wear-9)*2;
    target+=pace==GpPaceAttack?22:(pace==GpPaceConserve?-12:0);
    if(tyre<=GpTyreHard)target-=weather->wetness/4;
    else if(tyre==GpTyreIntermediate)target+=(45-(int)weather->wetness)/2;
    else target+=70-(int)weather->wetness;
    if(event==GpEventSafetyCar)target-=28;
    else if(event==GpEventVsc)target-=14;
    else if(event==GpEventYellow)target-=6;
    if(target<GP_TEMPERATURE_MIN)target=GP_TEMPERATURE_MIN;
    if(target>GP_TEMPERATURE_MAX)target=GP_TEMPERATURE_MAX;
    int step=(unsigned)tyre<GpTyreCount?heating[tyre]:14;
    step+=pace==GpPaceAttack?4:(pace==GpPaceConserve?-2:0);
    if(track->length_m>=6000)step+=2;
    int next=temperature;
    if(next<target){next+=step;if(next>target)next=target;}
    else{next-=event==GpEventSafetyCar?20:12;if(next<target)next=target;}
    if(next<GP_TEMPERATURE_MIN)next=GP_TEMPERATURE_MIN;
    if(next>GP_TEMPERATURE_MAX)next=GP_TEMPERATURE_MAX;
    int mean=((int)temperature+next)/2,loss=0;
    if(mean<optimum-10)loss=(optimum-10-mean)*140;
    if(mean>optimum+10)loss=(mean-optimum-10)*180;
    unsigned wear=next>optimum+10?(unsigned)(next-optimum-10+3)/4:0;
    return (GpTyreThermal){(uint16_t)loss,(uint8_t)next,(uint8_t)wear};
}
