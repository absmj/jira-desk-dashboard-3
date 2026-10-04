# Data formats (web app → device)

The web app reads Jira, computes all statistics, and sends two **separate** JSON
documents. The device never talks to Jira and never computes statistics.

Text must be **Unicode NFC** (`str.normalize("NFC")` in JavaScript). Supported
non-ASCII letters: `ə Ə ı İ ö Ö ü Ü ç Ç ş Ş ğ Ğ`. Anything else is shown as `?`.

Status of this document: both files are parsed on the device from internal flash
(LittleFS, `/device/*.json`, built from the `data/` folder). `sprint.json` matches
`src/app/SprintData.h`; `alerts.json` matches `src/app/AlertRules.h`. Alerts play
through `IAudio`: a buzzer in the simulator (dev), the DFPlayer on real hardware
(Phase 6, MP3 files on the DFPlayer's own SD card).

## /device/sprint.json

```json
{
  "v": 1,
  "sprint": {
    "name": "SPRINT 24",
    "start": "2026-10-01",
    "end": "2026-10-18",
    "done": 26,
    "total": 40
  },
  "stats": {
    "topAssignee": { "name": "Əli Məmmədov", "count": 5 },
    "byStatus": { "todo": 6, "inProgress": 8, "done": 26 },
    "overdue": 2,
    "blocked": 1,
    "pace": "behind"
  },
  "tasks": [
    { "k": "PRJ-101", "t": "Giriş ekranı", "s": "IP", "a": "Əli Məmmədov" }
  ]
}
```

| Field | Rule |
|---|---|
| `sprint.name` | up to 23 bytes; shown truncated to 13 characters |
| `start`, `end` | `YYYY-MM-DD`, valid calendar date, years 2000–2099 |
| `done`, `total` | integers; progress % is derived on the device |
| `stats.topAssignee` | person with the most **open** tasks; `name` up to 23 bytes |
| `stats.pace` | `"ahead"`, `"onTrack"` or `"behind"` (actual vs ideal burndown) |
| `tasks` | at most **24**; send only what is worth listing, open tasks first |
| `tasks[].k` | Jira key, up to 11 bytes; the project prefix is dropped on screen |
| `tasks[].t` | title, up to 39 bytes (about 19 Azerbaijani letters) |
| `tasks[].s` | `"TD"` to do, `"IP"` in progress, `"BL"` blocked, `"DN"` done |

`DN` tasks are not listed on screen. Blocked tasks are prefixed with `!`.

## /device/alerts.json

Rules evaluated when new data arrives and once a minute. They are separate from
time-based notifications because they depend on **data**, not on the clock.

```json
{
  "v": 1,
  "quietHours": { "from": "19:00", "to": "08:30" },
  "rules": [
    { "id": "last_day",    "when": { "daysLeft": { "lte": 1 } },  "audio": "0002.mp3", "repeat": "daily", "text": "Sprint son gün!" },
    { "id": "half_done",   "when": { "progress": { "gte": 50 } },  "audio": "0003.mp3", "repeat": "once" },
    { "id": "has_overdue", "when": { "overdue":  { "gte": 1 } },   "audio": "0004.mp3", "repeat": "cooldown", "cooldownMin": 240 },
    { "id": "standup",     "when": { "time": "09:55", "days": ["mon","tue","wed","thu","fri"] }, "audio": "0005.mp3", "repeat": "daily" }
  ]
}
```

- Conditions: `daysLeft`, `progress`, `overdue`, `blocked` (each `{"gte": n}` and/or
  `{"lte": n}`), `time` (`"HH:MM"`, matches only during that minute) and `days`
  (`["mon",...,"sun"]`). All present conditions must hold. A rule with no condition is rejected.
- Optional: `enabled` (default true), `text` (shown on screen), `audio` (`"NNNN.mp3"`, 4 digits).
- `repeat`: `once`, `daily` (default) or `cooldown` (needs `cooldownMin`, 1..1440).
- Invalid rules are skipped and counted; a bad version or missing `rules` rejects the file.
- Policies: one alert per poll; nothing fires in quiet hours (and the rule is not consumed);
  a `time` rule missed while the device is off or busy is skipped, not replayed; fired-state is
  RAM-only for now, so a reboot can repeat `once`/`daily` rules whose condition still holds.
- When a rule fires, the device calls `Pager::showAlert(title, text)` (already
  implemented) and plays the MP3. Nothing plays during `quietHours`.

## Display limits (84 × 48 px)

| Item | Limit |
|---|---|
| Characters per line | 13 (text beyond this is cut and ends with `.`) |
| Content lines per page | 4, under a one-line header |
| Task rows per page | 4 |
| Pages in rotation | 16 maximum |
| Page dwell time | dashboard 6 s, sprint 4 s, stats 4.5 s, attention 4 s, tasks 4.5 s |

## Glyph limitations

The Azerbaijani letters are drawn on a 5 × 8 pixel grid. Capitals with an accent
above (`Ö İ Ğ`) cannot keep full capital height and are drawn slightly smaller;
`ğ Ğ` use a flat bar instead of a curved breve. Check them on the real Nokia
5110 and adjust the table in `src/display/AzText.cpp` if needed.

## MP3 files and the dev simulation

`audio: "0002.mp3"` in `alerts.json` means track 2 -> file `/MP3/0002.mp3`.

- **Prod:** the files are on the DFPlayer's own FAT32 SD card under `/MP3/` (not in this repo's flash).
- **Dev (Wokwi):** `data/MP3/NNNN.mp3` is packed into LittleFS together with the JSONs, and
  `SimMp3Audio` treats LittleFS as the card. It checks that the file exists (missing -> `[audio] track N not found`),
  derives the play duration from the file size (assumes 64 kbps), and queues a "finished" event.
  The sound is the buzzer melody of the same track number. The DFPlayer UART protocol is not simulated.
- Regenerate the test files: `python3 tools/make_test_mp3.py` (needs `pip install lameenc`).
  Then `pio run -e sim -t buildfs` and rebuild, so the new files are inside `firmware.merged.bin`.
