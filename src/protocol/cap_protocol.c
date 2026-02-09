#include "cap_protocol.h"

#include <string.h>

#include "../util/endian.h"
#include "cap_framing.h"
#include "../../include/cap_protocol_api.h"

static cap_protocol_config_t g_config;
static cap_frame_reassembler_t g_reassembler;

void cap_protocol_init(cap_protocol_config_t config) {
    g_config = config;
    cap_framing_init(&g_reassembler);
}

void cap_protocol_reset(void) {
    cap_framing_reset(&g_reassembler);
}

int cap_parse_message(const uint8_t *msg, size_t len, uint32_t *out_type, const uint8_t **out_header) {
    if (!msg || len < CAP_MSG_BASE_SIZE || !out_type || !out_header) {
        return -1;
    }

    uint32_t type = cap_read_u32_be(&msg[0]);
    uint32_t total_len = cap_read_u32_be(&msg[4]);
    if (total_len != len) {
        return -1;
    }

    *out_type = type;
    *out_header = &msg[8];
    return 0;
}

void cap_protocol_feed_bytes(const uint8_t *data, size_t len) {
    if (!g_config.on_message) {
        return;
    }

    size_t offset = 0;
    while (offset < len) {
        const uint8_t *msg = NULL;
        size_t msg_len = 0;
        int result = cap_framing_feed(&g_reassembler, &data[offset], len - offset, &msg, &msg_len);
        if (result < 0) {
            return;
        }
        if (result == 1 && msg && msg_len > 0) {
            uint32_t type = 0;
            const uint8_t *header = NULL;
            if (cap_parse_message(msg, msg_len, &type, &header) == 0) {
                g_config.on_message(type, msg, msg_len, g_config.ctx);
            }
        }
        offset = len;
    }
}

size_t cap_build_message(uint32_t type,
                         const void *header8,
                         const uint8_t *tlv,
                         size_t tlv_len,
                         uint8_t *out_buf,
                         size_t out_buf_len) {
    if (!header8 || !out_buf) {
        return 0;
    }

    size_t total_len = CAP_MSG_BASE_SIZE + tlv_len;
    if (total_len > out_buf_len || total_len > CAP_MAX_MESSAGE_SIZE) {
        return 0;
    }

    cap_write_u32_be(&out_buf[0], type);
    cap_write_u32_be(&out_buf[4], (uint32_t)total_len);
    memcpy(&out_buf[8], header8, CAP_MSG_HEADER_SIZE);
    if (tlv_len > 0 && tlv) {
        memcpy(&out_buf[8 + CAP_MSG_HEADER_SIZE], tlv, tlv_len);
    }

    return total_len;
}
