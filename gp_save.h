#ifndef GP_SAVE_H
#define GP_SAVE_H
#include <stddef.h>
#include "race_engine.h"
#include "race_clock.h"
#include "weekend.h"
#define GP_SAVE_BYTES 640u
#define GP_LAP_MS 6000u
/* The persisted clock uses milliseconds, not platform-specific Furi ticks. */
typedef struct { GpRace race; GpRaceClock clock; uint32_t sequence; GpWeekend weekend; } GpSavedGame;
typedef enum {
    GpSaveOk, GpSaveRecovered, GpSaveMissing, GpSaveNoCard,
    GpSaveCorrupt, GpSaveVersion, GpSaveIoError,
} GpSaveStatus;
typedef enum { GpIoOk, GpIoMissing, GpIoError } GpIoResult;
/* Slots 0/1 retain two generations. Slot 2 is a temporary candidate.
   replace() must leave the other numbered slot untouched on every failure. */
typedef struct {
    void* context;
    bool (*ready)(void*);
    GpIoResult (*read)(void*,unsigned,uint8_t*,size_t,size_t*);
    bool (*write)(void*,unsigned,const uint8_t*,size_t);
    bool (*replace)(void*,unsigned,unsigned);
} GpSaveIo;
bool gp_save_encode(const GpSavedGame*,uint8_t out[GP_SAVE_BYTES]);
GpSaveStatus gp_save_decode(const uint8_t*,size_t,GpSavedGame*);
GpSaveStatus gp_save_load(const GpSaveIo*,GpSavedGame*);
GpSaveStatus gp_save_store(const GpSaveIo*,const GpSavedGame*);
#endif
