#ifndef FLIPPER_GP_RACE_ENGINE_H
#define FLIPPER_GP_RACE_ENGINE_H

#include <stdbool.h>
#include <stdint.h>
#include "weather.h"

#define GP_TEAM_COUNT 11
#define GP_DRIVER_COUNT 22
#define GP_TRACK_COUNT 23
#define GP_TOTAL_LAPS 12 /* short distance */

typedef enum {GpTyreSoft=0,GpTyreMedium=1,GpTyreHard=2,GpTyreIntermediate=3,GpTyreWet=4,GpTyreCount=5} GpTyre;
typedef enum {GpPaceConserve=0,GpPaceNormal=1,GpPaceAttack=2} GpPace;
typedef enum {GpEventNone=0,GpEventYellow=1,GpEventVsc=2,GpEventSafetyCar=3,GpEventRed=4} GpEvent;

typedef struct {
    const char* name;
    const char* code;
    uint8_t rating; /* game balance, not real F1 performance */
} GpTeamInfo;

typedef struct {
    const char* full_name;
    const char* short_name;
    const char* code;
    uint8_t team;
    uint8_t rating; /* game balance, not real F1 performance */
} GpDriverInfo;

typedef struct {
    const char* name;
    const char* circuit;
    uint8_t actual_laps;
    uint16_t length_m;
    uint8_t tyre_wear;
    uint8_t difficulty;
    uint8_t overtaking; /* game balance: 1 narrow, 5 many opportunities */
} GpTrackInfo;

typedef enum {GpDriverRunning=0,GpDriverAccident=1,GpDriverFailure=2} GpDriverStatus;
typedef struct {
    GpDriverStatus status;
    uint8_t retired_lap;
    uint8_t driver_index;
    uint8_t wear; /* 100 -> 0 */
    uint8_t temperature; /* Celsius, tyre_model.h defines bounds */
    uint8_t wear_fraction; /* fractional loss, denominator total_laps */
    GpTyre tyre;
    uint32_t time_ms;
    uint32_t last_lap_ms;
    uint8_t pits;
} GpDriver;

typedef enum {GpBattleNone=0,GpBattlePassed,GpBattleHeld,GpBattleBlocked,GpBattleLost,GpBattleMistake} GpBattleResult;
typedef struct {
    uint8_t result,opponent,lap;
} GpBattleReport;
typedef struct {
    GpDriver drivers[GP_DRIVER_COUNT];
    uint8_t ranking[GP_DRIVER_COUNT];
    uint8_t lap;
    uint8_t total_laps;
    uint8_t team;
    uint8_t player; /* currently selected player driver, 2*team+active_slot */
    uint8_t active_slot;
    uint8_t track;
    GpPace pace[2];
    bool pit_queued[2];
    GpTyre pit_tyres[2];
    GpEvent event;
    uint8_t event_serial;
    uint8_t event_remaining;
    uint8_t incident_driver; /* GP_DRIVER_COUNT means none */
    uint8_t incident_serial;
    bool incident_pending;
    uint32_t incident_rng; /* independent stream; zero disables spontaneous incidents */
    uint32_t battle_rng; /* zero preserves legacy racing rules */
    GpBattleReport battle[2]; /* last on-track contest for each player driver */
    uint32_t rng_state;
    GpWeather weather;
} GpRace;

const GpTeamInfo* gp_team_info(uint8_t team);
const GpDriverInfo* gp_driver_info(uint8_t driver);
const GpTrackInfo* gp_track_info(uint8_t track);
const char* gp_pace_name(GpPace pace);
const char* gp_tyre_name(GpTyre tyre);
const char* gp_event_name(GpEvent event);
void gp_race_start(GpRace* race,uint8_t team,uint8_t driver_slot,uint8_t track,uint32_t seed);
void gp_select_driver(GpRace* race,uint8_t slot);
void gp_race_apply_grid(GpRace* race,const uint8_t grid[GP_DRIVER_COUNT]);
void gp_advance_lap(GpRace* race);
uint8_t gp_position(const GpRace* race,uint8_t driver_index);
uint32_t gp_gap_ms(const GpRace* race,uint8_t driver_index);
uint8_t gp_points_for_position(uint8_t position);
uint8_t gp_running_count(const GpRace*);
bool gp_race_finished(const GpRace*);
bool gp_driver_precedes(const GpDriver*,const GpDriver*);
bool gp_retire_driver(GpRace*,uint8_t,GpDriverStatus);
bool gp_set_event(GpRace*,GpEvent,uint8_t laps);
bool gp_restart_race(GpRace*);
const char* gp_retirement_name(GpDriverStatus);
GpPace gp_driver_pace(const GpRace*,uint8_t driver);
uint32_t gp_pit_loss_ms(const GpRace* race);
uint8_t gp_pit_rejoin_position(const GpRace* race,uint8_t driver);
bool gp_pit_traffic(const GpRace* race,uint8_t driver);

#endif
