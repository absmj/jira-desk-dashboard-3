// Demo data shaped like  /rest/agile/1.0/sprint/{id}/issue  (trimmed). Used by the "Demo" button and the tests.
(function (root) {
  'use strict';
  const sprint = { id: 24, name: 'SPRINT 24', state: 'active', startDate: '2026-10-01T09:00:00.000Z', endDate: '2026-10-18T18:00:00.000Z' };
  const st = (name, cat) => ({ name, statusCategory: { key: cat } });
  const issue = (key, summary, status, assignee, extra) => ({
    key,
    fields: Object.assign({ summary, status, sprint, labels: [], issuelinks: [],
      assignee: assignee ? { displayName: assignee } : null }, extra || {}),
  });
  const A = 'Əli Məmmədov', L = 'Leyla Həsənova', N = 'Nigar Əliyeva', R = 'Rəşad Quliyev';
  const demo = {
    total: 10,
    issues: [
      issue('PRJ-101', 'Giriş ekranı', st('İcrada', 'indeterminate'), A, { duedate: '2026-10-12' }),
      issue('PRJ-102', 'API testləri', st('Görüləcək', 'new'), L, { duedate: '2026-10-04' }),
      issue('PRJ-103', 'Hesabat səhifəsi', st('İcrada', 'indeterminate'), A, { labels: ['blocked'] }),
      issue('PRJ-104', 'Ödəniş inteqrasiyası və kart tokenləşdirmə qaydalarının yoxlanması', st('İcrada', 'indeterminate'), N),
      issue('PRJ-105', 'Şifrə bərpası', st('Görüləcək', 'new'), R, { customfield_10021: [{ value: 'Impediment', id: '10019' }] }),
      issue('PRJ-090', 'Login səhifəsi', st('Hazırdır', 'done'), R),
      issue('PRJ-091', 'Profil səhifəsi', st('Hazırdır', 'done'), A),
      issue('PRJ-106', 'Bildirişlər', st('Görüləcək', 'new'), null),
      issue('PRJ-107', 'Çıxış hesabatı', st('İcrada', 'indeterminate'), L, {
        issuelinks: [{ type: { inward: 'is blocked by', outward: 'blocks' },
          inwardIssue: { key: 'PRJ-104', fields: { status: st('İcrada', 'indeterminate') } } }] }),
      issue('PRJ-108', 'Тест кириллица 🙂', st('Görüləcək', 'new'), A),
    ],
  };
  if (typeof module !== 'undefined' && module.exports) module.exports = demo;
  else root.DEMO_JIRA = demo;
})(typeof self !== 'undefined' ? self : this);
