#ifndef CAP_MESSAGES_H
#define CAP_MESSAGES_H

#include <stdint.h>

#define CAP_PROTOCOL_VERSION 1u
#define CAP_MSG_HEADER_SIZE 8u
#define CAP_MSG_BASE_SIZE 16u
#define CAP_MAX_MESSAGE_SIZE 1024u

typedef enum {
    CAP_MSG_HANDSHAKE = 1,
    CAP_MSG_DESTINATION = 2,
    CAP_MSG_MOVEMENT = 3,
    CAP_MSG_DESTINATION_REQUEST = 4,
    CAP_MSG_CAP_DIRECTION = 5,
    CAP_MSG_CAP_DIRECTION_REQUEST_START = 6,
    CAP_MSG_CAP_DIRECTION_REQUEST_STOP = 7
} cap_message_type_t;

typedef struct {
    uint32_t protocol_version;
    uint32_t capabilities;
} cap_handshake_header_t;

typedef struct {
    uint32_t direction;
    uint32_t distance_meters;
} cap_destination_header_t;

typedef struct {
    uint32_t direction;
    uint32_t speed_cm_s;
} cap_movement_header_t;

typedef struct {
    uint32_t reserved0;
    uint32_t reserved1;
} cap_reserved_header_t;

typedef struct {
    uint32_t direction;
    uint32_t reserved;
} cap_direction_header_t;

#endif
