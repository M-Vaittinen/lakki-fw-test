#ifndef CAP_PROTOCOL_H
#define CAP_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "cap_messages.h"

int cap_parse_message(const uint8_t *msg, size_t len, uint32_t *out_type, const uint8_t **out_header);

#endif
