#include "Screens.h"

#include <stdio.h>
#include <string.h>

#include "../display/AzText.h"

// ---- Layout (84 x 48 px, 6 x 8 px cells, 14 chars per line) ---------------------
//   y  0..9    inverted header
//   y 12,21,30,39   four content lines (9 px pitch leaves row 7 free for cedillas)
static constexpr int16_t kHeaderH = 10;
static constexpr int16_t kX = 1;
static constexpr int16_t kFirstLineY = 12;
static constexpr int16_t kLinePitch = 9;
static constexpr uint8_t kLineChars = 13;  // 1 + 13*6 = 79 <= 84

// ---- Labels (Azerbaijani). Change here to switch language. ---------------------------
namespace label {
static const char* kNoData1 = "Məlumat yoxdur";
static const char* kNoData2 = "Veb ilə göndər";
static const char* kStats = "Statistika";
static const char* kAttention = "Diqqət!";
static const char* kTopAssignee = "Ən çox task:";
static const char* kOpenTasks = "açıq task";
static const char* kTodo = "Gözləyən";
static const char* kInProgress = "İcrada";
static const char* kDone = "Bitib";
static const char* kOverdue = "Gecikən";
static const char* kBlocked = "Bloklanıb";
static const char* kTasks = "Tasklar";
static const char* kRemaining = "Qalan";
static const char* kDays = "gün";
static const char* kLastDay = "Son gün!";
static const char* kEnded = "Sprint bitib";
static const char* kDoneShort = "Bitən";
static const char* kOpen = "Açıq";
static const char* kTempoAhead = "Tempo irəlidə";
static const char* kTempoOn = "Tempo normal";
static const char* kTempoBehind = "Tempo geridə";
}  // namespace label

// ---- helpers -------------------------------------------------------------------------
static void header(IDisplay& d, const char* utf8) {
    d.fillRect(0, 0, d.width(), kHeaderH, true);
    char buf[64];
    aztext::fit(buf, sizeof(buf), utf8, kLineChars);
    aztext::draw(d, 2, 1, buf, 1, true);
}

static void line(IDisplay& d, uint8_t row, const char* utf8) {
    char buf[64];
    aztext::fit(buf, sizeof(buf), utf8, kLineChars);
    aztext::draw(d, kX, kFirstLineY + row * kLinePitch, buf, 1, false);
}

static void formatDayMonth(char* out, size_t n, const Date& dt) {
    snprintf(out, n, "%02u.%02u", static_cast<unsigned>(dt.d), static_cast<unsigned>(dt.m));
}

static void daysLeftText(char* out, size_t n, int32_t left) {
    if (left < 0) {
        snprintf(out, n, "%s", label::kEnded);
    } else if (left == 0) {
        snprintf(out, n, "%s", label::kLastDay);
    } else {
        snprintf(out, n, "%s %ld %s", label::kRemaining, static_cast<long>(left), label::kDays);
    }
}

// Returns the Nth open task (skipping done ones) or nullptr.
static const Task* nthOpenTask(const SprintData& s, uint8_t n) {
    const uint8_t limit = s.taskCount > kMaxTasks ? kMaxTasks : s.taskCount;
    for (uint8_t i = 0; i < limit; ++i) {
        if (!isOpenTask(s.tasks[i])) continue;
        if (n == 0) return &s.tasks[i];
        --n;
    }
    return nullptr;
}

// Splits text into display lines of at most maxChars code points, breaking at
// spaces when possible. Writes the next line to out; returns where to continue
// (or nullptr when the text is exhausted).
static const char* wrapNext(const char* s, uint8_t maxChars, char* out, size_t outSize) {
    while (*s == ' ') ++s;
    if (*s == '\0') {
        out[0] = '\0';
        return nullptr;
    }
    const char* p = s;
    const char* lastSpace = nullptr;
    uint8_t n = 0;
    while (*p && n < maxChars) {
        if (*p == ' ') lastSpace = p;
        aztext::nextCodepoint(p);
        ++n;
    }
    const char* end = p;
    const char* next = p;
    if (*p != '\0' && *p != ' ' && lastSpace) {
        end = lastSpace;
        next = lastSpace + 1;
    }
    size_t len = static_cast<size_t>(end - s);
    if (len >= outSize) len = outSize - 1;
    memcpy(out, s, len);
    out[len] = '\0';
    while (*next == ' ') ++next;
    return *next ? next : nullptr;
}

// ---- pages ---------------------------------------------------------------------------------
static void drawNoData(IDisplay& d) {
    header(d, "Jira");
    line(d, 1, label::kNoData1);
    line(d, 2, label::kNoData2);
}

static void drawDashboard(IDisplay& d, const SprintData& s, const Date& today) {
    char buf[64];
    header(d, s.name);

    snprintf(buf, sizeof(buf), "%u%%", static_cast<unsigned>(progressPct(s)));
    aztext::draw(d, 2, 12, buf, 2);

    // Counts live right of the big percentage: 32 px = 5 characters. "26/40"
    // fits; with 3-digit counts it is split over two lines instead.
    snprintf(buf, sizeof(buf), "%u/%u", static_cast<unsigned>(s.done),
             static_cast<unsigned>(s.total));
    if (strlen(buf) <= 5) {
        aztext::draw(d, 52, 13, buf);
        aztext::draw(d, 52, 22, "task");
    } else {
        snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(s.done));
        aztext::draw(d, 52, 13, buf);
        snprintf(buf, sizeof(buf), "/%u", static_cast<unsigned>(s.total));
        aztext::draw(d, 52, 22, buf);
    }

    d.progressBar(0, 30, d.width(), 7, progressPct(s));

    daysLeftText(buf, sizeof(buf), daysLeft(s, today));
    line(d, 3, buf);
}

static void drawSprint(IDisplay& d, const SprintData& s, const Date& today) {
    char a[16], b[16], buf[64];
    header(d, s.name);

    formatDayMonth(a, sizeof(a), s.start);
    formatDayMonth(b, sizeof(b), s.end);
    snprintf(buf, sizeof(buf), "%s - %s", a, b);
    line(d, 0, buf);

    daysLeftText(buf, sizeof(buf), daysLeft(s, today));
    line(d, 1, buf);

    snprintf(buf, sizeof(buf), "%s: %u/%u", label::kDoneShort, static_cast<unsigned>(s.done),
             static_cast<unsigned>(s.total));
    line(d, 2, buf);

    snprintf(buf, sizeof(buf), "%s: %u", label::kOpen,
             static_cast<unsigned>(s.total > s.done ? s.total - s.done : 0));
    line(d, 3, buf);
}

static void drawStats(IDisplay& d, const SprintData& s, uint8_t index) {
    char buf[96];
    header(d, label::kStats);
    if (index == 0) {
        line(d, 0, label::kTopAssignee);
        if (s.stats.topName[0] == '\0') {
            line(d, 1, "-");
        } else {
            line(d, 1, s.stats.topName);
            snprintf(buf, sizeof(buf), "%u %s", static_cast<unsigned>(s.stats.topCount),
                     label::kOpenTasks);
            line(d, 2, buf);
        }
    } else {
        snprintf(buf, sizeof(buf), "%s: %u", label::kTodo, static_cast<unsigned>(s.stats.todo));
        line(d, 0, buf);
        snprintf(buf, sizeof(buf), "%s: %u", label::kInProgress,
                 static_cast<unsigned>(s.stats.inProgress));
        line(d, 1, buf);
        snprintf(buf, sizeof(buf), "%s: %u", label::kDone, static_cast<unsigned>(s.stats.done));
        line(d, 2, buf);
        const char* tempo = s.stats.pace == Pace::Ahead    ? label::kTempoAhead
                            : s.stats.pace == Pace::Behind ? label::kTempoBehind
                                                           : label::kTempoOn;
        line(d, 3, tempo);
    }
}

static void drawAttention(IDisplay& d, const SprintData& s) {
    char buf[64];
    header(d, label::kAttention);
    uint8_t row = 0;
    if (s.stats.overdue > 0) {
        snprintf(buf, sizeof(buf), "%s: %u", label::kOverdue,
                 static_cast<unsigned>(s.stats.overdue));
        line(d, row++, buf);
    }
    if (s.stats.blocked > 0) {
        snprintf(buf, sizeof(buf), "%s: %u", label::kBlocked,
                 static_cast<unsigned>(s.stats.blocked));
        line(d, row++, buf);
    }
}

static void drawTasks(IDisplay& d, const SprintData& s, const Page& p) {
    char buf[96];
    snprintf(buf, sizeof(buf), "%s %u/%u", label::kTasks, static_cast<unsigned>(p.index + 1),
             static_cast<unsigned>(p.count));
    header(d, buf);

    for (uint8_t r = 0; r < Pager::kTaskRowsPerPage; ++r) {
        const Task* t = nthOpenTask(s, p.index * Pager::kTaskRowsPerPage + r);
        if (!t) break;
        // "101 Title", the project prefix is dropped to save width; '!' marks blocked.
        const char* dash = strrchr(t->key, '-');
        const char* num = dash ? dash + 1 : t->key;
        const bool blocked = t->status[0] == 'B' && t->status[1] == 'L';
        snprintf(buf, sizeof(buf), "%s%s %s", blocked ? "!" : "", num, t->title);
        line(d, r, buf);
    }
}

static void drawAlert(IDisplay& d, const Pager& pager) {
    header(d, pager.alertTitle());
    const char* rest = pager.alertMessage();
    char buf[64];
    for (uint8_t row = 0; row < 4 && rest; ++row) {
        rest = wrapNext(rest, kLineChars, buf, sizeof(buf));
        if (buf[0] == '\0') break;
        line(d, row, buf);
    }
}

void renderPage(IDisplay& d, const SprintData& data, const Date& today, const Pager& pager) {
    d.clear();
    const Page p = pager.current();
    switch (p.kind) {
        case PageKind::Alert:
            drawAlert(d, pager);
            break;
        case PageKind::Dashboard:
            drawDashboard(d, data, today);
            break;
        case PageKind::Sprint:
            drawSprint(d, data, today);
            break;
        case PageKind::Stats:
            drawStats(d, data, p.index);
            break;
        case PageKind::Attention:
            drawAttention(d, data);
            break;
        case PageKind::Tasks:
            drawTasks(d, data, p);
            break;
        case PageKind::NoData:
        default:
            drawNoData(d);
            break;
    }
    d.flush();
}
