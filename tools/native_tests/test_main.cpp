// Host-side tests for the pure-C++ logic (no Arduino, no hardware).
// Build and run: tools/native_tests/run.sh
#include <stdio.h>
#include <string.h>

#include "../../src/app/Pager.h"
#include "../../src/app/SampleData.h"
#include "../../src/app/Screens.h"
#include "../../src/app/SprintData.h"
#include "../../src/display/AzText.h"
#include "../../src/display/IDisplay.h"

static int g_failed = 0;
static int g_checks = 0;
#define CHECK(cond)                                                       \
    do {                                                                  \
        ++g_checks;                                                       \
        if (!(cond)) {                                                    \
            ++g_failed;                                                   \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                 \
    } while (0)

// Records every draw call and flags anything that leaves the 84x48 surface.
class MockDisplay : public IDisplay {
public:
    int violations = 0;
    int textCalls = 0;
    bool begin() override { return true; }
    int16_t width() const override { return 84; }
    int16_t height() const override { return 48; }
    void clear() override {}
    void flush() override {}
    void text(int16_t x, int16_t y, const char* s, uint8_t size, bool) override {
        ++textCalls;
        const int w = static_cast<int>(strlen(s)) * 6 * size;
        if (x < 0 || y < 0 || x + w > 84 || y + 8 * size > 48) {
            ++violations;
            printf("    text out of bounds: x=%d y=%d w=%d '%s'\n", x, y, w, s);
        }
    }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool) override {
        if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > 84 || y + h > 48) {
            ++violations;
            printf("    rect out of bounds: x=%d y=%d w=%d h=%d\n", x, y, w, h);
        }
    }
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h) override {
        fillRect(x, y, w, h, true);
    }
    void progressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t) override {
        fillRect(x, y, w, h, true);
    }
};

static void testDates() {
    printf("dates\n");
    Date a{2026, 10, 4}, b{2026, 10, 18};
    CHECK(daysBetween(a, b) == 14);
    CHECK(daysBetween(b, a) == -14);
    CHECK(daysBetween(Date{2028, 2, 28}, Date{2028, 3, 1}) == 2);   // leap year
    CHECK(daysBetween(Date{2027, 2, 28}, Date{2027, 3, 1}) == 1);   // non-leap
    CHECK(daysBetween(Date{2026, 12, 31}, Date{2027, 1, 1}) == 1);  // year boundary

    Date d{};
    CHECK(parseDate("2026-10-18", d) && d.y == 2026 && d.m == 10 && d.d == 18);
    CHECK(!parseDate("2026-13-01", d));
    CHECK(!parseDate("2026-02-30", d));
    CHECK(!parseDate("2026/10/18", d));
    CHECK(!parseDate("26-10-18", d));
    CHECK(!parseDate(nullptr, d));
    CHECK(parseDate("2028-02-29", d));
    CHECK(!parseDate("2027-02-29", d));
}

static void testDerived() {
    printf("derived values\n");
    SprintData s;
    makeSampleData(s);
    CHECK(progressPct(s) == 65);
    CHECK(daysLeft(s, Date{2026, 10, 4}) == 14);
    CHECK(daysLeft(s, Date{2026, 10, 18}) == 0);
    CHECK(daysLeft(s, Date{2026, 10, 20}) == -2);
    CHECK(openTaskCount(s) == 9);  // 10 tasks, 1 done
    s.total = 0;
    CHECK(progressPct(s) == 0);
    s.total = 3;
    s.done = 5;  // inconsistent input must not exceed 100
    CHECK(progressPct(s) == 100);
}

static void testUtf8() {
    printf("utf-8 and text fitting\n");
    const char* az = "əƏıİöÖüÜçÇşŞğĞ";
    const uint32_t expect[] = {0x259, 0x18F, 0x131, 0x130, 0xF6, 0xD6, 0xFC,
                               0xDC,  0xE7,  0xC7,  0x15F, 0x15E, 0x11F, 0x11E};
    const char* p = az;
    for (uint32_t e : expect) CHECK(aztext::nextCodepoint(p) == e);
    CHECK(aztext::nextCodepoint(p) == 0);
    CHECK(aztext::length(az) == 14);

    for (uint32_t e : expect) CHECK(aztext::glyphRows(e) != nullptr);
    CHECK(aztext::glyphRows('a') == nullptr);
    CHECK(aztext::glyphRows(0x4E2D) == nullptr);

    char out[32];
    aztext::fit(out, sizeof(out), "Ödəniş inteqrasiyası", 13);
    CHECK(aztext::length(out) == 13);
    CHECK(out[strlen(out) - 1] == '.');
    CHECK(strncmp(out, "Ödəniş inteq", strlen("Ödəniş inteq")) == 0);

    aztext::fit(out, sizeof(out), "Qısa", 13);
    CHECK(strcmp(out, "Qısa") == 0);

    // A tiny output buffer must still be valid UTF-8 and NUL terminated.
    char tiny[7];
    aztext::fit(tiny, sizeof(tiny), "əəəəəəəə", 8);
    CHECK(strlen(tiny) < sizeof(tiny));
    const char* q = tiny;
    bool bad = false;
    while (*q)
        if (aztext::nextCodepoint(q) == 0xFFFD) bad = true;
    CHECK(!bad);

    // Malformed input never loops forever and always advances.
    const char broken[] = {char(0xC9), 'a', char(0xE2), char(0x82), '\0'};
    const char* r = broken;
    int guard = 0;
    while (*r && guard++ < 10) aztext::nextCodepoint(r);
    CHECK(guard < 10);
}

static int pageIndexNow(const Pager& p) { return static_cast<int>(p.current().kind) * 100 + p.current().index; }

static void testPager() {
    printf("pager\n");
    SprintData s;
    makeSampleData(s);
    Pager pg;
    uint32_t t = 1000;
    pg.rebuild(s, t);

    // Dashboard, Sprint, Stats x2, Attention, Tasks x3 (9 open tasks / 4 per page)
    CHECK(pg.pageCount() == 8);
    CHECK(pg.current().kind == PageKind::Dashboard);
    CHECK(pg.tick(t));          // initial draw requested
    CHECK(!pg.tick(t + 100));   // nothing changes before dwell time
    CHECK(!pg.tick(t + 5999));
    CHECK(pg.tick(t + 6000));   // dashboard dwell = 6000
    CHECK(pg.current().kind == PageKind::Sprint);

    // Walk a full cycle and check order and wrap-around.
    PageKind order[8];
    uint32_t now = t + 6000;
    order[0] = PageKind::Dashboard;
    order[1] = pg.current().kind;
    for (int i = 2; i < 8; ++i) {
        now += 10000;
        CHECK(pg.tick(now));
        order[i] = pg.current().kind;
    }
    CHECK(order[2] == PageKind::Stats && order[3] == PageKind::Stats);
    CHECK(order[4] == PageKind::Attention);
    CHECK(order[5] == PageKind::Tasks && order[6] == PageKind::Tasks && order[7] == PageKind::Tasks);
    now += 10000;
    CHECK(pg.tick(now));
    CHECK(pg.current().kind == PageKind::Dashboard);  // wrapped

    // Attention page is skipped when there is nothing to warn about.
    s.stats.overdue = 0;
    s.stats.blocked = 0;
    pg.rebuild(s, now);
    CHECK(pg.pageCount() == 7);
    bool hasAttention = false;
    for (int i = 0; i < 8; ++i) {
        now += 10000;
        pg.tick(now);
        if (pg.current().kind == PageKind::Attention) hasAttention = true;
    }
    CHECK(!hasAttention);

    // Fewer open tasks -> fewer task pages; none -> no task page.
    for (uint8_t i = 0; i < s.taskCount; ++i) copyStr(s.tasks[i].status, 3, "DN");
    pg.rebuild(s, now);
    CHECK(pg.pageCount() == 4);

    // No data -> a single NoData page that never rotates.
    SprintData empty;
    memset(&empty, 0, sizeof(empty));
    pg.rebuild(empty, now);
    CHECK(pg.pageCount() == 1);
    CHECK(pg.current().kind == PageKind::NoData);
    CHECK(pg.tick(now));
    CHECK(!pg.tick(now + 600000));
}

static void testPagerPauseAndAlert() {
    printf("pager pause / alert / millis overflow\n");
    SprintData s;
    makeSampleData(s);
    Pager pg;
    uint32_t t = 0;
    pg.rebuild(s, t);
    pg.tick(t);

    // Pause freezes rotation; resume gives a full dwell again.
    pg.setPaused(true, 1000);
    CHECK(!pg.tick(60000));
    CHECK(pg.current().kind == PageKind::Dashboard);
    pg.setPaused(false, 60000);
    CHECK(!pg.tick(65999));
    CHECK(pg.tick(66000));
    CHECK(pg.current().kind == PageKind::Sprint);

    // Alert interrupts, rotation is suspended, then the same page returns.
    pg.showAlert("Test", "Mesaj", 70000, 3000);
    CHECK(pg.tick(70000));
    CHECK(pg.current().kind == PageKind::Alert);
    CHECK(!pg.tick(72999));
    CHECK(pg.current().kind == PageKind::Alert);
    CHECK(pg.tick(73000));
    CHECK(pg.current().kind == PageKind::Sprint);
    CHECK(!pg.tick(76999));  // returned page gets its full 4000 ms
    CHECK(pg.tick(77000));
    CHECK(pg.current().kind == PageKind::Stats);

    // Overlong alert text is truncated, never overflows.
    char longText[300];
    memset(longText, 'x', sizeof(longText));
    longText[299] = '\0';
    pg.showAlert(longText, longText, 80000);
    CHECK(strlen(pg.alertTitle()) < 24);
    CHECK(strlen(pg.alertMessage()) < 64);

    // Rotation keeps working across the 32-bit millis() wrap (~49.7 days).
    Pager w;
    uint32_t start = 0xFFFFFF00u;
    w.rebuild(s, start);
    w.tick(start);
    CHECK(!w.tick(start + 100));
    CHECK(w.tick(start + 6000));  // wraps past 0
    CHECK(w.current().kind == PageKind::Sprint);
}

static void testRenderFitsDisplay() {
    printf("every page fits 84x48 (sample data)\n");
    SprintData s;
    makeSampleData(s);
    const Date today{2026, 10, 4};
    Pager pg;
    pg.rebuild(s, 0);

    MockDisplay d;
    uint32_t now = 0;
    for (int i = 0; i < pg.pageCount() + 1; ++i) {
        renderPage(d, s, today, pg);
        now += 10000;
        pg.tick(now);
    }
    CHECK(d.violations == 0);
    CHECK(d.textCalls > 0);

    // Worst-case strings: max-length names, big numbers, long alert text.
    SprintData w = s;
    copyStr(w.name, sizeof(w.name), "ÇOX UZUN SPRINT ADI BURADA");
    copyStr(w.stats.topName, sizeof(w.stats.topName), "Əhmədzadə Məmmədhüseynov");
    w.done = 999;
    w.total = 999;
    w.stats.todo = w.stats.inProgress = w.stats.done = 999;
    w.stats.overdue = w.stats.blocked = 999;
    w.stats.topCount = 999;
    for (uint8_t i = 0; i < w.taskCount; ++i)
        copyStr(w.tasks[i].title, sizeof(w.tasks[i].title), "Çox uzun tapşırıq adı ğşüöıə İƏ");
    Pager pw;
    pw.rebuild(w, 0);
    MockDisplay d2;
    now = 0;
    for (int i = 0; i < pw.pageCount() + 1; ++i) {
        renderPage(d2, w, Date{2026, 10, 19}, pw);  // sprint already ended
        now += 10000;
        pw.tick(now);
    }
    pw.showAlert("Çox uzun xəbərdarlıq başlığı", "Bu çox uzun mesajdır və bir neçə sətrə bölünməlidir ki, ekrana sığsın", now);
    renderPage(d2, w, Date{2026, 10, 18}, pw);  // last day
    CHECK(d2.violations == 0);

    MockDisplay d3;
    Pager pe;
    SprintData none;
    memset(&none, 0, sizeof(none));
    pe.rebuild(none, 0);
    renderPage(d3, none, today, pe);
    CHECK(d3.violations == 0);
}

static void printGlyphs() {
    printf("\nglyph preview (# = pixel):\n");
    const char* az = "əƏıİöÖüÜçÇşŞğĞ";
    const char* p = az;
    for (int n = 0; n < 14; ++n) {
        const char* start = p;
        const uint32_t cp = aztext::nextCodepoint(p);
        const uint8_t* rows = aztext::glyphRows(cp);
        printf("%.*s  U+%04X\n", static_cast<int>(p - start), start, static_cast<unsigned>(cp));
        for (int r = 0; r < 8; ++r) {
            printf("   ");
            for (int c = 0; c < 5; ++c) putchar((rows[r] & (0x10 >> c)) ? '#' : '.');
            putchar('\n');
        }
    }
}

int main(int argc, char** argv) {
    testDates();
    testDerived();
    testUtf8();
    testPager();
    testPagerPauseAndAlert();
    testRenderFitsDisplay();
    if (argc > 1 && strcmp(argv[1], "--glyphs") == 0) printGlyphs();
    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    (void)pageIndexNow;
    return g_failed ? 1 : 0;
}
