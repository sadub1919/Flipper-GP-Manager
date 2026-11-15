#ifndef GP_WEEKEND_H
#define GP_WEEKEND_H
#include "qualifying.h"
#define GP_SPRINT_COUNT 6
/* Stable values are part of save format 2. */
typedef enum {GpStageFP1,GpStageFP2,GpStageFP3,GpStageSprintQual,GpStageSprint,GpStageQual,GpStageRace,GpStageDone} GpStage;
typedef enum {GpWeekendReady,GpWeekendQualSetup,GpWeekendQualResult,GpWeekendGrid,GpWeekendRacing,GpWeekendResult} GpWeekendPhase;
typedef struct {bool recorded;uint8_t finish[GP_DRIVER_COUNT];} GpRoundResult; /* ID | 0x80 for DNF */
#define GP_CAREER_HISTORY 5
#define GP_CAREER_STATS 6
/* GP starts, wins, podiums, poles, points (including sprints), driver titles. */
typedef struct {
    uint32_t season,points[2];
    uint8_t position[2],wins[2],podiums[2],poles[2],qualifying[2];
    bool partial;
} GpSeasonSummary;
typedef struct {
    uint32_t season,totals[2][GP_CAREER_STATS];
    bool partial_stats,partial_totals;
    uint8_t history_count,qual_pos[GP_TRACK_COUNT][2],pole[GP_TRACK_COUNT];
    GpSeasonSummary history[GP_CAREER_HISTORY];
} GpCareer;
typedef struct {
    GpCareer career;
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
void gp_career_record_qual(GpWeekend*,const GpQualifying*);
uint32_t gp_career_stat(const GpWeekend*,const GpRace*,uint8_t slot,uint8_t stat,bool lifetime);
void gp_career_compare(const GpWeekend*,const GpRace*,uint8_t out[2],bool qualifying);
uint8_t gp_career_goal(const GpRace*);
bool gp_career_goal_met(const GpWeekend*,const GpRace*,uint8_t slot);
bool gp_career_next(GpWeekend*,const GpRace*,uint32_t seed);
bool gp_career_valid(const GpWeekend*,const GpRace*);
bool gp_weekend_valid(const GpWeekend*,const GpRace*);
#endif
