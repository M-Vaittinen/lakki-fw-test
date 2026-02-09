#include "ble_bluez_nus.h"

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

static ble_transport_config_t g_config;
static int g_att_fd = -1;
static pthread_t g_rx_thread;
static int g_rx_thread_started = 0;
static volatile int g_rx_thread_stop = 0;
static uint16_t g_rx_handle = 0;
static uint16_t g_tx_handle = 0;
static uint16_t g_tx_ccc_handle = 0;
static uint16_t g_mtu = 23;

static const uint8_t NUS_SERVICE_UUID[16] = {
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0xf3, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e
};
static const uint8_t NUS_RX_UUID[16] = {
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0xf3, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e
};
static const uint8_t NUS_TX_UUID[16] = {
    0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
    0xf3, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e
};

static const uint16_t ATT_CID = 0x0004;
static const uint8_t ATT_OP_MTU_REQ = 0x02;
static const uint8_t ATT_OP_MTU_RESP = 0x03;
static const uint8_t ATT_OP_FIND_INFO_REQ = 0x04;
static const uint8_t ATT_OP_FIND_INFO_RESP = 0x05;
static const uint8_t ATT_OP_READ_BY_TYPE_REQ = 0x08;
static const uint8_t ATT_OP_READ_BY_TYPE_RESP = 0x09;
static const uint8_t ATT_OP_READ_BY_GROUP_REQ = 0x10;
static const uint8_t ATT_OP_READ_BY_GROUP_RESP = 0x11;
static const uint8_t ATT_OP_WRITE_REQ = 0x12;
static const uint8_t ATT_OP_WRITE_RESP = 0x13;
static const uint8_t ATT_OP_HANDLE_NOTIFY = 0x1b;

static uint16_t read_le16(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static void write_le16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)(value & 0xff);
    data[1] = (uint8_t)((value >> 8) & 0xff);
}

static bool eir_extract_name(const uint8_t *eir, size_t eir_len, char *out, size_t out_len) {
    size_t offset = 0;
    while (offset < eir_len) {
        uint8_t field_len = eir[offset];
        if (field_len == 0) {
            break;
        }
        if (offset + field_len >= eir_len) {
            break;
        }
        uint8_t field_type = eir[offset + 1];
        if (field_type == 0x08 || field_type == 0x09) {
            size_t name_len = field_len - 1;
            if (name_len >= out_len) {
                name_len = out_len - 1;
            }
            memcpy(out, &eir[offset + 2], name_len);
            out[name_len] = '\0';
            return true;
        }
        offset += field_len + 1;
    }
    return false;
}

static bool parse_bdaddr(const char *device_name, bdaddr_t *out) {
    if (!device_name) {
        return false;
    }
    if (strlen(device_name) != 17 || strchr(device_name, ':') == NULL) {
        return false;
    }
    str2ba(device_name, out);
    return true;
}

static int find_device_address(int dev_id, const char *device_name, bdaddr_t *out_addr, uint8_t *out_addr_type) {
    if (parse_bdaddr(device_name, out_addr)) {
        *out_addr_type = BDADDR_LE_PUBLIC;
        return 0;
    }

    int sock = hci_open_dev(dev_id);
    if (sock < 0) {
        perror("hci_open_dev");
        return -1;
    }

    struct hci_filter old_filter;
    socklen_t old_filter_len = sizeof(old_filter);
    if (getsockopt(sock, SOL_HCI, HCI_FILTER, &old_filter, &old_filter_len) < 0) {
        perror("getsockopt");
        close(sock);
        return -1;
    }

    struct hci_filter new_filter;
    hci_filter_clear(&new_filter);
    hci_filter_set_ptype(HCI_EVENT_PKT, &new_filter);
    hci_filter_set_event(EVT_LE_META_EVENT, &new_filter);
    if (setsockopt(sock, SOL_HCI, HCI_FILTER, &new_filter, sizeof(new_filter)) < 0) {
        perror("setsockopt");
        close(sock);
        return -1;
    }

    if (hci_le_set_scan_parameters(sock, 0x01, htobs(0x0010), htobs(0x0010), LE_PUBLIC_ADDRESS, 0x00, 1000) < 0) {
        perror("hci_le_set_scan_parameters");
        setsockopt(sock, SOL_HCI, HCI_FILTER, &old_filter, sizeof(old_filter));
        close(sock);
        return -1;
    }

    if (hci_le_set_scan_enable(sock, 0x01, 0x00, 1000) < 0) {
        perror("hci_le_set_scan_enable");
        setsockopt(sock, SOL_HCI, HCI_FILTER, &old_filter, sizeof(old_filter));
        close(sock);
        return -1;
    }

    int found = -1;
    time_t start = time(NULL);
    while (time(NULL) - start < 10) {
        uint8_t buf[HCI_MAX_EVENT_SIZE];
        int len = read(sock, buf, sizeof(buf));
        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("hci read");
            break;
        }
        if (len < (1 + HCI_EVENT_HDR_SIZE)) {
            continue;
        }
        evt_le_meta_event *meta = (evt_le_meta_event *)(buf + (1 + HCI_EVENT_HDR_SIZE));
        if (meta->subevent != EVT_LE_ADVERTISING_REPORT) {
            continue;
        }
        uint8_t reports = meta->data[0];
        uint8_t *ptr = meta->data + 1;
        for (uint8_t i = 0; i < reports; i++) {
            le_advertising_info *info = (le_advertising_info *)ptr;
            char name[64];
            if (eir_extract_name(info->data, info->length, name, sizeof(name))) {
                if (strcmp(name, device_name) == 0) {
                    *out_addr = info->bdaddr;
                    *out_addr_type = info->bdaddr_type;
                    found = 0;
                    break;
                }
            }
            ptr += sizeof(le_advertising_info) + info->length;
        }
        if (found == 0) {
            break;
        }
    }

    hci_le_set_scan_enable(sock, 0x00, 0x00, 1000);
    setsockopt(sock, SOL_HCI, HCI_FILTER, &old_filter, sizeof(old_filter));
    close(sock);
    if (found != 0) {
        fprintf(stderr, "BLE device '%s' not found during scan.\n", device_name);
    }
    return found;
}

static int att_send(int fd, const uint8_t *data, size_t len) {
    ssize_t written = write(fd, data, len);
    if (written < 0 || (size_t)written != len) {
        perror("att write");
        return -1;
    }
    return 0;
}

static int att_recv(int fd, uint8_t *buffer, size_t buffer_len) {
    ssize_t len = read(fd, buffer, buffer_len);
    if (len < 0) {
        perror("att read");
        return -1;
    }
    return (int)len;
}

static int att_exchange_mtu(int fd, uint16_t desired) {
    uint8_t req[3];
    req[0] = ATT_OP_MTU_REQ;
    write_le16(&req[1], desired);
    if (att_send(fd, req, sizeof(req)) < 0) {
        return -1;
    }
    uint8_t resp[64];
    int len = att_recv(fd, resp, sizeof(resp));
    if (len < 3 || resp[0] != ATT_OP_MTU_RESP) {
        fprintf(stderr, "Unexpected MTU response.\n");
        return -1;
    }
    g_mtu = read_le16(&resp[1]);
    return 0;
}

static int att_discover_primary_service(int fd, uint16_t *out_start, uint16_t *out_end) {
    uint8_t req[7];
    req[0] = ATT_OP_READ_BY_GROUP_REQ;
    write_le16(&req[1], 0x0001);
    write_le16(&req[3], 0xffff);
    write_le16(&req[5], 0x2800);
    if (att_send(fd, req, sizeof(req)) < 0) {
        return -1;
    }
    uint8_t resp[512];
    int len = att_recv(fd, resp, sizeof(resp));
    if (len < 2 || resp[0] != ATT_OP_READ_BY_GROUP_RESP) {
        fprintf(stderr, "Unexpected service discovery response.\n");
        return -1;
    }
    uint8_t entry_len = resp[1];
    if (entry_len < 6) {
        fprintf(stderr, "Service discovery response too short.\n");
        return -1;
    }
    for (int offset = 2; offset + entry_len <= len; offset += entry_len) {
        uint16_t start = read_le16(&resp[offset]);
        uint16_t end = read_le16(&resp[offset + 2]);
        const uint8_t *uuid = &resp[offset + 4];
        if (entry_len == 20 && memcmp(uuid, NUS_SERVICE_UUID, 16) == 0) {
            *out_start = start;
            *out_end = end;
            return 0;
        }
    }
    fprintf(stderr, "NUS service not found.\n");
    return -1;
}

static int att_discover_characteristics(int fd, uint16_t start, uint16_t end) {
    uint16_t current = start;
    while (current <= end) {
        uint8_t req[7];
        req[0] = ATT_OP_READ_BY_TYPE_REQ;
        write_le16(&req[1], current);
        write_le16(&req[3], end);
        write_le16(&req[5], 0x2803);
        if (att_send(fd, req, sizeof(req)) < 0) {
            return -1;
        }
        uint8_t resp[512];
        int len = att_recv(fd, resp, sizeof(resp));
        if (len < 2) {
            fprintf(stderr, "Characteristic discovery failed.\n");
            return -1;
        }
        if (resp[0] != ATT_OP_READ_BY_TYPE_RESP) {
            return 0;
        }
        uint8_t entry_len = resp[1];
        if (entry_len < 7) {
            fprintf(stderr, "Characteristic entry too short.\n");
            return -1;
        }
        for (int offset = 2; offset + entry_len <= len; offset += entry_len) {
            uint16_t handle = read_le16(&resp[offset]);
            uint8_t props = resp[offset + 2];
            uint16_t value_handle = read_le16(&resp[offset + 3]);
            const uint8_t *uuid = &resp[offset + 5];
            if (entry_len == 21) {
                if (memcmp(uuid, NUS_RX_UUID, 16) == 0) {
                    g_rx_handle = value_handle;
                } else if (memcmp(uuid, NUS_TX_UUID, 16) == 0) {
                    g_tx_handle = value_handle;
                }
            }
            (void)props;
            current = handle + 1;
        }
        if (resp[0] != ATT_OP_READ_BY_TYPE_RESP) {
            break;
        }
    }
    if (g_rx_handle == 0 || g_tx_handle == 0) {
        fprintf(stderr, "Failed to locate NUS characteristics.\n");
        return -1;
    }
    return 0;
}

static int att_find_ccc_descriptor(int fd, uint16_t start, uint16_t end) {
    uint8_t req[7];
    req[0] = ATT_OP_FIND_INFO_REQ;
    write_le16(&req[1], start);
    write_le16(&req[3], end);
    if (att_send(fd, req, sizeof(req)) < 0) {
        return -1;
    }
    uint8_t resp[512];
    int len = att_recv(fd, resp, sizeof(resp));
    if (len < 2 || resp[0] != ATT_OP_FIND_INFO_RESP) {
        fprintf(stderr, "Descriptor discovery failed.\n");
        return -1;
    }
    uint8_t format = resp[1];
    int entry_len = (format == 0x01) ? 4 : 18;
    for (int offset = 2; offset + entry_len <= len; offset += entry_len) {
        uint16_t handle = read_le16(&resp[offset]);
        if (format == 0x01) {
            uint16_t uuid = read_le16(&resp[offset + 2]);
            if (uuid == 0x2902) {
                g_tx_ccc_handle = handle;
                return 0;
            }
        } else {
            const uint8_t *uuid = &resp[offset + 2];
            if (memcmp(uuid, "\x02\x29", 2) == 0) {
                g_tx_ccc_handle = handle;
                return 0;
            }
        }
    }
    fprintf(stderr, "CCC descriptor not found.\n");
    return -1;
}

static int att_write_ccc(int fd, uint16_t handle, uint16_t value) {
    uint8_t req[5];
    req[0] = ATT_OP_WRITE_REQ;
    write_le16(&req[1], handle);
    write_le16(&req[3], value);
    if (att_send(fd, req, sizeof(req)) < 0) {
        return -1;
    }
    uint8_t resp[64];
    int len = att_recv(fd, resp, sizeof(resp));
    if (len < 1 || resp[0] != ATT_OP_WRITE_RESP) {
        fprintf(stderr, "Failed to enable notifications.\n");
        return -1;
    }
    return 0;
}

static void *ble_rx_thread(void *ctx) {
    (void)ctx;
    uint8_t buffer[512];
    while (!g_rx_thread_stop) {
        int len = att_recv(g_att_fd, buffer, sizeof(buffer));
        if (len <= 0) {
            if (g_rx_thread_stop) {
                break;
            }
            continue;
        }
        if (buffer[0] == ATT_OP_HANDLE_NOTIFY && len >= 3) {
            uint16_t handle = read_le16(&buffer[1]);
            if (handle == g_tx_handle && g_config.on_rx) {
                g_config.on_rx(&buffer[3], (size_t)(len - 3), g_config.ctx);
            }
        }
    }
    return NULL;
}

int ble_connect(const char *adapter, const char *device_name, ble_transport_config_t config) {
    if (g_att_fd >= 0) {
        fprintf(stderr, "BLE already connected.\n");
        return -1;
    }

    g_config = config;

    int dev_id = -1;
    if (adapter && adapter[0] != '\0') {
        dev_id = hci_devid(adapter);
    } else {
        dev_id = hci_get_route(NULL);
    }
    if (dev_id < 0) {
        fprintf(stderr, "Failed to find adapter %s.\n", adapter ? adapter : "(default)");
        return -1;
    }

    bdaddr_t addr;
    uint8_t addr_type = BDADDR_LE_PUBLIC;
    if (find_device_address(dev_id, device_name, &addr, &addr_type) < 0) {
        return -1;
    }

    int sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
    if (sock < 0) {
        perror("l2cap socket");
        return -1;
    }

    struct sockaddr_l2 local = {0};
    local.l2_family = AF_BLUETOOTH;
    local.l2_cid = htobs(ATT_CID);
    local.l2_bdaddr_type = BDADDR_LE_PUBLIC;
    bacpy(&local.l2_bdaddr, BDADDR_ANY);
    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("l2cap bind");
        close(sock);
        return -1;
    }

    struct sockaddr_l2 remote = {0};
    remote.l2_family = AF_BLUETOOTH;
    remote.l2_cid = htobs(ATT_CID);
    remote.l2_bdaddr_type = addr_type;
    bacpy(&remote.l2_bdaddr, &addr);
    if (connect(sock, (struct sockaddr *)&remote, sizeof(remote)) < 0) {
        perror("l2cap connect");
        close(sock);
        return -1;
    }

    g_att_fd = sock;
    g_rx_handle = 0;
    g_tx_handle = 0;
    g_tx_ccc_handle = 0;
    g_mtu = 23;

    if (att_exchange_mtu(g_att_fd, 247) < 0) {
        ble_disconnect();
        return -1;
    }

    uint16_t service_start = 0;
    uint16_t service_end = 0;
    if (att_discover_primary_service(g_att_fd, &service_start, &service_end) < 0) {
        ble_disconnect();
        return -1;
    }

    if (att_discover_characteristics(g_att_fd, service_start, service_end) < 0) {
        ble_disconnect();
        return -1;
    }

    if (att_find_ccc_descriptor(g_att_fd, (uint16_t)(g_tx_handle + 1), service_end) < 0) {
        ble_disconnect();
        return -1;
    }

    if (att_write_ccc(g_att_fd, g_tx_ccc_handle, 0x0001) < 0) {
        ble_disconnect();
        return -1;
    }

    g_rx_thread_stop = 0;
    if (pthread_create(&g_rx_thread, NULL, ble_rx_thread, NULL) == 0) {
        g_rx_thread_started = 1;
    } else {
        perror("pthread_create");
        ble_disconnect();
        return -1;
    }

    printf("BLE connected (MTU=%u).\n", g_mtu);
    return 0;
}

int ble_send(const uint8_t *data, size_t len) {
    if (g_att_fd < 0 || g_rx_handle == 0) {
        fprintf(stderr, "BLE not connected.\n");
        return -1;
    }
    if (!data || len == 0) {
        return 0;
    }
    size_t max_payload = (g_mtu > 3) ? (g_mtu - 3) : 20;
    size_t offset = 0;
    while (offset < len) {
        size_t chunk = len - offset;
        if (chunk > max_payload) {
            chunk = max_payload;
        }
        uint8_t buffer[512];
        if (chunk + 3 > sizeof(buffer)) {
            fprintf(stderr, "Chunk too large.\n");
            return -1;
        }
        buffer[0] = ATT_OP_WRITE_REQ;
        write_le16(&buffer[1], g_rx_handle);
        memcpy(&buffer[3], data + offset, chunk);
        if (att_send(g_att_fd, buffer, chunk + 3) < 0) {
            return -1;
        }
        uint8_t resp[64];
        int resp_len = att_recv(g_att_fd, resp, sizeof(resp));
        if (resp_len < 1 || resp[0] != ATT_OP_WRITE_RESP) {
            fprintf(stderr, "Write response missing.\n");
            return -1;
        }
        offset += chunk;
    }
    return 0;
}

int ble_disconnect(void) {
    if (g_att_fd >= 0) {
        g_rx_thread_stop = 1;
        if (g_rx_thread_started) {
            shutdown(g_att_fd, SHUT_RDWR);
            pthread_join(g_rx_thread, NULL);
            g_rx_thread_started = 0;
        }
        close(g_att_fd);
        g_att_fd = -1;
    }
    return 0;
}
