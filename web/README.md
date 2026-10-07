# Web app

No build step. From the repository root:

**Automatic mode (board ID -> active sprint -> tasks)**

    JIRA_SITE=https://SIZIN.atlassian.net JIRA_EMAIL=siz@corp.az JIRA_TOKEN=... node web/proxy.js
    # open http://localhost:8000/web/index.html in Chrome or Edge, enter the Board ID, press the button

The proxy (`proxy.js`, Node only, no dependencies) listens on 127.0.0.1, is GET-only, reads the token from the
environment (never from the page, never logged), finds the active sprint of the board
(`/rest/agile/1.0/board/{id}/sprint?state=active`) and pages through its issues. Without the three variables it still
serves the page, and only manual pasting works.

**Manual mode (no proxy):** `python3 -m http.server 8000`, then paste the JSON from
`/rest/agile/1.0/sprint/<id>/issue?maxResults=100`. Pasting the sprint *list* is detected and the page tells you the
sprint id and the next URL.

`localhost` counts as a secure context, so Web Bluetooth works; elsewhere you need https.

## Checks the page makes

- Estimates: story points (`customfield_10016`) or original estimate (`timeoriginalestimate`, hours), auto-detected or
  chosen in the "Estimate sahəsi" box. Pace is estimate-weighted when at least half of the tasks are estimated,
  otherwise by task count (the chip says which).
- Warnings: missing estimate field, partial coverage, open tasks without estimate or assignee, due date after sprint
  end, ended sprint with open tasks, sprint not started, more than one active sprint, truncated list.

## Files

- `convert.js`  Jira JSON -> `sprint.json` (statistics, limits, UTF-8 byte limits, NFC). Pure, tested in Node.
- `protocol.js` BLE framing and CRC32. Pure, tested in Node. Spec: `docs/ble-protocol.md`.
- `proxy.js`    local Jira proxy + static server.
- `app.js`      UI and Web Bluetooth.

## Tests

    node web/test/test.js          # converter + protocol (56 checks)
    node web/test/proxy.test.js    # proxy against a mock Jira (28 checks)
    node web/test/e2e.js           # headless Chromium, real proxy, mock Jira, MOCK Bluetooth device (npm i playwright-core)

The e2e Bluetooth device is a mock written from `docs/ble-protocol.md`. It is not the firmware and not hardware.
