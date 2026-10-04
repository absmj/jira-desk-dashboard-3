#include "SampleData.h"

static void addTask(SprintData& s, const char* key, const char* title, const char* status,
                    const char* assignee) {
    if (s.taskCount >= kMaxTasks) return;
    Task& t = s.tasks[s.taskCount++];
    copyStr(t.key, sizeof(t.key), key);
    copyStr(t.title, sizeof(t.title), title);
    copyStr(t.status, sizeof(t.status), status);
    copyStr(t.assignee, sizeof(t.assignee), assignee);
}

void makeSampleData(SprintData& s) {
    memset(&s, 0, sizeof(s));
    s.valid = true;
    copyStr(s.name, sizeof(s.name), "SPRINT 24");
    s.start = Date{2026, 10, 1};
    s.end = Date{2026, 10, 18};
    s.done = 26;
    s.total = 40;

    copyStr(s.stats.topName, sizeof(s.stats.topName), "Əli Məmmədov");
    s.stats.topCount = 5;
    s.stats.todo = 6;
    s.stats.inProgress = 8;
    s.stats.done = 26;
    s.stats.overdue = 2;
    s.stats.blocked = 1;
    s.stats.pace = Pace::Behind;

    addTask(s, "PRJ-101", "Giriş ekranı", "IP", "Əli Məmmədov");
    addTask(s, "PRJ-102", "API testləri", "TD", "Leyla Həsənova");
    addTask(s, "PRJ-103", "Hesabat səhifəsi", "BL", "Əli Məmmədov");
    addTask(s, "PRJ-104", "Ödəniş inteqrasiyası", "IP", "Nigar Əliyeva");
    addTask(s, "PRJ-105", "Şifrə bərpası", "TD", "Rəşad Quliyev");
    addTask(s, "PRJ-106", "Çıxış düyməsi", "IP", "Əli Məmmədov");
    addTask(s, "PRJ-107", "Doğrulama kodu", "TD", "Leyla Həsənova");
    addTask(s, "PRJ-108", "İstifadəçi profili", "IP", "Əli Məmmədov");
    addTask(s, "PRJ-109", "Ümumi hesabat", "TD", "Nigar Əliyeva");
    addTask(s, "PRJ-090", "Login səhifəsi", "DN", "Rəşad Quliyev");
}
