// node web/test/test.js   (no dependencies)
const assert = require('assert');
const C = require('../convert.js');
const P = require('../protocol.js');
let n = 0;
const ok = (c, m) => { n++; assert.ok(c, m); };

// ---- helpers -------------------------------------------------------------------------------
const st = (key, name) => ({ key, name });
const issue = (key, summary, cat, extra = {}) => ({
  key,
  fields: Object.assign({
    summary,
    status: { name: cat === 'done' ? 'Done' : cat === 'new' ? 'To Do' : 'In Progress', statusCategory: { key: cat } },
    assignee: null, duedate: null, labels: [], issuelinks: [],
    sprint: { name: 'SPRINT 24', state: 'active', startDate: '2026-10-01T05:00:00.000Z', endDate: '2026-10-18T13:00:00.000Z' },
  }, extra),
});
const who = (n) => ({ displayName: n });
const today = '2026-10-05';

// ---- convert -----------------------------------------------------------------------------------
{
  const jira = { issues: [
    issue('PRJ-1', 'Giriş ekranı', 'indeterminate', { assignee: who('Əli Məmmədov'), duedate: '2026-10-03' }), // overdue
    issue('PRJ-2', 'API testləri', 'new', { assignee: who('Leyla Həsənova') }),
    issue('PRJ-3', 'Hesabat', 'indeterminate', { assignee: who('Əli Məmmədov'), labels: ['blocked'] }),
    issue('PRJ-4', 'Ödəniş', 'done', { assignee: who('Əli Məmmədov'), duedate: '2026-09-01' }),                 // done: never overdue
    issue('PRJ-5', 'Şifrə', 'new', { assignee: who('Rəşad Quliyev'),
      issuelinks: [{ type: { inward: 'is blocked by' }, inwardIssue: { fields: { status: { statusCategory: { key: 'new' } } } } }] }),
    issue('PRJ-6', 'Bitmiş', 'done'),
  ] };
  const r = C.convert(jira, { today });
  ok(r.errors.length === 0, r.errors.join());
  const d = r.device;
  ok(d.v === 1 && d.sprint.name === 'SPRINT 24');
  ok(d.sprint.start === '2026-10-01' && d.sprint.end === '2026-10-18', 'dates cut to YYYY-MM-DD');
  ok(d.sprint.total === 6 && d.sprint.done === 2);
  ok(d.stats.byStatus.todo === 2 && d.stats.byStatus.inProgress === 2 && d.stats.byStatus.done === 2);
  ok(d.stats.overdue === 1, 'only the open overdue task counts');
  ok(d.stats.blocked === 2, 'label + "is blocked by" link');
  ok(d.stats.topAssignee.name === 'Əli Məmmədov' && d.stats.topAssignee.count === 2, 'top = most OPEN tasks');
  ok(d.tasks.length === 4 && d.tasks.every((t) => t.s !== 'DN'), 'done tasks not listed');
  ok(d.tasks[0].s === 'BL' && d.tasks[1].s === 'BL', 'blocked first');
  ok(d.tasks.find((t) => t.k === 'PRJ-1').s === 'IP');
  ok(JSON.parse(r.json).tasks.length === 4);
}
{ // status-name blocked, flagged field, overrides, errors
  const flagged = issue('PRJ-7', 'X', 'indeterminate', { customfield_10021: [{ value: 'Impediment' }] });
  ok(C.isBlocked(flagged), 'Flagged / Impediment');
  ok(C.isBlocked(issue('A-1', 'x', 'indeterminate', { status: { name: 'Blocked', statusCategory: { key: 'indeterminate' } } })));
  ok(!C.isBlocked(issue('A-2', 'x', 'indeterminate')));
  const none = C.convert({ issues: [{ key: 'A-1', fields: { summary: 's', status: { statusCategory: { key: 'new' } } } }] }, { today });
  ok(none.errors.length === 3 && none.device === null, 'no sprint info -> name, start and end errors');
  const fixed = C.convert({ issues: [{ key: 'A-1', fields: { summary: 's', status: { statusCategory: { key: 'new' } } } }] },
    { today, sprint: { name: 'S1', start: '2026-10-01', end: '2026-10-10' } });
  ok(fixed.errors.length === 0 && fixed.device.sprint.name === 'S1');
  ok(C.convert({ foo: 1 }, { today }).errors.length === 1);
  ok(C.convert({ issues: [] }, { today, sprint: { name: 'S', start: '2026-10-01', end: '2026-10-10' } }).errors.length === 1);
  ok(C.convert(jiraOf(1), { today, sprint: { name: 'S', start: '2026-10-10', end: '2026-10-01' } }).errors.length === 1, 'end before start');
}
function jiraOf(k) { return { issues: Array.from({ length: k }, (_, i) => issue('P-' + i, 't' + i, 'new')) }; }
{ // limits, truncation, normalisation, unsupported glyphs
  const big = C.convert(jiraOf(40), { today });
  ok(big.device.tasks.length === 24 && big.warnings.some((w) => w.includes('24')), 'max 24 tasks + warning');
  ok(big.device.sprint.total === 40);
  const long = issue('PROJECT-123456', 'Ə'.repeat(60), 'new', { assignee: who('Ş'.repeat(40)) });
  const r = C.convert({ issues: [long] }, { today });
  const t = r.device.tasks[0];
  ok(Buffer.byteLength(t.t) <= 39 && Buffer.byteLength(t.a) <= 23 && Buffer.byteLength(t.k) <= 11, 'byte limits');
  ok(!t.t.includes('�') && /^Ə+$/.test(t.t), 'never cut inside a UTF-8 character');
  const nfd = C.convert({ issues: [issue('A-1', 'Çox şey', 'new')] }, { today });   // C + combining cedilla
  ok(nfd.device.tasks[0].t === 'Çox şey', 'NFC');
  const emoji = C.convert({ issues: [issue('A-1', 'Deploy 🚀 Привет', 'new')] }, { today });
  ok(emoji.warnings.some((w) => w.includes('🚀')) && emoji.warnings.some((w) => w.includes('П')), 'unsupported glyph warning');
  ok(C.unsupportedChars('əƏıİöÖüÜçÇşŞğĞ abc ~').length === 0, 'all Azerbaijani letters supported');
  ok(C.truncateUtf8('ab', 1) === 'a');
}
{ // pace
  ok(C.computePace(10, 40, '2026-10-01', '2026-10-21', '2026-10-11') === 'behind', '10/40 at 50%');
  ok(C.computePace(20, 40, '2026-10-01', '2026-10-21', '2026-10-11') === 'onTrack');
  ok(C.computePace(30, 40, '2026-10-01', '2026-10-21', '2026-10-11') === 'ahead');
  ok(C.computePace(0, 0, '2026-10-01', '2026-10-21', '2026-10-11') === 'onTrack');
  ok(C.computePace(0, 10, '2026-10-01', '2026-10-21', '2026-09-20') === 'onTrack', 'before start');
}

// ---- estimates and checks -----------------------------------------------------------------------------
{
  const sp = (v) => ({ customfield_10016: v });
  const sprintOpts = { today: '2026-10-11', sprint: { name: 'S', start: '2026-10-01', end: '2026-10-21' } };
  const mk = (list) => ({ issues: list.map(([k, cat, extra]) => issue(k, 't' + k, cat, Object.assign({ assignee: who('A') }, extra))) });

  // one big finished task: by count we look behind, by points we are ahead
  const j = mk([['A-1', 'done', sp(13)], ['A-2', 'new', sp(1)], ['A-3', 'new', sp(1)], ['A-4', 'new', sp(1)], ['A-5', 'new', sp(1)]]);
  const r = C.convert(j, sprintOpts);
  ok(r.summary.estimateField === 'customfield_10016' && r.summary.unit === 'SP' && r.summary.byPoints);
  ok(r.summary.donePoints === 13 && r.summary.totalPoints === 17);
  ok(r.device.stats.pace === 'ahead', 'pace weighted by story points');
  ok(r.device.sprint.done === 1 && r.device.sprint.total === 5, 'done/total stay task counts (the screen says "task")');
  const noPts = C.convert(j, Object.assign({ estimateField: 'customfield_99999' }, sprintOpts));
  ok(noPts.device.stats.pace === 'behind' && noPts.warnings.some((w) => w.includes('0/5')), 'unknown field -> by count + warning');

  // time estimate in seconds -> hours
  const t = C.convert(mk([['T-1', 'done', { timeoriginalestimate: 7200 }], ['T-2', 'new', { timeoriginalestimate: 3600 }]]), sprintOpts);
  ok(t.summary.estimateField === 'timeoriginalestimate' && t.summary.unit === 'saat' && t.summary.totalPoints === 3);

  // no estimate field at all
  const none = C.convert(mk([['N-1', 'new'], ['N-2', 'done']]), sprintOpts);
  ok(none.summary.estimateField === null && none.warnings.some((w) => w.includes('Estimate sahəsi tapılmadı')));

  // low coverage -> by count, warning names the numbers
  const low = C.convert(mk([['L-1', 'done', sp(8)], ['L-2', 'new'], ['L-3', 'new'], ['L-4', 'new']]), sprintOpts);
  ok(!low.summary.byPoints && low.warnings.some((w) => w.includes('1/4')));

  // quality checks
  const q = C.convert({ issues: [
    issue('Q-1', 'a', 'new', { customfield_10016: 3 }),                                                    // no assignee
    issue('Q-2', 'b', 'indeterminate', { assignee: who('A'), duedate: '2026-11-01', customfield_10016: 2 }), // due after sprint end
    issue('Q-3', 'c', 'new', { assignee: who('A') }),                                                       // no estimate
  ] }, sprintOpts);
  ok(q.warnings.some((w) => w.includes('icraçısı yoxdur') && w.includes('Q-1')));
  ok(q.warnings.some((w) => w.includes('sonundan sonradır') && w.includes('Q-2')));
  ok(q.warnings.some((w) => w.includes('estimate yoxdur') && w.includes('Q-3')) && q.summary.unestimatedOpen === 1);
  const ended = C.convert(mk([['E-1', 'new', sp(1)]]), Object.assign({}, sprintOpts, { today: '2026-10-25' }));
  ok(ended.warnings.some((w) => w.includes('Sprint bitib')));
  const future = C.convert(mk([['F-1', 'new', sp(1)]]), Object.assign({}, sprintOpts, { today: '2026-09-20' }));
  ok(future.warnings.some((w) => w.includes('hələ başlamayıb')));
  const clean = C.convert(mk([['C-1', 'new', sp(1)], ['C-2', 'done', sp(1)]]), sprintOpts);
  ok(clean.warnings.length === 0, 'a clean sprint has no warnings: ' + clean.warnings.join('|'));

  // sprint attached by the proxy wins over detection inside the issues
  const viaProxy = C.convert({ sprint: { name: 'From proxy', startDate: '2026-10-02T00:00:00Z', endDate: '2026-10-09T00:00:00Z' },
    issues: [{ key: 'X-1', fields: { summary: 's', status: { statusCategory: { key: 'new' } }, assignee: who('A'), customfield_10016: 1 } }] }, { today: '2026-10-05' });
  ok(viaProxy.errors.length === 0 && viaProxy.device.sprint.name === 'From proxy' && viaProxy.device.sprint.end === '2026-10-09');
}

// ---- protocol --------------------------------------------------------------------------------------
{
  ok(P.crc32(new TextEncoder().encode('123456789')) === 0xcbf43926, 'crc32 check value');
  const text = JSON.stringify({ v: 1, s: 'Əli Məmmədov '.repeat(20) });
  const bytes = new TextEncoder().encode(text);
  const frames = P.buildFrames(P.FILE.sprint, text);
  ok(frames.every((f) => f.length <= 20), 'every write fits the default 20-byte ATT payload');
  ok(frames[0][0] === P.T.BEGIN && frames[0][2] === 1 && frames[0].length === 9);
  const dv = new DataView(frames[0].buffer);
  ok(dv.getUint16(3, true) === bytes.length && dv.getUint32(5, true) === P.crc32(bytes));
  ok(frames[frames.length - 1][0] === P.T.END);
  ok(frames.slice(1, -1).every((f) => f[0] === P.T.DATA));
  ok(frames.every((f, i) => f[1] === (i & 0xff)), 'sequence numbers');
  const got = Buffer.concat(frames.slice(1, -1).map((f) => Buffer.from(f.subarray(2))));
  ok(got.equals(Buffer.from(bytes)), 'payload reassembles exactly');
}

console.log(`web tests: ${n} checks passed`);
