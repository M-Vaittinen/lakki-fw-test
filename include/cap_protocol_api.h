#ifndef CAP_PROTOCOL_API_H
#define CAP_PROTOCOL_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*cap_msg_cb)(uint32_t type, const uint8_t *msg, size_t len, void *ctx);

typedef struct {
    cap_msg_cb on_message;
    void *ctx;
} cap_protocol_config_t;

void cap_protocol_init(cap_protocol_config_t config);
void cap_protocol_reset(void);
void cap_protocol_feed_bytes(const uint8_t *data, size_t len);

size_t cap_build_message(uint32_t type,
                         const void *header8,
                         const uint8_t *tlv,
                         size_t tlv_len,
                         uint8_t *out_buf,
                         size_t out_buf_len);

#ifdef __cplusplus
}
#endif

#endif
