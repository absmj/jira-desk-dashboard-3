#!/usr/bin/env node
// Local helper server: serves the web app and fetches the active sprint from Jira on its behalf.
// A browser cannot call Jira directly (CORS), and the API token must never reach the page, so the token
// lives only in this process' environment.
//
//   JIRA_SITE=https://yourcompany.atlassian.net JIRA_EMAIL=you@company.com JIRA_TOKEN=... node web/proxy.js
//   open http://localhost:8000/web/index.html
//
// API token: https://id.atlassian.com/manage-profile/security/api-tokens  (needs read access to the board)
// Flow: board id -> active sprint (/rest/agile/1.0/board/{id}/sprint?state=active)
//       -> all its issues (/rest/agile/1.0/sprint/{id}/issue, paged).
'use strict';
const http = require('http');
const https = require('https');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const MAX_ISSUES = 1000;
const TYPES = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.json': 'application/json; charset=utf-8', '.md': 'text/plain; charset=utf-8' };

function jiraGet(cfg, urlPath) {
  return new Promise((resolve, reject) => {
    const u = new URL(urlPath, cfg.site);
    const lib = u.protocol === 'https:' ? https : http;
    const auth = 'Basic ' + Buffer.from(cfg.email + ':' + cfg.token).toString('base64');
    const req = lib.request(u, { method: 'GET', headers: { Authorization: auth, Accept: 'application/json' }, timeout: 15000 }, (res) => {
      let body = '';
      res.setEncoding('utf8');
      res.on('data', (c) => { body += c; });
      res.on('end', () => {
        let json = null;
        try { json = JSON.parse(body); } catch (e) { /* not JSON */ }
        resolve({ status: res.statusCode, json, text: body });
      });
    });
    req.on('timeout', () => req.destroy(new Error('Jira cavab vermədi (15 san.)')));
    req.on('error', reject);
    req.end();
  });
}

class HttpError extends Error { constructor(status, message) { super(message); this.status = status; } }

function jiraFail(r, what) {
  const detail = r.json && (r.json.errorMessages || []).join(' ');
  if (r.status === 401) return new HttpError(401, 'Jira giriş məlumatları qəbul etmədi: JIRA_EMAIL və JIRA_TOKEN-i yoxlayın.');
  if (r.status === 403) return new HttpError(403, 'Jira bu əməliyyata icazə vermədi (' + what + '): token sahibinin board-a girişi yoxdur.');
  if (r.status === 404) return new HttpError(404, what + ' tapılmadı.');
  if (r.status === 400 && /support sprints/i.test(detail || '')) return new HttpError(400, 'Bu board sprint dəstəkləmir (Kanban board). Scrum board-un ID-sini verin.');
  return new HttpError(502, 'Jira xətası (' + what + ', HTTP ' + r.status + ')' + (detail ? ': ' + detail : ''));
}

async function activeSprintIssues(cfg, board) {
  const sp = await jiraGet(cfg, `/rest/agile/1.0/board/${board}/sprint?state=active&maxResults=50`);
  if (sp.status !== 200 || !sp.json) throw jiraFail(sp, 'board ' + board);
  const active = (sp.json.values || []).filter((s) => s.state === undefined || s.state === 'active');
  if (!active.length) throw new HttpError(404, `Board ${board} üçün aktiv sprint yoxdur.`);
  const sprint = active[0];

  const issues = [];
  let total = Infinity;
  while (issues.length < Math.min(total, MAX_ISSUES)) {
    const r = await jiraGet(cfg, `/rest/agile/1.0/sprint/${sprint.id}/issue?startAt=${issues.length}&maxResults=100`);
    if (r.status !== 200 || !r.json) throw jiraFail(r, 'sprint ' + sprint.id);
    const page = r.json.issues || [];
    total = typeof r.json.total === 'number' ? r.json.total : issues.length + page.length;
    if (!page.length) break;
    issues.push(...page);
  }
  return {
    sprint: { id: sprint.id, name: sprint.name, startDate: sprint.startDate, endDate: sprint.endDate, state: sprint.state },
    activeSprints: active.map((s) => ({ id: s.id, name: s.name })),
    multipleActive: active.length > 1,
    total, truncated: issues.length < total, issues,
  };
}

function send(res, status, obj) {
  const body = JSON.stringify(obj);
  res.writeHead(status, { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store' });
  res.end(body);
}

function serveStatic(req, res, pathname) {
  if (pathname === '/') { res.writeHead(302, { location: '/web/index.html' }); return res.end(); }
  // allowlist: the web app and the device alerts template. Nothing else in the repository.
  const ok = (/^\/web\/[\w.\-\/]+$/.test(pathname) && !pathname.includes('..') && !pathname.startsWith('/web/test/'))
    || pathname === '/data/device/alerts.json';
  const file = path.join(ROOT, pathname);
  if (!ok || !file.startsWith(ROOT + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile()) { res.writeHead(404); return res.end('not found'); }
  res.writeHead(200, { 'content-type': TYPES[path.extname(file)] || 'application/octet-stream', 'cache-control': 'no-store' });
  fs.createReadStream(file).pipe(res);
}

function createServer(env) {
  const cfg = { site: env.JIRA_SITE, email: env.JIRA_EMAIL, token: env.JIRA_TOKEN };
  const configured = !!(cfg.site && cfg.email && cfg.token);
  return http.createServer(async (req, res) => {
    try {
      const url = new URL(req.url, 'http://localhost');
      if (req.method !== 'GET') return send(res, 405, { error: 'yalnız GET' });
      if (url.pathname === '/api/config') return send(res, 200, { configured, site: configured ? new URL(cfg.site).host : null });
      if (url.pathname === '/api/sprint-issues') {
        if (!configured) throw new HttpError(500, 'Server qurulmayıb: JIRA_SITE, JIRA_EMAIL və JIRA_TOKEN mühit dəyişənlərini verin.');
        const board = url.searchParams.get('board') || '';
        if (!/^\d{1,10}$/.test(board)) throw new HttpError(400, 'Board ID rəqəm olmalıdır.');
        return send(res, 200, await activeSprintIssues(cfg, board));
      }
      if (url.pathname.startsWith('/api/')) return send(res, 404, { error: 'tanınmayan ünvan' });
      return serveStatic(req, res, decodeURIComponent(url.pathname));
    } catch (e) {
      send(res, e.status || 502, { error: e.status ? e.message : 'Jira ilə əlaqə alınmadı: ' + e.message });
    }
  });
}

module.exports = { createServer, activeSprintIssues };

if (require.main === module) {
  const port = +process.env.PORT || 8000;
  const server = createServer(process.env);
  server.listen(port, '127.0.0.1', () => {
    const ok = process.env.JIRA_SITE && process.env.JIRA_EMAIL && process.env.JIRA_TOKEN;
    console.log(`http://localhost:${port}/web/index.html  (Jira: ${ok ? new URL(process.env.JIRA_SITE).host : 'qurulmayıb: yalnız əl ilə yapışdırma işləyəcək'})`);
  });
}
