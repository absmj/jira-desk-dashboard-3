// UI + Web Bluetooth. Conversion lives in convert.js, framing in protocol.js (both pure and tested in Node).
(function () {
  'use strict';
  const $ = (id) => document.getElementById(id);
  const P = window.DeskProtocol, C = window.JiraDevice;

  // ---- sample Jira data ---------------------------------------------------------------------------------
  function sampleJira() {
    const sprint = { name: 'SPRINT 24', state: 'active', startDate: '2026-10-01T05:00:00.000Z', endDate: '2026-10-18T13:00:00.000Z' };
    const mk = (key, summary, cat, who, due, extra) => ({
      key,
      fields: Object.assign({
        summary, sprint,
        status: { name: cat === 'done' ? 'Done' : cat === 'new' ? 'To Do' : 'In Progress', statusCategory: { key: cat } },
        assignee: who ? { displayName: who } : null, duedate: due || null, labels: [], issuelinks: [],
      }, extra || {}),
    });
    return {
      total: 12,
      issues: [
        mk('PRJ-101', 'Giriş ekranı', 'indeterminate', 'Əli Məmmədov', '2026-10-03'),
        mk('PRJ-102', 'API testləri', 'new', 'Leyla Həsənova', '2026-10-12'),
        mk('PRJ-103', 'Hesabat səhifəsi', 'indeterminate', 'Əli Məmmədov', '2026-10-14', { labels: ['blocked'] }),
        mk('PRJ-104', 'Ödəniş inteqrasiyası', 'indeterminate', 'Nigar Əliyeva', '2026-10-10'),
        mk('PRJ-105', 'Şifrə bərpası', 'new', 'Rəşad Quliyev', '2026-10-15'),
        mk('PRJ-106', 'Kredit kalkulyatoru', 'new', 'Əli Məmmədov', '2026-10-16'),
        mk('PRJ-107', 'Müştəri kartı', 'indeterminate', 'Leyla Həsənova', '2026-10-09'),
        mk('PRJ-090', 'Login səhifəsi', 'done', 'Rəşad Quliyev'),
        mk('PRJ-091', 'Profil səhifəsi', 'done', 'Nigar Əliyeva'),
        mk('PRJ-092', 'Bildirişlər', 'done', 'Əli Məmmədov'),
        mk('PRJ-093', 'Axtarış', 'done', 'Leyla Həsənova'),
        mk('PRJ-094', 'Hesabat eksportu', 'done', 'Rəşad Quliyev'),
      ],
    };
  }

  // ---- step 1/2: parse, convert, render -----------------------------------------------------------------------
  let result = null;

  function isoToday() {
    const d = new Date(), p = (n) => String(n).padStart(2, '0');
    return d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate());
  }
  $('f-today').value = isoToday();

  function msg(kind, text) {
    const d = document.createElement('div');
    d.className = 'msg ' + kind;
    d.textContent = text;
    return d;
  }

  function refresh() {
    const box = $('problems');
    box.textContent = '';
    $('chips').textContent = '';
    $('tasks').textContent = '';
    $('json-sprint').textContent = '—';
    result = null;

    const raw = $('jira').value.trim();
    if (!raw) { box.appendChild(msg('warn', 'Jira JSON-u yapışdırın və ya "Nümunə data" düyməsini basın.')); updateButtons(); return; }

    let jira;
    try { jira = JSON.parse(raw); } catch (e) {
      box.appendChild(msg('err', 'JSON oxunmadı: ' + e.message)); updateButtons(); return;
    }
    if (jira && Array.isArray(jira.values) && !Array.isArray(jira.issues)) {
      const act = jira.values.find((v) => v && v.state === 'active') || jira.values[0];
      box.appendChild(msg('warn', act && act.id
        ? `Bu sprint siyahısıdır, task-lar deyil. Aktiv sprint nömrəsi: ${act.id}. Növbəti ünvan: …/rest/agile/1.0/sprint/${act.id}/issue?maxResults=100&fields=*all,-description,-comment`
        : 'Bu sprint siyahısıdır, task-lar deyil.'));
      updateButtons(); return;
    }

    const o = { estimateField: ($('f-est').value.trim() || 'auto'), today: $('f-today').value || isoToday(), sprint: {} };
    if ($('f-name').value.trim()) o.sprint.name = $('f-name').value.trim();
    if ($('f-start').value) o.sprint.start = $('f-start').value;
    if ($('f-end').value) o.sprint.end = $('f-end').value;

    const r = C.convert(jira, o);
    if (r.detected && r.detected.name) {
      $('f-name').placeholder = r.detected.name;
    }
    r.errors.forEach((e) => box.appendChild(msg('err', e)));
    r.warnings.forEach((w) => box.appendChild(msg('warn', w)));
    if (!r.device) { updateButtons(); return; }
    result = r;

    const d = r.device, s = d.stats;
    const pct = d.sprint.total ? Math.round(100 * d.sprint.done / d.sprint.total) : 0;
    const paceText = { ahead: 'qabaqdadır', onTrack: 'normaldır', behind: 'geridədir' }[s.pace];
    [['sprint', d.sprint.name], ['tarix', d.sprint.start + ' → ' + d.sprint.end], ['hazır', `${d.sprint.done}/${d.sprint.total} (${pct}%)`],
     ['görüləcək', s.byStatus.todo], ['icrada', s.byStatus.inProgress], ['gecikmiş', s.overdue], ['bloklanmış', s.blocked],
     ['ən çox task', s.topAssignee.name ? `${s.topAssignee.name} (${s.topAssignee.count})` : '—'],
     ['tempo (' + (r.summary && r.summary.byPoints ? 'estimate-ə görə' : 'task sayına görə') + ')', paceText],
     ...(r.summary && r.summary.estimated ? [['estimate', `${r.summary.donePoints}/${r.summary.totalPoints} ${r.summary.unit}`]] : []),
    ].forEach(([k, v]) => {
      const c = document.createElement('span'); c.className = 'chip';
      const b = document.createElement('b'); b.textContent = k;
      c.append(b, document.createTextNode(String(v)));
      $('chips').appendChild(c);
    });

    const t = $('tasks');
    const head = t.createTHead().insertRow();
    ['', 'açar', 'başlıq', 'icraçı'].forEach((h) => { const th = document.createElement('th'); th.textContent = h; head.appendChild(th); });
    const body = t.createTBody();
    d.tasks.forEach((k) => {
      const tr = body.insertRow();
      const st = document.createElement('span'); st.className = 'st ' + k.s; st.textContent = k.s;
      tr.insertCell().appendChild(st);
      tr.insertCell().textContent = k.k;
      tr.insertCell().textContent = k.t;
      tr.insertCell().textContent = k.a;
    });
    $('json-sprint').textContent = JSON.stringify(d, null, 2) + `\n\n// ${new TextEncoder().encode(r.json).length} bayt`;
    updateButtons();
  }

  ['jira', 'f-name', 'f-start', 'f-end', 'f-today', 'f-est'].forEach((id) => $(id).addEventListener('input', refresh));
  $('sample').addEventListener('click', () => { $('jira').value = JSON.stringify(sampleJira(), null, 2); refresh(); });
  $('file').addEventListener('change', (ev) => {
    const f = ev.target.files && ev.target.files[0];
    if (!f) return;
    const rd = new FileReader();
    rd.onload = () => { $('jira').value = String(rd.result); refresh(); };
    rd.readAsText(f);
  });

  // ---- automatic mode: local proxy finds the active sprint from the board id -------------------------------
  const auto = (t, kind) => { const b = $('auto-state'); b.textContent = ''; if (t) b.appendChild(msg(kind || 'ok', t)); };
  fetch('/api/config').then((r) => (r.ok ? r.json() : Promise.reject())).then((cfg) => {
    if (!cfg) return;
    $('auto').hidden = false;
    $('manual').open = false;
    if (cfg.board) $('f-board').value = String(cfg.board);
    if (cfg.configured === false) auto('Proksi işləyir, lakin JIRA_SITE/JIRA_EMAIL/JIRA_TOKEN təyin olunmayıb.', 'warn');
  }).catch(() => {});
  $('fetch').addEventListener('click', async () => {
    const board = $('f-board').value.trim();
    if (!/^\d+$/.test(board)) { auto('Board ID rəqəm olmalıdır.', 'err'); return; }
    $('fetch').disabled = true; auto('Aktiv sprint axtarılır…', 'warn');
    try {
      const res = await fetch('/api/sprint-issues?board=' + board);
      const j = await res.json().catch(() => ({}));
      if (!res.ok) throw new Error(j.error || ('HTTP ' + res.status));
      $('jira').value = JSON.stringify({ sprint: j.sprint, total: j.total, issues: j.issues }, null, 2);
      let t = `${j.sprint && j.sprint.name ? j.sprint.name : 'sprint'}: ${j.issues.length} task gətirildi.`;
      if (j.multipleActive) t += ' Diqqət: board-da bir neçə aktiv sprint var, ilki seçildi.';
      if (j.truncated) t += ' Task sayı limitə çatdı, siyahı kəsildi.';
      auto(t, j.multipleActive || j.truncated ? 'warn' : 'ok');
      refresh();
    } catch (e) { auto(e.message, 'err'); }
    $('fetch').disabled = false;
  });

  // ---- alerts.json -----------------------------------------------------------------------------------------------------------
  let alertsOk = false;
  function checkAlerts() {
    const box = $('alerts-state');
    box.textContent = '';
    alertsOk = false;
    const text = $('alerts').value.trim();
    if (!text) { box.appendChild(msg('warn', 'alerts.json boşdur: göndərilməyəcək.')); updateButtons(); return; }
    try {
      const j = JSON.parse(text);
      if (j.v !== 1 || !Array.isArray(j.rules)) throw new Error('"v": 1 və "rules" massivi olmalıdır');
      const size = new TextEncoder().encode(text).length;
      if (size > C.LIMITS.file) throw new Error(`fayl çox böyükdür (${size} bayt, limit ${C.LIMITS.file})`);
      alertsOk = true;
      box.appendChild(msg('ok', `${j.rules.length} qayda, ${size} bayt. Qaydaların məzmununu cihaz özü yoxlayır.`));
    } catch (e) { box.appendChild(msg('err', 'alerts.json: ' + e.message)); }
    updateButtons();
  }
  $('alerts').addEventListener('input', checkAlerts);
  fetch('../data/device/alerts.json').then((r) => (r.ok ? r.text() : Promise.reject())).then((t) => { $('alerts').value = t; checkAlerts(); })
    .catch(() => { $('alerts').placeholder = 'alerts.json (layihənin kökündən server edin ki, avtomatik yüklənsin)'; checkAlerts(); });

  function download(name, text) {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob([text], { type: 'application/json' }));
    a.download = name;
    document.body.appendChild(a); a.click(); a.remove();
    setTimeout(() => URL.revokeObjectURL(a.href), 1000);
  }
  $('dl-sprint').addEventListener('click', () => result && download('sprint.json', result.json));
  $('dl-alerts').addEventListener('click', () => alertsOk && download('alerts.json', $('alerts').value));

  // ---- step 3: Web Bluetooth -----------------------------------------------------------------------------------------------------
  const log = (t) => { const l = $('log'); l.textContent += t + '\n'; l.scrollTop = l.scrollHeight; };
  let device = null, rx = null, statusChar = null, waiter = null, busy = false;

  if (!navigator.bluetooth) {
    $('ble-support').appendChild(msg('err', 'Bu brauzer Web Bluetooth dəstəkləmir. Chrome və ya Edge istifadə edin (iOS Safari dəstəkləmir).'));
    $('connect').disabled = true;
  } else if (!window.isSecureContext) {
    $('ble-support').appendChild(msg('err', 'Web Bluetooth yalnız https:// və ya localhost ünvanında işləyir.'));
    $('connect').disabled = true;
  }

  function connected() { return !!(device && device.gatt && device.gatt.connected && rx); }
  function updateButtons() {
    $('send-sprint').disabled = busy || !connected() || !result;
    $('send-alerts').disabled = busy || !connected() || !alertsOk;
    $('disconnect').disabled = busy || !connected();
    $('connect').disabled = busy || connected() || !navigator.bluetooth || !window.isSecureContext;
  }

  function onStatus(ev) {
    const v = new Uint8Array(ev.target.value.buffer);
    if (waiter) { const w = waiter; waiter = null; w(v); }
  }

  async function connect() {
    try {
      $('ble-state').textContent = 'Cihaz axtarılır…';
      device = await navigator.bluetooth.requestDevice({ filters: [{ services: [P.UUID.service] }] });
      device.addEventListener('gattserverdisconnected', () => {
        $('ble-state').textContent = 'Əlaqə kəsildi.'; log('əlaqə kəsildi'); rx = statusChar = null; updateButtons();
      });
      const server = await device.gatt.connect();
      const svc = await server.getPrimaryService(P.UUID.service);
      rx = await svc.getCharacteristic(P.UUID.rx);
      statusChar = await svc.getCharacteristic(P.UUID.status);
      await statusChar.startNotifications();
      statusChar.addEventListener('characteristicvaluechanged', onStatus);
      $('ble-state').textContent = 'Qoşuldu: ' + (device.name || 'cihaz');
      log('qoşuldu: ' + (device.name || device.id));
    } catch (e) {
      $('ble-state').textContent = 'Qoşulmadı: ' + e.message;
      log('xəta: ' + e.message);
      rx = null;
    }
    updateButtons();
  }

  function waitStatus(ms) {
    return new Promise((resolve, reject) => {
      const t = setTimeout(() => { waiter = null; reject(new Error('cihazdan cavab gəlmədi')); }, ms);
      waiter = (v) => { clearTimeout(t); resolve(v); };
    });
  }

  async function sendFile(fileId, label, text) {
    if (!connected() || busy) return;
    busy = true; updateButtons();
    $('progress').value = 0;
    try {
      const frames = P.buildFrames(fileId, text);
      log(`${label}: ${frames.length} paket göndərilir…`);
      const reply = waitStatus(15000);
      reply.catch(() => {});          // an early rejection must not be "unhandled"; it is awaited below
      for (let i = 0; i < frames.length; i++) {
        if (rx.writeValueWithResponse) await rx.writeValueWithResponse(frames[i]); else await rx.writeValue(frames[i]);
        $('progress').value = Math.round(100 * (i + 1) / frames.length);
      }
      const v = await reply;           // [code, fileId]
      if (v[0] === 0) { log(`${label}: cihaz qəbul etdi ✔`); $('ble-state').textContent = label + ' göndərildi.'; }
      else throw new Error('cihaz rədd etdi: ' + (P.STATUS[v[0]] || 'kod ' + v[0]));
    } catch (e) {
      log(`${label}: xəta, ${e.message}`);
      $('ble-state').textContent = label + ': ' + e.message;
    }
    busy = false; updateButtons();
  }

  $('connect').addEventListener('click', connect);
  $('disconnect').addEventListener('click', () => { if (device && device.gatt.connected) device.gatt.disconnect(); });
  $('send-sprint').addEventListener('click', () => result && sendFile(P.FILE.sprint, 'sprint.json', result.json));
  $('send-alerts').addEventListener('click', () => alertsOk && sendFile(P.FILE.alerts, 'alerts.json', $('alerts').value));

  refresh();
  checkAlerts();
  window.__appReady = true;
})();
