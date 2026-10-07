// Jira JSON -> /device/sprint.json  (pure functions, no DOM: runs in the browser and in Node)
//
// Input : the JSON of  /rest/agile/1.0/sprint/{id}/issue  (or the issue search API).
// Output: the exact document the device expects (see docs/data-format.md).
(function (root) {
  'use strict';

  const LIMITS = { tasks: 24, name: 23, assignee: 23, key: 11, title: 39, file: 8192 };
  const AZ_EXTRA = 'əƏıİöÖüÜçÇşŞğĞ';
  const enc = typeof TextEncoder !== 'undefined' ? new TextEncoder() : new (require('util').TextEncoder)();

  // ---- text ------------------------------------------------------------------------
  function clean(s) {
    return String(s == null ? '' : s)
      .normalize('NFC')                       // the device's glyph table expects NFC
      .replace(/[\u0000-\u001f\u007f]/g, ' ')  // control characters
      .replace(/\s+/g, ' ')
      .trim();
  }
  function byteLen(s) { return enc.encode(s).length; }
  // Cut to at most maxBytes of UTF-8 without splitting a character.
  function truncateUtf8(s, maxBytes) {
    let out = '', used = 0;
    for (const ch of s) {
      const n = byteLen(ch);
      if (used + n > maxBytes) break;
      out += ch; used += n;
    }
    return out;
  }
  function fit(s, maxBytes) { return truncateUtf8(clean(s), maxBytes); }
  // Characters the device cannot draw (it shows '?').
  function unsupportedChars(s) {
    const bad = new Set();
    for (const ch of s) {
      const cp = ch.codePointAt(0);
      if ((cp >= 32 && cp <= 126) || AZ_EXTRA.includes(ch)) continue;
      bad.add(ch);
    }
    return [...bad];
  }

  // ---- dates -------------------------------------------------------------------------
  function dayNumber(iso) {                     // "YYYY-MM-DD" -> days since epoch, or NaN
    const m = /^(\d{4})-(\d{2})-(\d{2})/.exec(iso || '');
    if (!m) return NaN;
    return Math.floor(Date.UTC(+m[1], +m[2] - 1, +m[3]) / 86400000);
  }
  function dateOnly(iso) { const m = /^(\d{4}-\d{2}-\d{2})/.exec(iso || ''); return m ? m[1] : ''; }
  function localToday() {
    const d = new Date();
    const p = (n) => String(n).padStart(2, '0');
    return d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate());
  }

  // ---- reading Jira --------------------------------------------------------------------
  function categoryOf(issue) {                  // "new" | "indeterminate" | "done"
    const f = issue.fields || {};
    return (f.status && f.status.statusCategory && f.status.statusCategory.key) || 'indeterminate';
  }

  function isBlocked(issue) {
    const f = issue.fields || {};
    if (f.status && /block|bloklan|gözləyir/i.test(f.status.name || '')) return true;
    if ((f.labels || []).some((l) => /^(blocked|bloklanıb|blok)$/i.test(l))) return true;
    for (const k of Object.keys(f)) {           // "Flagged" field = customfield_xxxxx with value "Impediment"
      const v = f[k];
      if (k.startsWith('customfield') && Array.isArray(v) && v.some((x) => x && x.value === 'Impediment')) return true;
    }
    for (const link of f.issuelinks || []) {    // "is blocked by" an issue that is not done yet
      const inward = link.inwardIssue;
      if (inward && /is blocked by/i.test((link.type && link.type.inward) || '') &&
          ((inward.fields && inward.fields.status && inward.fields.status.statusCategory &&
            inward.fields.status.statusCategory.key) !== 'done')) return true;
    }
    return false;
  }

  function detectSprint(issues) {
    let best = null;
    const consider = (s) => {
      if (!s || typeof s !== 'object' || !s.name) return;
      if (!best || s.state === 'active' || (best.state !== 'active' && s.state !== 'closed')) best = s;
    };
    for (const is of issues) {
      const f = is.fields || {};
      consider(f.sprint);
      for (const k of Object.keys(f)) {
        if (Array.isArray(f[k])) f[k].forEach((x) => x && x.startDate !== undefined && consider(x));
      }
    }
    return best ? { name: best.name, start: dateOnly(best.startDate), end: dateOnly(best.endDate || best.completeDate) } : null;
  }

  // Ideal burndown: by now we should have finished total * elapsed-fraction tasks.
  function computePace(done, total, start, end, today) {
    const s = dayNumber(start), e = dayNumber(end), t = dayNumber(today);
    if (!(e > s) || total === 0) return 'onTrack';
    const elapsed = Math.min(1, Math.max(0, (t - s) / (e - s)));
    const diff = done - total * elapsed;
    const tol = Math.max(1, total * 0.1);       // within 10% of the sprint (at least 1 task) = on track
    return diff > tol ? 'ahead' : diff < -tol ? 'behind' : 'onTrack';
  }

  // ---- estimates ------------------------------------------------------------------------------------
  // Story points live in a site-specific custom field (customfield_10016 on most Jira Cloud sites);
  // the time estimate is the standard field timeoriginalestimate (seconds).
  function numeric(v) { return typeof v === 'number' && isFinite(v) && v >= 0 ? v : null; }
  function pickEstimate(issues, mode) {
    const has = (name) => issues.some((is) => numeric(is.fields && is.fields[name]) !== null);
    let field = null;
    if (mode && mode !== 'auto') field = mode;
    else if (has('customfield_10016')) field = 'customfield_10016';
    else if (has('timeoriginalestimate')) field = 'timeoriginalestimate';
    if (!field) return { field: null, unit: '', of: () => null };
    const time = field === 'timeoriginalestimate' || field === 'aggregatetimeoriginalestimate';
    return {
      field, unit: time ? 'saat' : 'SP',
      of: (f) => { const v = numeric(f && f[field]); return v === null ? null : (time ? v / 3600 : v); },
    };
  }
  const round1 = (x) => Math.round(x * 10) / 10;

  // ---- main ------------------------------------------------------------------------------
  function convert(jira, opts) {
    opts = opts || {};
    const today = opts.today || localToday();
    const warnings = [], errors = [];
    const issues = Array.isArray(jira) ? jira : (jira && jira.issues);
    if (!Array.isArray(issues)) return { errors: ['JSON-da "issues" siyahısı tapılmadı.'], warnings, device: null };
    if (issues.length === 0) errors.push('Sprint-də heç bir task yoxdur.');
    if (jira && typeof jira.total === 'number' && jira.total > issues.length)
      warnings.push(`Jira ${jira.total} task bildirir, yalnız ${issues.length} yapışdırılıb. URL-ə &startAt=${issues.length} əlavə edib qalanını da yükləyin.`);

    const top = jira && !Array.isArray(jira) && jira.sprint && jira.sprint.name   // attached by web/proxy.js
      ? { name: jira.sprint.name, start: dateOnly(jira.sprint.startDate), end: dateOnly(jira.sprint.endDate || jira.sprint.completeDate) } : null;
    const found = top || detectSprint(issues) || {};
    const o = opts.sprint || {};
    const sprint = {
      name: fit(o.name || found.name || '', LIMITS.name),
      start: o.start || found.start || '',
      end: o.end || found.end || '',
    };
    if (!sprint.name) errors.push('Sprint adı tapılmadı: əl ilə yazın.');
    if (isNaN(dayNumber(sprint.start))) errors.push('Sprint başlama tarixi tapılmadı: əl ilə yazın (YYYY-MM-DD).');
    if (isNaN(dayNumber(sprint.end))) errors.push('Sprint bitmə tarixi tapılmadı: əl ilə yazın (YYYY-MM-DD).');
    if (!errors.length && dayNumber(sprint.end) < dayNumber(sprint.start)) errors.push('Bitmə tarixi başlamadan əvvəldir.');

    const est = pickEstimate(issues, opts.estimateField);
    const rows = issues.map((is) => {
      const f = is.fields || {};
      const cat = categoryOf(is);
      const open = cat !== 'done';
      const blocked = open && isBlocked(is);
      const s = cat === 'done' ? 'DN' : blocked ? 'BL' : cat === 'new' ? 'TD' : 'IP';
      const due = dateOnly(f.duedate);
      return {
        k: fit(is.key, LIMITS.key),
        t: fit(f.summary, LIMITS.title),
        a: fit(f.assignee && f.assignee.displayName, LIMITS.assignee),
        s, cat, open, blocked,
        overdue: open && due !== '' && dayNumber(due) < dayNumber(today),
        due,
        est: est.of(f),
      };
    });

    const total = rows.length;
    if (total > 9999) errors.push('Task sayı 9999-dan çoxdur.');
    const done = rows.filter((r) => !r.open).length;
    const byStatus = {
      todo: rows.filter((r) => r.cat === 'new').length,
      inProgress: rows.filter((r) => r.cat === 'indeterminate').length,
      done,
    };
    const overdue = rows.filter((r) => r.overdue).length;
    const blocked = rows.filter((r) => r.blocked).length;

    // Top assignee = most OPEN tasks (ties: alphabetical, so the result is stable).
    const perPerson = new Map();
    rows.filter((r) => r.open && r.a).forEach((r) => perPerson.set(r.a, (perPerson.get(r.a) || 0) + 1));
    const ranked = [...perPerson.entries()].sort((x, y) => y[1] - x[1] || x[0].localeCompare(y[0], 'az'));
    const topAssignee = ranked.length ? { name: ranked[0][0], count: ranked[0][1] } : { name: '', count: 0 };

    // Open tasks only: blocked first, then overdue, then earliest due date, then key.
    const rank = { BL: 0, IP: 1, TD: 2 };
    const listed = rows.filter((r) => r.open).sort((x, y) =>
      (rank[x.s] - rank[y.s]) || (y.overdue - x.overdue) ||
      ((x.due || '9999') < (y.due || '9999') ? -1 : (x.due || '9999') > (y.due || '9999') ? 1 : 0) ||
      x.k.localeCompare(y.k, 'en', { numeric: true }));
    if (listed.length > LIMITS.tasks) warnings.push(`${listed.length} açıq task var, cihaza yalnız ilk ${LIMITS.tasks} göndərilir.`);

    // Progress weighted by estimates when most tasks have one; otherwise by task count.
    const estimated = rows.filter((r) => r.est !== null);
    const totalPts = estimated.reduce((a, r) => a + r.est, 0);
    const donePts = estimated.filter((r) => !r.open).reduce((a, r) => a + r.est, 0);
    const coverage = total ? estimated.length / total : 0;
    const byPoints = est.field !== null && coverage >= 0.5 && totalPts > 0;
    const pace = byPoints ? computePace(donePts, totalPts, sprint.start, sprint.end, today)
                          : computePace(done, total, sprint.start, sprint.end, today);
    if (est.field === null) warnings.push('Estimate sahəsi tapılmadı (story points və ya original estimate): tempo task sayına görə hesablandı.');
    else if (!byPoints) warnings.push(`Estimate task-ların yalnız ${estimated.length}/${total}-də var (${est.field}): tempo task sayına görə hesablandı.`);

    // Data-quality checks on OPEN tasks
    const open = rows.filter((r) => r.open);
    const list = (arr) => arr.slice(0, 5).map((r) => r.k).join(', ') + (arr.length > 5 ? ` … (+${arr.length - 5})` : '');
    const noEst = est.field === null ? [] : open.filter((r) => r.est === null);
    if (noEst.length) warnings.push(`${noEst.length} açıq task-da estimate yoxdur: ${list(noEst)}`);
    const noWho = open.filter((r) => !r.a);
    if (noWho.length) warnings.push(`${noWho.length} açıq task-ın icraçısı yoxdur: ${list(noWho)}`);
    const late = open.filter((r) => r.due && dayNumber(r.due) > dayNumber(sprint.end));
    if (late.length) warnings.push(`${late.length} task-ın bitmə tarixi sprint-in sonundan sonradır: ${list(late)}`);
    if (dayNumber(today) > dayNumber(sprint.end) && open.length) warnings.push(`Sprint bitib, amma ${open.length} açıq task qalır.`);
    if (dayNumber(today) < dayNumber(sprint.start)) warnings.push('Sprint hələ başlamayıb.');

    const device = {
      v: 1,
      sprint: { name: sprint.name, start: sprint.start, end: sprint.end, done, total },
      stats: { topAssignee, byStatus, overdue, blocked, pace },
      tasks: listed.slice(0, LIMITS.tasks).map((r) => ({ k: r.k, t: r.t, s: r.s, a: r.a })),
    };

    // Warn about text the device will draw as '?'.
    const texts = [device.sprint.name, topAssignee.name].concat(device.tasks.flatMap((t) => [t.t, t.a]));
    const bad = unsupportedChars(texts.join(''));
    if (bad.length) warnings.push('Cihazda "?" kimi görünəcək simvollar: ' + bad.join(' '));
    const json = JSON.stringify(device);
    if (byteLen(json) > LIMITS.file) errors.push(`Fayl çox böyükdür (${byteLen(json)} bayt, limit ${LIMITS.file}).`);

    const summary = { estimateField: est.field, unit: est.unit, estimated: estimated.length, byPoints,
                      donePoints: round1(donePts), totalPoints: round1(totalPts), unestimatedOpen: noEst.length };
    return { device: errors.length ? null : device, json: errors.length ? '' : json, errors, warnings, detected: found, today, summary };
  }

  const api = { convert, pickEstimate, computePace, truncateUtf8, unsupportedChars, isBlocked, detectSprint, dayNumber, LIMITS };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.JiraDevice = api;
})(typeof self !== 'undefined' ? self : this);
