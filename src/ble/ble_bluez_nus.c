#include "ble_bluez_nus.h"

#include <stdio.h>

#include <bluetooth/bluetooth.h>

static ble_transport_config_t g_config;

int ble_connect(const char *adapter, const char *device_name, ble_transport_config_t config) {
    (void)adapter;
    (void)device_name;
    g_config = config;
    fprintf(stderr, "BLE connect not implemented yet.\n");
    return -1;
}

int ble_send(const uint8_t *data, size_t len) {
    (void)data;
    (void)len;
    (void)g_config;
    fprintf(stderr, "BLE send not implemented yet.\n");
    return -1;
}

int ble_disconnect(void) {
    fprintf(stderr, "BLE disconnect not implemented yet.\n");
    return 0;
}
