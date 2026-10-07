# Battery power plan (design only, nothing implemented or measured)

Every current below is a **typical datasheet-class figure or my estimate**, not a measurement of this build.
Measure the real board with a USB power meter or a µA-capable multimeter before trusting any life estimate.

## Where the current goes

| Part | Always on | Notes |
|---|---|---|
| ESP32-C3, BLE advertising, CPU awake | ~20-30 mA | far too much for a battery wall clock |
| ESP32-C3 light sleep | ~0.1-0.3 mA | RAM and GPIO state kept, wakes on timer/GPIO |
| ESP32-C3 deep sleep | ~0.005-0.02 mA | reboots on wake (state must come from RTC memory / flash / DS3231) |
| DFPlayer Mini idle | **~15-20 mA** | the biggest silent drain; plays at 100+ mA |
| Nokia 5110 (PCD8544) | ~0.2-0.3 mA | keeps showing the image with the MCU asleep |
| Nokia backlight | ~5-20 mA | keep off, or switch it |
| DS3231 module | ~0.1-0.2 mA | module LEDs and charging resistor can add more |
| SuperMini board regulator + power LED | unknown, **verify** | cheap clones often have a lit LED (~1 mA+) and a leaky regulator |

## Plan

1. **DFPlayer is powered only while an alert plays.** High-side P-MOSFET (or a load switch) on a spare GPIO feeds the
   DFPlayer VCC. Sequence: switch on, wait ~1 s for it to boot, play, wait for its BUSY pin (or the track length), switch
   off. Never leave its TX/RX connected to a powered MCU pin while it is off without a series resistor (back-powering).
2. **Backlight off by default.** Optional: a button turns it on for 10 s.
3. **BLE only on demand.** A button wakes the device into a 60 s "sync" window (advertising, `BleManager`); then BLE is
   shut down (`NimBLEDevice::deinit`). No permanent advertising.
4. **MCU sleeps between page changes.** The PCD8544 holds the image, so: draw, light-sleep until the next page flip
   (timer), draw, repeat.
5. **Alerts come from the DS3231.** Program its alarm for the next rule time; the INT/SQW pin wakes the MCU.
   The 250 ms polling loop in `main.cpp` stops being needed on battery: compute "next interesting minute"
   from the rules instead.
6. **Time source:** DS3231 on battery backup (its own coin cell or the main cell). `SimClock` stays dev-only.

## Proposed pin additions (free on the plan in `include/pins.h`)

| GPIO | Use | Why this pin |
|---|---|---|
| 4 | DS3231 INT/SQW | deep/light-sleep wake capable (C3: GPIO0-5), no longer needed for SD SCK |
| 5 | Wake / sync button | same |
| 6 | DFPlayer power switch | plain output |
| 7 | DFPlayer BUSY | input |

**Buttons added to the enclosure (POWER, VOL-, VOL+) change this table.** The SuperMini breaks out 13 GPIOs (0-10, 20, 21)
and all were already taken. To free two inputs for the volume buttons: drop the DFPlayer BUSY pin and the DFPlayer TX -> ESP RX line.
Final plan: 4 DS3231 INT, 5 POWER/wake, 6 DFPlayer power switch, 7 VOL+, 21 VOL-, 20 ESP TX -> DFPlayer RX (via 1 kohm).
Cost: no feedback from the player, so the end of a track is a timer (track length stored in `alerts.json` later), and no
"is it busy" check before switching the DFPlayer off. GPIO21 is the default UART0 TX and prints the ROM boot log;
a pressed VOL- at boot would fight it, harmless but worth knowing. Alternative if this hurts: one ADC pin with a resistor ladder for the two volume keys.
The POWER button is a tact switch: it wakes/sleeps the firmware, it does not disconnect the battery (that needs a latching or slide switch).

(SD is on the DFPlayer itself, the JSON files are in internal flash, so the old SD SPI pins are free.)

## Rough budget (formula, then an example with assumed numbers)

    I_avg = I_sleep_total + (I_awake - I_sleep_total) * duty
    life  = capacity / I_avg * derating      (derating ~0.6-0.8: self-discharge, cut-off voltage, cold, ageing)

Assumed: light sleep 0.2 mA, display 0.3 mA, DS3231 0.15 mA, regulator 0.1 mA -> **0.75 mA** floor.
Page flip every 5 s, 30 ms awake at 25 mA -> +0.15 mA. DFPlayer: 6 alerts/day, 20 s each at 120 mA -> +0.17 mA.
BLE sync 1 min/day at 25 mA -> +0.02 mA. Total **about 1.1 mA**.

| Cell | Ideal | With derating 0.7 |
|---|---|---|
| 1000 mAh | ~38 days | ~26 days |
| 2000 mAh | ~76 days | ~53 days |
| 3000 mAh | ~114 days | ~80 days |

Without the plan (CPU awake, BLE advertising, DFPlayer powered): about 45-50 mA, i.e. **a 2000 mAh cell lasts under 2 days**.
Capacity that fits the 78 mm enclosure was not decided yet.

## Order of work

1. Measure the SuperMini + Nokia + DS3231 alone (no DFPlayer) in light sleep. This decides whether the board
   regulator/LED must be modified or replaced (e.g. a low-quiescent LDO on a bare module).
2. `PowerPolicy` (pure C++, host-tested): given time, next page flip, next alert and the sync flag, return the next
   wake time and what must be on. Easy to test; no hardware needed.
3. DS3231 driver + alarm (Phase 3).
4. DFPlayer power gate + UART driver (`DfPlayerAudio`, Phase 6).
5. Button + sync window with `BleManager::end()`.

Step 2 can be done now without hardware; the rest needs the parts.
