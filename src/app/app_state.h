#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t target_direction;
    uint32_t target_distance;
    bool has_target;
    uint32_t cap_direction;
    bool stream_enabled;
} app_state_t;

void app_state_init(app_state_t *state);

#endif
