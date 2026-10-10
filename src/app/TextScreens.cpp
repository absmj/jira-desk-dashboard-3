#include "TextScreens.h"

#include <stdio.h>
#include <string.h>

#include "../display/AzText.h"

// ---- Layout (16 x 2 characters; the display fits longer text by cutting it) ------------------
namespace {

constexpr uint8_t kMaxCols = 20;  // buffers are sized for up to 20 columns (HD44780 20x4 would reuse this)

namespace label {
const char* kNoData1 = "Məlumat yoxdur";
const char* kNoData2 = "Veb ilə göndər";
const char* kOpenTasks = "açıq task";
const char* kTodo = "Gözl";
const char* kInProgress = "İcra";
const char* kDone = "Bitib";
const char* kOverdue = "Gecikən";
const char* kBlocked = "Bloklanıb";
const char* kAttention = "Diqqət!";
const char* kRemaining = "Qalan";
const char* kDays = "gün";
const char* kLastDay = "Son gün!";
const char* kEnded = "Sprint bitib";
const char* kAhead = "irəlidə";
const char* kOnTrack = "normal";
const char* kBehind = "geridə";
}  // namespace label

struct Lines {
    char row[2][64];
    Lines() { row[0][0] = row[1][0] = '\0'; }
};

// Left part cut to `leftMax` code points, then a gap, then `right` (kept whole). Total <= cols.
void leftRight(char* out, size_t n, const char* left, const char* right, uint8_t cols) {
    const uint8_t rl = static_cast<uint8_t>(aztext::length(right));
    uint8_t leftMax = cols > rl + 1 ? static_cast<uint8_t>(cols - rl - 1) : 0;
    char l[64];
    aztext::fit(l, sizeof(l), left, leftMax);
    const uint8_t used = static_cast<uint8_t>(aztext::length(l));
    int w = snprintf(out, n, "%s", l);
    for (uint8_t i = used; i < cols - rl && w + 1 < static_cast<int>(n); ++i) out[w++] = ' ';
    snprintf(out + w, n - static_cast<size_t>(w), "%s", right);
}

void daysLeftText(char* out, size_t n, int32_t left) {
    if (left < 0) snprintf(out, n, "%s", label::kEnded);
    else if (left == 0) snprintf(out, n, "%s", label::kLastDay);
    else snprintf(out, n, "%s %ld %s", label::kRemaining, static_cast<long>(left), label::kDays);
}

const Task* nthOpenTask(const SprintData& s, uint8_t n) {
    const uint8_t limit = s.taskCount > kMaxTasks ? kMaxTasks : s.taskCount;
    for (uint8_t i = 0; i < limit; ++i) {
        if (!isOpenTask(s.tasks[i])) continue;
        if (n == 0) return &s.tasks[i];
        --n;
    }
    return nullptr;
}

// Next display line of at most maxChars code points, broken at a space when possible.
const char* wrapNext(const char* s, uint8_t maxChars, char* out, size_t outSize) {
    while (*s == ' ') ++s;
    if (*s == '\0') { out[0] = '\0'; return nullptr; }
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
    if (*p != '\0' && *p != ' ' && lastSpace) { end = lastSpace; next = lastSpace + 1; }
    size_t len = static_cast<size_t>(end - s);
    if (len >= outSize) len = outSize - 1;
    memcpy(out, s, len);
    out[len] = '\0';
    while (*next == ' ') ++next;
    return *next ? next : nullptr;
}

void pageNoData(Lines& L) {
    snprintf(L.row[0], sizeof(L.row[0]), "%s", label::kNoData1);
    snprintf(L.row[1], sizeof(L.row[1]), "%s", label::kNoData2);
}

void pageDashboard(Lines& L, const SprintData& s, uint8_t cols) {
    char pct[8];
    snprintf(pct, sizeof(pct), "%u%%", static_cast<unsigned>(progressPct(s)));
    leftRight(L.row[0], sizeof(L.row[0]), s.name, pct, cols);

    // "26/40 =====....." : counts, then a bar that uses the remaining cells
    char counts[16];
    snprintf(counts, sizeof(counts), "%u/%u", static_cast<unsigned>(s.done), static_cast<unsigned>(s.total));
    const uint8_t cl = static_cast<uint8_t>(strlen(counts));
    const uint8_t bar = cols > cl + 1 ? static_cast<uint8_t>(cols - cl - 1) : 0;
    const uint8_t filled = static_cast<uint8_t>((static_cast<uint32_t>(bar) * progressPct(s) + 50) / 100);
    int w = snprintf(L.row[1], sizeof(L.row[1]), "%s ", counts);
    for (uint8_t i = 0; i < bar && w + 1 < static_cast<int>(sizeof(L.row[1])); ++i) L.row[1][w++] = i < filled ? '=' : '.';
    L.row[1][w] = '\0';
}

void pageSprint(Lines& L, const SprintData& s, const Date& today) {
    snprintf(L.row[0], sizeof(L.row[0]), "%02u.%02u - %02u.%02u", static_cast<unsigned>(s.start.d),
             static_cast<unsigned>(s.start.m), static_cast<unsigned>(s.end.d), static_cast<unsigned>(s.end.m));
    daysLeftText(L.row[1], sizeof(L.row[1]), daysLeft(s, today));
}

void pageStats(Lines& L, const SprintData& s, uint8_t index) {
    if (index == 0) {
        if (s.stats.topName[0] == '\0') {
            snprintf(L.row[0], sizeof(L.row[0]), "-");
        } else {
            snprintf(L.row[0], sizeof(L.row[0]), "%s", s.stats.topName);
            snprintf(L.row[1], sizeof(L.row[1]), "%u %s", static_cast<unsigned>(s.stats.topCount), label::kOpenTasks);
        }
    } else {
        snprintf(L.row[0], sizeof(L.row[0]), "%s:%u %s:%u", label::kTodo, static_cast<unsigned>(s.stats.todo),
                 label::kInProgress, static_cast<unsigned>(s.stats.inProgress));
        const char* tempo = s.stats.pace == Pace::Ahead ? label::kAhead
                            : s.stats.pace == Pace::Behind ? label::kBehind : label::kOnTrack;
        snprintf(L.row[1], sizeof(L.row[1]), "%s:%u %s", label::kDone, static_cast<unsigned>(s.stats.done), tempo);
    }
}

void pageAttention(Lines& L, const SprintData& s) {
    char items[2][32];
    uint8_t n = 0;
    if (s.stats.overdue > 0) snprintf(items[n++], 32, "%s: %u", label::kOverdue, static_cast<unsigned>(s.stats.overdue));
    if (s.stats.blocked > 0) snprintf(items[n++], 32, "%s: %u", label::kBlocked, static_cast<unsigned>(s.stats.blocked));
    if (n == 2) {
        snprintf(L.row[0], sizeof(L.row[0]), "%s", items[0]);
        snprintf(L.row[1], sizeof(L.row[1]), "%s", items[1]);
    } else {
        snprintf(L.row[0], sizeof(L.row[0]), "%s", label::kAttention);
        if (n == 1) snprintf(L.row[1], sizeof(L.row[1]), "%s", items[0]);
    }
}

void pageTasks(Lines& L, const SprintData& s, const Page& p, uint8_t perPage) {
    for (uint8_t r = 0; r < 2 && r < perPage; ++r) {
        const Task* t = nthOpenTask(s, static_cast<uint8_t>(p.index * perPage + r));
        if (!t) break;
        const char* dash = strrchr(t->key, '-');
        const char* num = dash ? dash + 1 : t->key;
        const bool blocked = t->status[0] == 'B' && t->status[1] == 'L';
        snprintf(L.row[r], sizeof(L.row[r]), "%s%s %s", blocked ? "!" : "", num, t->title);
    }
}

// A long message is shown two lines at a time, cycling every kAlertChunkMs.
void pageAlert(Lines& L, const Pager& pager, uint8_t cols, uint32_t nowMs) {
    const char* msg = pager.alertMessage()[0] ? pager.alertMessage() : pager.alertTitle();
    char wrapped[8][64];
    uint8_t n = 0;
    const char* rest = msg;
    while (rest && n < 8) {
        rest = wrapNext(rest, cols, wrapped[n], sizeof(wrapped[n]));
        if (wrapped[n][0] == '\0') break;
        ++n;
    }
    if (n == 0) return;
    const uint8_t chunks = static_cast<uint8_t>((n + 1) / 2);
    const uint8_t chunk = static_cast<uint8_t>((nowMs / kAlertChunkMs) % chunks);
    snprintf(L.row[0], sizeof(L.row[0]), "%s", wrapped[chunk * 2]);
    if (chunk * 2 + 1 < n) snprintf(L.row[1], sizeof(L.row[1]), "%s", wrapped[chunk * 2 + 1]);
}

}  // namespace

void renderTextPage(ITextDisplay& d, const SprintData& data, const Date& today, const Pager& pager,
                    uint32_t nowMs) {
    const uint8_t cols = d.cols() > kMaxCols ? kMaxCols : d.cols();
    const Page p = pager.current();
    Lines L;
    switch (p.kind) {
        case PageKind::Alert: pageAlert(L, pager, cols, nowMs); break;
        case PageKind::Dashboard: pageDashboard(L, data, cols); break;
        case PageKind::Sprint: pageSprint(L, data, today); break;
        case PageKind::Stats: pageStats(L, data, p.index); break;
        case PageKind::Attention: pageAttention(L, data); break;
        case PageKind::Tasks: pageTasks(L, data, p, pager.taskRows()); break;
        case PageKind::NoData:
        default: pageNoData(L); break;
    }
    // Cut to the display width ourselves so an overlong line ends in '.', like on the graphic panel.
    char fitted[2][64];
    for (uint8_t i = 0; i < 2; ++i) aztext::fit(fitted[i], sizeof(fitted[i]), L.row[i], cols);
    const char* lines[2] = {fitted[0], fitted[1]};
    d.show(lines, 2);
}
