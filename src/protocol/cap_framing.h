#ifndef CAP_FRAMING_H
#define CAP_FRAMING_H

#include <stddef.h>
#include <stdint.h>

#include "cap_messages.h"

typedef struct {
    uint8_t buffer[CAP_MAX_MESSAGE_SIZE];
    size_t len;
    size_t expected_len;
} cap_frame_reassembler_t;

void cap_framing_init(cap_frame_reassembler_t *state);
void cap_framing_reset(cap_frame_reassembler_t *state);

int cap_framing_feed(cap_frame_reassembler_t *state,
                     const uint8_t *data,
                     size_t len,
                     const uint8_t **out_msg,
                     size_t *out_len);

#endif
