# Cap ↔ Phone BLE Protocol

This document describes the BLE transport and binary message protocol used between the cap
(peripheral) and phone (central). The definitions here align with the shared protocol header used
by the Android app (`external_navigation_protocol.h`).

## BLE transport overview

* **Transport**: Bluetooth Low Energy (BLE) using the Nordic UART Service (NUS) UUIDs.
* **Service UUID**: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
* **RX Characteristic UUID** (phone → cap writes): `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
* **TX Characteristic UUID** (cap → phone notifications): `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`
* **Client Characteristic Configuration (CCC)** descriptor UUID: `00002902-0000-1000-8000-00805F9B34FB`

### GATT roles and data flow

* The **phone** connects as the **GATT client/central**.
* The **cap** advertises the NUS **service** and acts as the **GATT server/peripheral**.
* The phone **writes** outbound messages to the **RX** characteristic.
* The phone **subscribes** to notifications on the **TX** characteristic to receive inbound
  messages from the cap.

### Framing and MTU

* Payloads larger than the current ATT payload size are split into multiple BLE writes/notifications.
* The BLE transport is message-framed at the application layer; frames are concatenated by the
  receiver to reconstruct the full protocol message.

## Binary message protocol

All multi-byte integers are **big-endian**.

### Message layout

```
+----------------+----------------+---------------------------+------------------+
| Message Type   | Total Length   | Message-Specific Header   | TLV Attributes   |
| (4 bytes)      | (4 bytes)      | (variable, usually 8B)    | (0..N)           |
+----------------+----------------+---------------------------+------------------+
```

* **Message Type**: 32-bit integer enumerated below.
* **Total Length**: 32-bit integer representing the full message size in bytes, including
  type, length, header, and attributes.
* **Message-Specific Header**: 8 bytes for all currently defined messages.
* **Attributes**: Optional TLV list for extensibility.

### TLV attribute format

```
+----------------+----------------+------------------+
| Attribute Type | Attribute Len  | Payload          |
| (2 bytes)      | (2 bytes)      | (0..N bytes)     |
+----------------+----------------+------------------+
```

* **Attribute Type**: 16-bit integer.
* **Attribute Length**: 16-bit integer, including type + length + payload.
* **Payload**: Binary data for the attribute.

## Message types and headers

| Type ID | Name                        | Direction         | Header (8 bytes)                             |
|--------:|-----------------------------|-------------------|----------------------------------------------|
| 1       | HANDSHAKE                    | cap ↔ phone       | protocol_version (u32), capabilities (u32)   |
| 2       | DESTINATION                  | phone → cap       | direction (u32), distance_meters (u32)       |
| 3       | MOVEMENT                     | phone → cap       | direction (u32), speed_cm_s (u32)            |
| 4       | DESTINATION_REQUEST          | cap → phone       | reserved0 (u32), reserved1 (u32)             |
| 5       | CAP_DIRECTION                | cap → phone       | direction (u32), reserved (u32)              |
| 6       | CAP_DIRECTION_REQUEST_START  | phone → cap       | reserved0 (u32), reserved1 (u32)             |
| 7       | CAP_DIRECTION_REQUEST_STOP   | phone → cap       | reserved0 (u32), reserved1 (u32)             |

### Header field definitions

* **protocol_version**: Protocol version supported by the sender.
* **capabilities**: Bitmask describing optional features (reserved for future use).
* **direction**: Direction in degrees (0–359), encoded as an unsigned 32-bit integer.
* **distance_meters**: Distance to destination in meters.
* **speed_cm_s**: Speed in centimeters per second.
* **reserved**: Must be zero for now; reserved for future use in `CAP_DIRECTION` messages.
* **reserved0/reserved1**: Must be zero for now; reserved for future use in
  `DESTINATION_REQUEST`, `CAP_DIRECTION_REQUEST_START`, and
  `CAP_DIRECTION_REQUEST_STOP` messages.

## Typical message flow

1. **Handshake**: Cap and phone exchange `HANDSHAKE` messages after BLE connection.
2. **Navigation updates**: Phone periodically sends `DESTINATION` or `MOVEMENT` messages.
3. **Destination request**: Cap sends `DESTINATION_REQUEST` to ask the phone to recompute the
   latest direction and distance based on its current location and stored destination.
4. **Cap direction stream**: Phone sends `CAP_DIRECTION_REQUEST_START` to request that the cap
   periodically publish its current heading via `CAP_DIRECTION`. Phone sends
   `CAP_DIRECTION_REQUEST_STOP` to stop the stream.
