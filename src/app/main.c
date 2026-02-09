#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_state.h"
#include "console_ui.h"
#include "../ble/ble_transport.h"
#include "../protocol/cap_messages.h"
#include "../../include/cap_protocol_api.h"
#include "../util/endian.h"

static app_state_t g_state;

static void print_message_summary(const char *prefix, uint32_t type, const uint8_t *msg, size_t len) {
    (void)msg;
    printf("%s type=%u len=%zu\n", prefix, type, len);
}

static void on_protocol_message(uint32_t type, const uint8_t *msg, size_t len, void *ctx) {
    (void)ctx;
    print_message_summary("RX", type, msg, len);

    const uint8_t *header = &msg[8];
    if (type == CAP_MSG_DESTINATION) {
        g_state.target_direction = cap_read_u32_be(&header[0]);
        g_state.target_distance = cap_read_u32_be(&header[4]);
        g_state.has_target = true;
        printf("Updated target: direction=%u distance=%u\n",
               g_state.target_direction,
               g_state.target_distance);
    } else if (type == CAP_MSG_CAP_DIRECTION_REQUEST_START) {
        g_state.stream_enabled = true;
        printf("Cap direction stream requested.\n");
    } else if (type == CAP_MSG_CAP_DIRECTION_REQUEST_STOP) {
        g_state.stream_enabled = false;
        printf("Cap direction stream stopped.\n");
    }
}

static void on_ble_rx(const uint8_t *data, size_t len, void *ctx) {
    (void)ctx;
    cap_protocol_feed_bytes(data, len);
}

static int send_message(uint32_t type, const void *header8) {
    uint8_t buffer[CAP_MAX_MESSAGE_SIZE];
    size_t msg_len = cap_build_message(type, header8, NULL, 0, buffer, sizeof(buffer));
    if (msg_len == 0) {
        fprintf(stderr, "Failed to build message.\n");
        return -1;
    }
    print_message_summary("TX", type, buffer, msg_len);
    return ble_send(buffer, msg_len);
}

static int cmd_connect(const char *adapter, const char *device_name) {
    ble_transport_config_t config = {
        .on_rx = on_ble_rx,
        .ctx = NULL
    };
    return ble_connect(adapter, device_name, config);
}

static int cmd_disconnect(void) {
    return ble_disconnect();
}

static int cmd_send_handshake(void) {
    cap_handshake_header_t header = {
        .protocol_version = CAP_PROTOCOL_VERSION,
        .capabilities = 0
    };
    uint8_t header_buf[CAP_MSG_HEADER_SIZE];
    cap_write_u32_be(&header_buf[0], header.protocol_version);
    cap_write_u32_be(&header_buf[4], header.capabilities);
    return send_message(CAP_MSG_HANDSHAKE, header_buf);
}

static int cmd_send_destination_request(void) {
    cap_reserved_header_t header = {0, 0};
    uint8_t header_buf[CAP_MSG_HEADER_SIZE];
    cap_write_u32_be(&header_buf[0], header.reserved0);
    cap_write_u32_be(&header_buf[4], header.reserved1);
    return send_message(CAP_MSG_DESTINATION_REQUEST, header_buf);
}

static int cmd_send_cap_direction(void) {
    cap_direction_header_t header = {
        .direction = g_state.cap_direction,
        .reserved = 0
    };
    uint8_t header_buf[CAP_MSG_HEADER_SIZE];
    cap_write_u32_be(&header_buf[0], header.direction);
    cap_write_u32_be(&header_buf[4], header.reserved);
    return send_message(CAP_MSG_CAP_DIRECTION, header_buf);
}

static int cmd_start_cap_direction_stream(void) {
    g_state.stream_enabled = true;
    printf("Stream enabled (local).\n");
    return 0;
}

static int cmd_stop_cap_direction_stream(void) {
    g_state.stream_enabled = false;
    printf("Stream disabled (local).\n");
    return 0;
}

static void cmd_set_cap_direction(unsigned int degrees) {
    g_state.cap_direction = degrees % 360u;
    printf("Cap direction set to %u deg.\n", g_state.cap_direction);
}

static void maybe_send_stream_message(void) {
    if (!g_state.stream_enabled) {
        return;
    }

    static time_t last_sent = 0;
    time_t now = time(NULL);
    if (now != last_sent) {
        last_sent = now;
        cmd_send_cap_direction();
    }
}

int main(void) {
    app_state_init(&g_state);

    cap_protocol_config_t config = {
        .on_message = on_protocol_message,
        .ctx = NULL
    };
    cap_protocol_init(config);

    console_ui_ops_t ops = {
        .connect = cmd_connect,
        .disconnect = cmd_disconnect,
        .send_handshake = cmd_send_handshake,
        .send_destination_request = cmd_send_destination_request,
        .send_cap_direction = cmd_send_cap_direction,
        .start_cap_direction_stream = cmd_start_cap_direction_stream,
        .stop_cap_direction_stream = cmd_stop_cap_direction_stream,
        .set_cap_direction = cmd_set_cap_direction,
        .state = &g_state
    };

    console_ui_print_banner();

    char line[256];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        if (console_ui_handle_line(line, ops)) {
            break;
        }
        maybe_send_stream_message();
    }

    return 0;
}
