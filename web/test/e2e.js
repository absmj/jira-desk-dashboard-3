// Headless end-to-end test with a mocked Web Bluetooth device.
//   node web/test/e2e.js        (needs: npm i playwright-core; Chromium at $PW_CHROMIUM or /opt/pw-browsers/chromium)
const { chromium } = require('playwright-core');
const http = require('http'), fs = require('fs'), path = require('path');
const assert = require('assert');

const ROOT = path.resolve(__dirname, '../..');
const { createServer } = require('../proxy.js');

// Mock Jira: board 7 -> active sprint 42 -> issues with story points.
const mkIssue = (key, cat, who, sp) => ({ key, fields: { summary: 'Task ' + key, status: { name: cat, statusCategory: { key: cat === 'Done' ? 'done' : 'indeterminate' } }, assignee: who ? { displayName: who } : null, customfield_10016: sp, duedate: null, labels: [], issuelinks: [] } });
const jira = http.createServer((req, res) => {
  const u = new URL(req.url, 'http://x');
  const reply = (o) => { res.writeHead(200, { 'content-type': 'application/json' }); res.end(JSON.stringify(o)); };
  if (u.pathname === '/rest/agile/1.0/board/7/sprint') return reply({ values: [{ id: 42, name: 'SPRINT 24', state: 'active', startDate: '2026-10-01T05:00:00.000Z', endDate: '2026-10-18T13:00:00.000Z' }] });
  if (u.pathname === '/rest/agile/1.0/sprint/42/issue') {
    const all = [mkIssue('A-1', 'Done', 'Əli Məmmədov', 5), mkIssue('A-2', 'In Progress', 'Əli Məmmədov', 8), mkIssue('A-3', 'In Progress', 'Leyla Həsənova', 3), mkIssue('A-4', 'In Progress', null, null)];
    return reply({ startAt: 0, maxResults: 50, total: all.length, issues: all });
  }
  res.writeHead(404); res.end('{}');
});
let srv;
const MOCK = () => {
  // A fake device that speaks docs/ble-protocol.md: validates length + CRC32, then notifies [code, fileId].
  window.__received = {}; window.__corrupt = false; window.__maxWrite = 0;
  let cur = null, listener = null;
  const notify = (code, id) => listener && listener({ target: { value: { buffer: new Uint8Array([code, id]).buffer } } });
  const statusChar = { startNotifications: async () => {}, addEventListener: (t, f) => { listener = f; } };
  const rx = {
    writeValueWithResponse: async (data) => {
      const f = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
      window.__maxWrite = Math.max(window.__maxWrite, f.length);
      if (f[0] === 1) {
        const dv = new DataView(f.buffer, f.byteOffset);
        cur = { id: f[2], len: dv.getUint16(3, true), crc: dv.getUint32(5, true), seq: f[1], data: [] };
      } else if (f[0] === 2) {
        if (!cur || f[1] !== ((cur.seq + 1) & 255)) return notify(5, cur ? cur.id : 0);
        cur.seq = f[1]; cur.data.push(...f.subarray(2));
      } else if (f[0] === 3) {
        const bytes = new Uint8Array(cur.data);
        if (window.__corrupt) bytes[0] ^= 1;
        if (bytes.length !== cur.len) return notify(1, cur.id);
        if (window.DeskProtocol.crc32(bytes) !== cur.crc) return notify(2, cur.id);
        window.__received[cur.id] = new TextDecoder().decode(bytes);
        notify(0, cur.id);
      }
    },
  };
  const service = { getCharacteristic: async (u) => (u.endsWith('0002-7d8e-4f10-9a2b-3c4d5e6f7081') ? rx : statusChar) };
  const gatt = { connected: false, connect: async () => { gatt.connected = true; return { getPrimaryService: async () => service }; }, disconnect: () => { gatt.connected = false; } };
  const dev = { name: 'JiraDesk', id: 'mock', gatt, addEventListener: () => {} };
  Object.defineProperty(navigator, 'bluetooth', { value: { requestDevice: async () => dev }, configurable: true });
};

(async () => {
  await new Promise((r) => jira.listen(0, '127.0.0.1', r));
  srv = createServer({ JIRA_SITE: 'http://127.0.0.1:' + jira.address().port, JIRA_EMAIL: 'a@b.c', JIRA_TOKEN: 't' });
  await new Promise((r) => srv.listen(8790, '127.0.0.1', r));
  const b = await chromium.launch({ executablePath: process.env.PW_CHROMIUM || '/opt/pw-browsers/chromium' });
  const p = await b.newPage({ viewport: { width: 1100, height: 1400 } });
  const errs = [];
  p.on('pageerror', (e) => errs.push(e.message));
  p.on('console', (m) => { if (m.type() === 'error' && !/ERR_|Failed to load resource/.test(m.text())) errs.push(m.text()); });
  await p.route(/fonts\.(googleapis|gstatic)\.com/, (r) => r.abort());
  await p.addInitScript(MOCK);
  await p.goto('http://localhost:8790/web/index.html');
  await p.waitForFunction(() => window.__appReady);

  assert(await p.isVisible('#auto'), 'auto block visible when the proxy is configured');
  assert(await p.isDisabled('#send-sprint'), 'send is disabled without data');

  // automatic mode: board id -> active sprint -> issues, estimate-weighted
  await p.fill('#f-board', 'abc'); await p.click('#fetch');
  assert((await p.textContent('#auto-state')).includes('rəqəm'), 'non-numeric board rejected');
  await p.fill('#f-board', '7'); await p.click('#fetch');
  await p.waitForFunction(() => document.getElementById('auto-state').textContent.includes('4 task'));
  const chips = await p.textContent('#chips');
  assert(chips.includes('SPRINT 24') && chips.includes('estimate-ə görə') && chips.includes('5/16'), 'estimate chips: ' + chips);
  assert((await p.textContent('#problems')).includes('estimate'), 'unestimated open task is flagged');
  await p.click('#sample');
  assert((await p.textContent('#chips')).includes('SPRINT 24'));
  assert((await p.locator('#tasks tbody tr').count()) === 7, 'seven open tasks listed');
  assert(await p.isDisabled('#send-sprint'), 'send is disabled until connected');

  await p.click('#connect');
  await p.waitForFunction(() => !document.getElementById('send-sprint').disabled);
  await p.click('#send-sprint');
  await p.waitForFunction(() => document.getElementById('log').textContent.includes('cihaz qəbul etdi'));
  const got = JSON.parse(await p.evaluate(() => window.__received[1]));
  assert.strictEqual(got.sprint.name, 'SPRINT 24');
  assert.strictEqual(got.tasks.length, 7);
  assert(got.stats.topAssignee.name === 'Əli Məmmədov');
  assert((await p.evaluate(() => window.__maxWrite)) <= 20, 'writes fit 20 bytes');
  assert.strictEqual(await p.inputValue('#alerts') !== '', true, 'alerts.json loaded from ../data/device');

  await p.click('#send-alerts');
  await p.waitForFunction(() => window.__received[2]);
  assert(JSON.parse(await p.evaluate(() => window.__received[2])).rules.length >= 1);

  await p.evaluate(() => { window.__corrupt = true; });       // device sees a bad CRC
  await p.click('#send-sprint');
  await p.waitForFunction(() => document.getElementById('log').textContent.includes('CRC'));

  await p.fill('#jira', '{"issues": 5');                       // broken JSON
  assert((await p.textContent('#problems')).includes('JSON oxunmadı'));
  assert(await p.isDisabled('#send-sprint'));

  assert.deepStrictEqual(errs, [], 'no page errors: ' + errs.join('|'));
  console.log('web e2e: OK');
  await b.close(); srv.close(); jira.close();
})().catch((e) => { console.error(e); process.exit(1); });
