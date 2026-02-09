#include "cap_framing.h"

#include <string.h>

#include "../util/endian.h"

void cap_framing_init(cap_frame_reassembler_t *state) {
    cap_framing_reset(state);
}

void cap_framing_reset(cap_frame_reassembler_t *state) {
    if (!state) {
        return;
    }
    state->len = 0;
    state->expected_len = 0;
}

static int cap_framing_update_expected(cap_frame_reassembler_t *state) {
    if (state->len < 8) {
        return 0;
    }
    uint32_t total_len = cap_read_u32_be(&state->buffer[4]);
    if (total_len < CAP_MSG_BASE_SIZE || total_len > CAP_MAX_MESSAGE_SIZE) {
        cap_framing_reset(state);
        return -1;
    }
    state->expected_len = (size_t)total_len;
    return 0;
}

int cap_framing_feed(cap_frame_reassembler_t *state,
                     const uint8_t *data,
                     size_t len,
                     const uint8_t **out_msg,
                     size_t *out_len) {
    if (!state || !data || !out_msg || !out_len) {
        return -1;
    }

    *out_msg = NULL;
    *out_len = 0;

    size_t offset = 0;
    while (offset < len) {
        size_t remaining = len - offset;
        size_t space = CAP_MAX_MESSAGE_SIZE - state->len;
        size_t to_copy = remaining < space ? remaining : space;

        memcpy(&state->buffer[state->len], &data[offset], to_copy);
        state->len += to_copy;
        offset += to_copy;

        if (state->expected_len == 0) {
            if (cap_framing_update_expected(state) < 0) {
                return -1;
            }
        }

        if (state->expected_len > 0 && state->len >= state->expected_len) {
            *out_msg = state->buffer;
            *out_len = state->expected_len;

            size_t extra = state->len - state->expected_len;
            if (extra > 0) {
                memmove(state->buffer, &state->buffer[state->expected_len], extra);
            }
            state->len = extra;
            state->expected_len = 0;
            cap_framing_update_expected(state);
            return 1;
        }
    }

    return 0;
}
