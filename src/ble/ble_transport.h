#ifndef BLE_TRANSPORT_H
#define BLE_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

typedef void (*ble_rx_cb)(const uint8_t *data, size_t len, void *ctx);

typedef struct {
    ble_rx_cb on_rx;
    void *ctx;
} ble_transport_config_t;

int ble_connect(const char *adapter, const char *device_name, ble_transport_config_t config);
int ble_send(const uint8_t *data, size_t len);
int ble_disconnect(void);

#endif
