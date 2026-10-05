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

  // ---- main ------------------------------------------------------------------------------
  function convert(jira, opts) {
    opts = opts || {};
    const today = opts.today || localToday();
    const warnings = [], errors = [];
    const issues = Array.isArray(jira) ? jira : (jira && jira.issues);
    if (!Array.isArray(issues)) return { errors: ['JSON-da "issues" siyahısı tapılmadı.'], warnings, device: null };
    if (issues.length === 0) errors.push('Sprint-də heç bir task yoxdur.');

    const found = detectSprint(issues) || {};
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

    const device = {
      v: 1,
      sprint: { name: sprint.name, start: sprint.start, end: sprint.end, done, total },
      stats: { topAssignee, byStatus, overdue, blocked, pace: computePace(done, total, sprint.start, sprint.end, today) },
      tasks: listed.slice(0, LIMITS.tasks).map((r) => ({ k: r.k, t: r.t, s: r.s, a: r.a })),
    };

    // Warn about text the device will draw as '?'.
    const texts = [device.sprint.name, topAssignee.name].concat(device.tasks.flatMap((t) => [t.t, t.a]));
    const bad = unsupportedChars(texts.join(''));
    if (bad.length) warnings.push('Cihazda "?" kimi görünəcək simvollar: ' + bad.join(' '));
    const json = JSON.stringify(device);
    if (byteLen(json) > LIMITS.file) errors.push(`Fayl çox böyükdür (${byteLen(json)} bayt, limit ${LIMITS.file}).`);

    return { device: errors.length ? null : device, json: errors.length ? '' : json, errors, warnings, detected: found, today };
  }

  const api = { convert, computePace, truncateUtf8, unsupportedChars, isBlocked, detectSprint, dayNumber, LIMITS };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.JiraDevice = api;
})(typeof self !== 'undefined' ? self : this);
