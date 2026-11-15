#ifndef GP_MOCK_RANDOM_H
#define GP_MOCK_RANDOM_H
#include <stdint.h>
static inline uint32_t furi_hal_random_get(void){static uint32_t seed=62026;return ++seed;}
#endif
