#ifndef CONSOLE_UI_H
#define CONSOLE_UI_H

#include <stddef.h>

#include "app_state.h"

typedef struct {
    int (*connect)(const char *adapter, const char *device_name);
    int (*disconnect)(void);
    int (*send_handshake)(void);
    int (*send_destination_request)(void);
    int (*send_cap_direction)(void);
    int (*start_cap_direction_stream)(void);
    int (*stop_cap_direction_stream)(void);
    void (*set_cap_direction)(unsigned int degrees);
    const app_state_t *state;
} console_ui_ops_t;

void console_ui_print_banner(void);
void console_ui_print_help(void);
int console_ui_handle_line(const char *line, console_ui_ops_t ops);

#endif
