# Web app

Static page, no build step. From the repository root:

    python3 -m http.server 8000      # then open http://localhost:8000/web/index.html in Chrome or Edge

(`localhost` counts as a secure context, so Web Bluetooth is allowed; elsewhere you need https.)

- `convert.js`  Jira JSON -> `sprint.json` (statistics, 24-task limit, UTF-8 byte limits, NFC). Pure, tested in Node.
- `protocol.js` BLE framing and CRC32. Pure, tested in Node. Spec: `docs/ble-protocol.md`.
- `app.js`      UI and Web Bluetooth.

Tests: `node web/test/test.js` (41 checks). `web/test/e2e.js` drives the page in headless Chromium against a mock device
(`npm i playwright-core`).

Jira: the page cannot call Jira directly (CORS), so you paste the JSON from
`/rest/agile/1.0/sprint/<id>/issue?maxResults=100` while logged in. No credentials touch this page.
