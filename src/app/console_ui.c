#include "console_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void console_ui_print_banner(void) {
    printf("Cap BLE Test Application (Linux)\n");
    printf("Type 'help' for commands.\n");
}

void console_ui_print_help(void) {
    printf("Available commands:\n");
    printf("  help\n");
    printf("  connect <adapter> <device_name>\n");
    printf("  disconnect\n");
    printf("  set-cap-direction <degrees>\n");
    printf("  send-handshake\n");
    printf("  send-destination-request\n");
    printf("  send-cap-direction\n");
    printf("  start-cap-direction-stream\n");
    printf("  stop-cap-direction-stream\n");
    printf("  show-state\n");
    printf("  quit\n");
}

static void console_ui_show_state(const app_state_t *state) {
    if (!state) {
        return;
    }

    printf("Current state:\n");
    if (state->has_target) {
        printf("  Target: direction=%u deg, distance=%u m\n",
               state->target_direction,
               state->target_distance);
    } else {
        printf("  Target: <none>\n");
    }
    printf("  Cap direction: %u deg\n", state->cap_direction);
    printf("  Stream enabled: %s\n", state->stream_enabled ? "yes" : "no");
}

int console_ui_handle_line(const char *line, console_ui_ops_t ops) {
    if (!line) {
        return 0;
    }

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s", line);

    char *cmd = strtok(buffer, " \t\r\n");
    if (!cmd) {
        return 0;
    }

    if (strcmp(cmd, "help") == 0) {
        console_ui_print_help();
        return 0;
    }

    if (strcmp(cmd, "quit") == 0) {
        return 1;
    }

    if (strcmp(cmd, "connect") == 0) {
        char *adapter = strtok(NULL, " \t\r\n");
        char *device = strtok(NULL, " \t\r\n");
        if (!adapter || !device) {
            printf("Usage: connect <adapter> <device_name>\n");
            return 0;
        }
        if (ops.connect) {
            ops.connect(adapter, device);
        }
        return 0;
    }

    if (strcmp(cmd, "disconnect") == 0) {
        if (ops.disconnect) {
            ops.disconnect();
        }
        return 0;
    }

    if (strcmp(cmd, "set-cap-direction") == 0) {
        char *value = strtok(NULL, " \t\r\n");
        if (!value) {
            printf("Usage: set-cap-direction <degrees>\n");
            return 0;
        }
        unsigned int degrees = (unsigned int)strtoul(value, NULL, 10);
        if (ops.set_cap_direction) {
            ops.set_cap_direction(degrees % 360u);
        }
        return 0;
    }

    if (strcmp(cmd, "send-handshake") == 0) {
        if (ops.send_handshake) {
            ops.send_handshake();
        }
        return 0;
    }

    if (strcmp(cmd, "send-destination-request") == 0) {
        if (ops.send_destination_request) {
            ops.send_destination_request();
        }
        return 0;
    }

    if (strcmp(cmd, "send-cap-direction") == 0) {
        if (ops.send_cap_direction) {
            ops.send_cap_direction();
        }
        return 0;
    }

    if (strcmp(cmd, "start-cap-direction-stream") == 0) {
        if (ops.start_cap_direction_stream) {
            ops.start_cap_direction_stream();
        }
        return 0;
    }

    if (strcmp(cmd, "stop-cap-direction-stream") == 0) {
        if (ops.stop_cap_direction_stream) {
            ops.stop_cap_direction_stream();
        }
        return 0;
    }

    if (strcmp(cmd, "show-state") == 0) {
        console_ui_show_state(ops.state);
        return 0;
    }

    printf("Unknown command. Type 'help'.\n");
    return 0;
}
