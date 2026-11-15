#ifndef GP_TRACK_SHAPES_H
#define GP_TRACK_SHAPES_H

#include <stdint.h>

typedef struct {
    const uint8_t* xy; /* interleaved x,y points in local 0..100 coordinates */
    uint8_t count; /* number of XY points */
} GpTrackShape;

const GpTrackShape* gp_track_shape(uint8_t track);

#endif
