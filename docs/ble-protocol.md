# BLE file transfer (web app -> device)

The web app (`web/`) sends the two JSON files of `docs/data-format.md` over one custom GATT service.
Status:
- **Web side**: implemented, tested against a *mock* device (`web/test/e2e.js`).
- **Firmware logic** (`src/ble/TransferSession`, `UploadApplier`, `LittleFsStore`): implemented and tested on the host.
  `tools/native_tests/gen_frames.js` feeds frames produced by the real `web/protocol.js` into the firmware's
  `TransferSession`, so both sides are checked against each other.
- **Firmware BLE glue** (`src/ble/BleManager.cpp`, NimBLE-Arduino): written against the 2.5.1 headers, **never compiled
  or run by me**. Built only for the `supermini` env (`-DBLE_RECEIVER`). Wokwi cannot simulate BLE, so this part is
  only proven once it runs on the physical ESP32-C3.

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

## What the firmware does

1. `TransferSession` receives into a 8 KB RAM buffer, never into the live file. BEGIN with length 0 or > 8192 is
   refused (code 6). DATA/END must follow the sequence (code 5). END checks length (1) and CRC (2).
2. `applyUpload` parses into a temporary struct with `fillSprint` / `fillAlerts` (code 3 on failure), writes
   `/device/<file>.json.tmp`, checks its size, renames it over the old file (code 4 on failure), and only then
   swaps the live data. A rejected or failed upload changes nothing.
3. While a verified file waits to be applied, further frames get code 6 (busy). Applying happens in `loop()`, not in
   the BLE callback; a mutex guards the session between the two tasks.
4. A half-received file is dropped silently after 5 s without frames (the web app has its own 15 s timeout).
5. After a sprint upload `main.cpp` calls `pager.rebuild`; after alerts, `engine.reset()`.

Known limits: the rename-over-existing behaviour of LittleFS on the ESP32 core is assumed (a fallback exists but has
a short window without the file); one connection at a time is not enforced; no pairing/encryption, anyone in range can
write a file (it still has to be valid JSON of the right shape).

Verify on the physical board: that the device is found by the service UUID filter and shows its name, write speed
(about 85 frames for a 1.5 KB file), and that a power cut during an upload leaves the old data intact.
