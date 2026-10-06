# BLE file transfer (web app -> device)

The web app (`web/`) sends the two JSON files of `docs/data-format.md` over one custom GATT service.
Status: **web side implemented and tested against a mock device. The firmware side does not exist yet.**

| | UUID | Properties |
|---|---|---|
| Service | `4a5b0001-7d8e-4f10-9a2b-3c4d5e6f7081` | advertised, so the browser can filter on it |
| RX (web -> device) | `4a5b0002-7d8e-4f10-9a2b-3c4d5e6f7081` | Write (with response) |
| Status (device -> web) | `4a5b0003-7d8e-4f10-9a2b-3c4d5e6f7081` | Notify |

All multi-byte numbers are little-endian. Every write is at most 20 bytes (the default ATT payload at MTU 23),
so no MTU negotiation is needed.

## Frames (RX)

| Byte 0 `type` | Byte 1 | Rest |
|---|---|---|
| `0x01` BEGIN | `seq` (0) | `fileId` u8 (1 = sprint.json, 2 = alerts.json), `length` u16, `crc32` u32 |
| `0x02` DATA | `seq` (+1 each frame, wraps at 256) | up to 18 bytes of the file |
| `0x03` END | `seq` | nothing |
| `0x04` ABORT | `seq` | nothing (reserved, the web app does not send it yet) |

CRC32 is the standard IEEE one (polynomial `0xEDB88320`, init `0xFFFFFFFF`, final XOR `0xFFFFFFFF`,
the same as zlib); check value for `"123456789"` is `0xCBF43926`.

## Reply (Status notification)

Two bytes `[code, fileId]`, sent after END (or immediately on a protocol error):

| code | meaning |
|---|---|
| 0 | OK: file stored and activated |
| 1 | length differs from BEGIN |
| 2 | CRC mismatch |
| 3 | JSON rejected by `fillSprint` / `fillAlerts` |
| 4 | storage error |
| 5 | frame out of sequence |
| 6 | file larger than 8192 bytes, or busy |

## What the firmware must do (not written yet)

1. Receive into a RAM buffer (max 8192 bytes), never straight into the live file.
2. On END check length and CRC, then parse into temporary structs with the existing loaders
   (`fillSprint`, `fillAlerts`). Only on success write `/device/<file>.json` to LittleFS
   (write a temp file, then rename) and swap the live data. A bad upload must never destroy good data.
3. Notify `[code, fileId]`. Abort a half-received file after about 5 seconds of silence.
4. After a successful sprint upload: `pager.rebuild(...)`; after alerts: reset the alert engine state.

Verify on real hardware: Web Bluetooth write speed (about 85 frames for a 1.5 KB file) and that the Wokwi
simulator cannot test BLE at all, so this part needs the physical ESP32-C3.
