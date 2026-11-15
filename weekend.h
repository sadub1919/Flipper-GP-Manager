#ifndef GP_WEEKEND_H
#define GP_WEEKEND_H
#include "qualifying.h"
#define GP_SPRINT_COUNT 6
/* Stable values are part of save format 2. */
typedef enum {GpStageFP1,GpStageFP2,GpStageFP3,GpStageSprintQual,GpStageSprint,GpStageQual,GpStageRace,GpStageDone} GpStage;
typedef enum {GpWeekendReady,GpWeekendQualSetup,GpWeekendQualResult,GpWeekendGrid,GpWeekendRacing,GpWeekendResult} GpWeekendPhase;
typedef struct {
    bool active,full_distance;
    uint8_t rounds_done;
    GpStage stage;
    GpWeekendPhase phase;
    uint32_t sprint_mask;
    GpQualifying qualifying;
} GpWeekend;
uint32_t gp_sprint_calendar(uint32_t seed);
bool gp_weekend_sprint(const GpWeekend*,uint8_t track);
void gp_weekend_start(GpWeekend*,bool full,uint32_t seed);
bool gp_weekend_next(GpWeekend*,uint8_t track);
uint8_t gp_sprint_laps(uint8_t track,bool full);
const char* gp_stage_name(GpStage);
bool gp_weekend_valid(const GpWeekend*,const GpRace*);
#endif
