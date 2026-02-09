# Cap BLE Test Application (Linux)

This repository contains a Linux-based BLE GATT test application that simulates the "cap"
device. It implements the BLE protocol described in `docs/cap_phone_protocol.md` and exposes a
console UI for testing a mobile app without real hardware.

## Build requirements

- Fedora Linux
- `bluez-libs-devel` (provides BlueZ headers and libbluetooth)
- GCC or Clang

## Build

```sh
make
```

## Run

```sh
./cap_ble_test
```

## Console commands

```
help
connect <adapter> <device_name>
disconnect
set-cap-direction <degrees>
send-handshake
send-destination-request
send-cap-direction
start-cap-direction-stream
stop-cap-direction-stream
show-state
quit
```

## Notes

- BLE transport is scaffolded using BlueZ but not fully implemented yet.
- Protocol encoding/decoding and message framing are implemented in `src/protocol` and can be
  reused in embedded firmware.
