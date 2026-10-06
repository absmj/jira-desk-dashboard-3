// Headless end-to-end test with a mocked Web Bluetooth device.
//   node web/test/e2e.js        (needs: npm i playwright-core; Chromium at $PW_CHROMIUM or /opt/pw-browsers/chromium)
const { chromium } = require('playwright-core');
const http = require('http'), fs = require('fs'), path = require('path');
const assert = require('assert');

const ROOT = path.resolve(__dirname, '../..');
const types = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript', '.json': 'application/json' };
const srv = http.createServer((req, res) => {
  const p = path.join(ROOT, decodeURIComponent(req.url.split('?')[0]));
  if (!p.startsWith(ROOT) || !fs.existsSync(p) || fs.statSync(p).isDirectory()) { res.writeHead(404); return res.end(); }
  res.writeHead(200, { 'content-type': types[path.extname(p)] || 'application/octet-stream' });
  fs.createReadStream(p).pipe(res);
});

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
  await new Promise((r) => srv.listen(8790, r));
  const b = await chromium.launch({ executablePath: process.env.PW_CHROMIUM || '/opt/pw-browsers/chromium' });
  const p = await b.newPage({ viewport: { width: 1100, height: 1400 } });
  const errs = [];
  p.on('pageerror', (e) => errs.push(e.message));
  p.on('console', (m) => { if (m.type() === 'error' && !/ERR_|Failed to load resource/.test(m.text())) errs.push(m.text()); });
  await p.route(/fonts\.(googleapis|gstatic)\.com/, (r) => r.abort());
  await p.addInitScript(MOCK);
  await p.goto('http://localhost:8790/web/index.html');
  await p.waitForFunction(() => window.__appReady);

  assert(await p.isDisabled('#send-sprint'), 'send is disabled without data');
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
  await b.close(); srv.close();
})().catch((e) => { console.error(e); process.exit(1); });
