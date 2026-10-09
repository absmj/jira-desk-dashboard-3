// node web/test/proxy.test.js  - proxy against a mock Jira (no network, no credentials)
const http = require('http');
const assert = require('assert');
const { createServer } = require('../proxy.js');

let checks = 0;
const ok = (c, m) => { checks++; assert.ok(c, m); };

const AUTH = 'Basic ' + Buffer.from('me@corp.test:secret').toString('base64');
const mkIssues = (n) => Array.from({ length: n }, (_, i) => ({ key: 'P-' + (i + 1), fields: { summary: 's' + i, description: 'LONG TEXT '.repeat(50), comment: { total: 3 } } }));
let mode = 'normal';
const calls = [];

const jira = http.createServer((req, res) => {
  calls.push(req.url);
  const reply = (st, o) => { res.writeHead(st, { 'content-type': 'application/json' }); res.end(JSON.stringify(o)); };
  if (req.headers.authorization !== AUTH) return reply(401, { message: 'bad creds' });
  const u = new URL(req.url, 'http://x');
  let m;
  if ((m = u.pathname.match(/^\/rest\/agile\/1\.0\/board\/(\d+)\/sprint$/))) {
    if (m[1] === '404') return reply(404, { errorMessages: ['board'] });
    if (m[1] === '9') return reply(400, { errorMessages: ['The board does not support sprints'] });
    if (m[1] === '5') return reply(200, { values: [] });
    if (m[1] === '6') return reply(200, { values: [{ id: 61, name: 'A', state: 'active' }, { id: 62, name: 'B', state: 'active' }] });
    return reply(200, { values: [{ id: 42, name: 'SPRINT 24', state: 'active', startDate: '2026-10-01T05:00:00.000Z', endDate: '2026-10-18T13:00:00.000Z' }] });
  }
  if ((m = u.pathname.match(/^\/rest\/agile\/1\.0\/sprint\/(\d+)\/issue$/))) {
    const start = +u.searchParams.get('startAt'), want = +u.searchParams.get('maxResults');
    const total = mode === 'huge' ? 5000 : 130;
    const size = Math.min(want, 50);                       // Jira may cap the page below what we asked
    return reply(200, { startAt: start, maxResults: size, total, issues: mkIssues(total).slice(start, start + size) });
  }
  reply(404, {});
});

const get = (port, p, method = 'GET') => new Promise((resolve, reject) => {
  http.request({ host: '127.0.0.1', port, path: p, method }, (res) => {
    let b = ''; res.on('data', (c) => { b += c; });
    res.on('end', () => { let j = null; try { j = JSON.parse(b); } catch (e) {} resolve({ status: res.statusCode, json: j, text: b, headers: res.headers }); });
  }).on('error', reject).end();
});

(async () => {
  await new Promise((r) => jira.listen(0, '127.0.0.1', r));
  const jp = jira.address().port;
  const good = createServer({ JIRA_SITE: 'http://127.0.0.1:' + jp, JIRA_EMAIL: 'me@corp.test', JIRA_TOKEN: 'secret' });
  const bad = createServer({ JIRA_SITE: 'http://127.0.0.1:' + jp, JIRA_EMAIL: 'me@corp.test', JIRA_TOKEN: 'wrong' });
  const none = createServer({});
  for (const s of [good, bad, none]) await new Promise((r) => s.listen(0, '127.0.0.1', r));
  const [gp, bp, np] = [good, bad, none].map((s) => s.address().port);

  let r = await get(gp, '/api/config');
  ok(r.json.configured === true && r.json.site === '127.0.0.1:' + jp && !JSON.stringify(r.json).includes('secret'), 'config never leaks the token');
  r = await get(np, '/api/config');
  ok(r.json.configured === false);
  r = await get(np, '/api/sprint-issues?board=7');
  ok(r.status === 500 && /JIRA_TOKEN/.test(r.json.error));

  r = await get(gp, '/api/sprint-issues?board=7');
  ok(r.status === 200, JSON.stringify(r.json));
  ok(r.json.sprint.id === 42 && r.json.sprint.name === 'SPRINT 24' && r.json.sprint.endDate.startsWith('2026-10-18'));
  ok(r.json.issues.length === 130 && r.json.total === 130 && !r.json.truncated, 'paged 50+50+30');
  ok(new Set(r.json.issues.map((i) => i.key)).size === 130, 'no duplicates across pages');
  ok(r.json.issues.every((i) => !('description' in i.fields) && !('comment' in i.fields)), 'description and comments are stripped');
  ok(calls.filter((c) => c.includes('/issue?')).every((c) => decodeURIComponent(c).includes('fields=*all,-description')), 'Jira is asked to leave descriptions out');
  ok(calls.filter((c) => c.includes('/issue')).length === 3, 'three issue pages');
  ok(calls[0].includes('/board/7/sprint?state=active'), 'board -> active sprint first');

  r = await get(gp, '/api/sprint-issues?board=6');
  ok(r.json.multipleActive === true && r.json.sprint.id === 61 && r.json.activeSprints.length === 2);
  r = await get(gp, '/api/sprint-issues?board=5');
  ok(r.status === 404 && /aktiv sprint yoxdur/.test(r.json.error));
  r = await get(gp, '/api/sprint-issues?board=404');
  ok(r.status === 404);
  r = await get(gp, '/api/sprint-issues?board=9');
  ok(r.status === 400 && /Kanban/.test(r.json.error));
  r = await get(bp, '/api/sprint-issues?board=7');
  ok(r.status === 401 && /JIRA_TOKEN/.test(r.json.error));
  mode = 'huge';
  r = await get(gp, '/api/sprint-issues?board=7');
  ok(r.json.issues.length === 1000 && r.json.truncated === true && r.json.total === 5000, 'hard cap of 1000 issues');
  mode = 'normal';

  r = await get(gp, '/api/sprint-issues?board=abc');
  ok(r.status === 400);
  r = await get(gp, '/api/sprint-issues?board=7%2F..%2F1');
  ok(r.status === 400, 'only digits reach Jira');
  r = await get(gp, '/api/sprint-issues?board=7', 'POST');
  ok(r.status === 405);
  r = await get(gp, '/api/nothing');
  ok(r.status === 404);

  r = await get(gp, '/web/index.html');
  ok(r.status === 200 && r.text.includes('Sprint Desk'));
  r = await get(gp, '/data/device/alerts.json');
  ok(r.status === 200 && JSON.parse(r.text).v === 1);
  for (const p of ['/web/../README.md', '/web/%2e%2e/platformio.ini', '/platformio.ini', '/src/main.cpp', '/.git/config', '/web/test/test.js', '/data/device/sprint.json']) {
    r = await get(gp, p);
    ok(r.status === 404, 'blocked: ' + p + ' -> ' + r.status);
  }

  for (const s of [good, bad, none, jira]) s.close();
  console.log(`proxy tests: ${checks} checks passed`);
})().catch((e) => { console.error(e); process.exit(1); });
