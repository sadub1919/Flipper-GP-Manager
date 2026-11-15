#ifndef GP_WEEKEND_H
#define GP_WEEKEND_H
#include "qualifying.h"
#define GP_SPRINT_COUNT 6
/* Stable values are part of save format 2. */
typedef enum {GpStageFP1,GpStageFP2,GpStageFP3,GpStageSprintQual,GpStageSprint,GpStageQual,GpStageRace,GpStageDone} GpStage;
typedef enum {GpWeekendReady,GpWeekendQualSetup,GpWeekendQualResult,GpWeekendGrid,GpWeekendRacing,GpWeekendResult} GpWeekendPhase;
typedef struct {bool recorded;uint8_t finish[GP_DRIVER_COUNT];} GpRoundResult; /* ID | 0x80 for DNF */
typedef struct {
    GpRoundResult results[GP_TRACK_COUNT][2]; /* Grand Prix, sprint */
    bool incomplete_history;
    uint16_t driver_points[GP_DRIVER_COUNT],team_points[GP_TEAM_COUNT];
    uint8_t driver_order[GP_DRIVER_COUNT],team_order[GP_TEAM_COUNT];
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
bool gp_weekend_record(GpWeekend*,const GpRace*);
void gp_championship_rebuild(GpWeekend*);
bool gp_weekend_valid(const GpWeekend*,const GpRace*);
#endif
