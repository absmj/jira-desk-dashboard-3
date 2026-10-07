# BLE bring-up on the real ESP32-C3 SuperMini

Nothing here has been run yet: this is the checklist for the first time. Wokwi cannot simulate BLE.

## 0. Static check already done
`BleManager.cpp` calls were compared with the NimBLE-Arduino 2.5.1 headers (`createCharacteristic(const char*, uint32_t)`,
`setCallbacks`, `onWrite(NimBLECharacteristic*, NimBLEConnInfo&)`, `notify(const uint8_t*, size_t)`,
`advertiseOnDisconnect`, `enableScanResponse` before `setName` so the name goes into the scan response).
That is reading, not compiling.

## 1. Build and flash
    pio run -e supermini
    pio run -e supermini -t uploadfs      # data/ -> LittleFS
    pio run -e supermini -t upload
    pio device monitor
Expected in the log: `[fs]`/`[data]` lines as before, then `[ble] advertising as 'JiraDesk'`.
If the build fails, send the first error line: the likely spots are the NimBLE calls.

## 2. Check advertising without the web app
Use nRF Connect (phone): the device `JiraDesk` should appear, with service `4a5b0001-...`.
If it is listed but without a name, the name did not fit the scan response: tell me what nRF Connect shows.

## 3. Send from the web app
    node web/proxy.js            # or: python3 -m http.server 8000
Open `http://localhost:8000/web/index.html` in Chrome/Edge, "Nümunə data" -> "Cihaza qoşul" -> "sprint.json göndər".
Expected serial: `[ble] status code=0 file=1`, `[ble] new sprint.json active`, and the display shows the new sprint.

## 4. Failure cases to try
| Action | Expected |
|---|---|
| Send, then reboot the board | new data is still shown (it was stored in LittleFS) |
| Paste broken JSON in the page | the page blocks it; to test the device, send a valid-JSON file with `"v": 2` through nRF Connect: code 3, old data stays |
| Disconnect (move away) in the middle of a send, then send again | the half file is dropped after 5 s; the next send succeeds |
| Send alerts.json | `new alerts.json active`; fired-once rules can fire again |
| Pull power during a send | after reboot the old data loads |

## 5. Measure
Time a 1.5 KB sprint.json (about 85 write-with-response frames) and note it. If it is slower than about 10 s, the next
step is write-without-response with flow control, or a larger MTU.
