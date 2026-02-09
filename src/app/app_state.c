#include "app_state.h"

void app_state_init(app_state_t *state) {
    if (!state) {
        return;
    }
    state->target_direction = 0;
    state->target_distance = 0;
    state->has_target = false;
    state->cap_direction = 0;
    state->stream_enabled = false;
}
