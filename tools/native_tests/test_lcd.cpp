// Host tests for the 16x2 HD44780 path: UTF-8 -> codes, CGRAM slots, command stream (against a fake
// panel that decodes the 4-bit protocol), and the text screens. No hardware.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "../../src/app/Pager.h"
#include "../../src/app/SampleData.h"
#include "../../src/app/TextScreens.h"
#include "../../src/display/AzText.h"
#include "../../src/display/Hd44780Display.h"

static int g_failed = 0, g_checks = 0;
#define CHECK(c)                                                  \
    do {                                                          \
        ++g_checks;                                               \
        if (!(c)) { ++g_failed; printf("  FAIL line %d: %s\n", __LINE__, #c); } \
    } while (0)

// A fake HD44780 behind the port interface: decodes nibbles, keeps DDRAM/CGRAM, checks init and timing.
struct FakePanel : IHd44780Port {
    uint8_t ddram[80] = {};
    uint8_t cgram[64] = {};
    bool toCgram = false;
    uint8_t addr = 0;
    int initNibbles = 0;
    bool fourBit = false, displayOn = false, entryInc = false, twoLine = false, cursorOn = false;
    bool havePending = false;
    uint8_t pendingHi = 0;
    bool pendingRs = false;
    uint64_t clock = 0, readyAt = 0;
    int timingViolations = 0, cgramWrites = 0, dataWrites = 0, commands = 0;
    bool backlight = true;

    void delayUs(uint32_t us) override { clock += us; }
    void setBacklight(bool on) override { backlight = on; }

    void writeNibble(uint8_t nib, bool rs) override {
        nib &= 0x0F;
        if (clock < readyAt) ++timingViolations;     // previous instruction still busy
        if (!fourBit) {                              // wake-up sequence: 3,3,3 then 2 -> 4-bit
            if (initNibbles < 3) { if (nib != 0x3) ++timingViolations; ++initNibbles; }
            else if (nib == 0x2) fourBit = true;
            else ++timingViolations;
            readyAt = clock + 100;
            return;
        }
        if (!havePending) { havePending = true; pendingHi = nib; pendingRs = rs; return; }
        havePending = false;
        exec(static_cast<uint8_t>((pendingHi << 4) | nib), pendingRs);
    }

    void exec(uint8_t v, bool rs) {
        readyAt = clock + 37;
        if (rs) {
            ++dataWrites;
            if (toCgram) { cgram[addr & 63] = v & 0x1F; ++cgramWrites; addr = (addr + 1) & 63; }
            else { ddram[rowBaseToIndex(addr)] = v; addr = nextDdram(addr); }
            return;
        }
        ++commands;
        if (v == 0x01) { memset(ddram, ' ', sizeof(ddram)); addr = 0; toCgram = false; readyAt = clock + 1520; }
        else if (v == 0x02) { addr = 0; toCgram = false; readyAt = clock + 1520; }
        else if (v & 0x80) { addr = v & 0x7F; toCgram = false; }
        else if (v & 0x40) { addr = v & 0x3F; toCgram = true; }
        else if (v & 0x20) { twoLine = v & 0x08; }
        else if (v & 0x08) { displayOn = v & 0x04; cursorOn = v & 0x02; }
        else if (v & 0x04) { entryInc = v & 0x02; }
    }
    // DDRAM addressing for 2 lines: 0x00-0x27 line 1, 0x40-0x67 line 2
    static int rowBaseToIndex(uint8_t a) { return a < 0x40 ? a : 40 + (a - 0x40); }
    static uint8_t nextDdram(uint8_t a) { return (a + 1) & 0x7F; }

    // What the user sees in row r (16 columns), decoded back to code points.
    std::vector<uint32_t> row(uint8_t r, uint8_t cols = 16) const {
        std::vector<uint32_t> out;
        for (uint8_t c = 0; c < cols; ++c) {
            const uint8_t code = ddram[rowBaseToIndex(static_cast<uint8_t>(r * 0x40 + c))];
            if (code < 8) out.push_back(cpForBitmap(&cgram[code * 8]));
            else out.push_back(code);
        }
        return out;
    }
    static uint32_t cpForBitmap(const uint8_t* bm) {
        static const uint32_t all[] = {0x0259, 0x018F, 0x0131, 0x0130, 0x00F6, 0x00D6, 0x00FC,
                                       0x00DC, 0x00E7, 0x00C7, 0x015F, 0x015E, 0x011F, 0x011E};
        for (uint32_t cp : all)
            if (memcmp(aztext::glyphRows(cp), bm, 8) == 0) return cp;
        return 0xFFFF;
    }
};

static std::vector<uint32_t> utf8Cps(const char* s, size_t pad = 16) {
    std::vector<uint32_t> v;
    while (uint32_t cp = aztext::nextCodepoint(s)) { if (v.size() < pad) v.push_back(cp); }
    while (v.size() < pad) v.push_back(' ');
    return v;
}

static void testInitAndAscii() {
    printf("init + ascii\n");
    FakePanel p;
    Hd44780Display d(p);
    CHECK(d.begin());
    CHECK(p.fourBit && p.twoLine && p.displayOn && !p.cursorOn && p.entryInc);
    CHECK(p.timingViolations == 0);
    const char* l[2] = {"Hello", "World 123"};
    d.show(l, 2);
    CHECK(p.row(0) == utf8Cps("Hello"));
    CHECK(p.row(1) == utf8Cps("World 123"));
    CHECK(p.timingViolations == 0);

    const char* long_[2] = {"0123456789ABCDEFXYZ", ""};  // longer than 16: cut
    d.show(long_, 2);
    CHECK(p.row(0) == utf8Cps("0123456789ABCDEF") && p.row(1) == utf8Cps(""));

    const char* odd[2] = {"a\\b~c", "中?"};              // ROM quirks and unsupported characters
    d.show(odd, 2);
    CHECK(p.row(0) == utf8Cps("a/b-c"));
    CHECK(p.row(1) == utf8Cps("??"));
}

static void testAzGlyphs() {
    printf("azerbaijani glyphs\n");
    FakePanel p;
    Hd44780Display d(p);
    d.begin();
    const char* l[2] = {"Əli Məmmədov", "çiçək şəhər ğ"};
    d.show(l, 2);
    CHECK(d.lastSubstituted() == 0);
    CHECK(p.row(0) == utf8Cps(l[0]));
    CHECK(p.row(1) == utf8Cps(l[1]));
    // each distinct letter occupies exactly one CGRAM slot: Ə ə ç ş ğ = 5
    int uploaded = p.cgramWrites / 8;
    CHECK(uploaded == 5);

    // same letters again: nothing uploaded, and an identical frame sends no rows at all
    const int before = p.dataWrites;
    d.show(l, 2);
    CHECK(p.cgramWrites / 8 == 5 && p.dataWrites == before);

    // only row 1 changes: row 0 is not rewritten (16 data bytes, not 32)
    const char* l2[2] = {"Əli Məmmədov", "ç"};
    d.show(l2, 2);
    CHECK(p.dataWrites - before == 16);
    CHECK(p.row(1) == utf8Cps("ç"));
    CHECK(p.timingViolations == 0);
}

static void testTooManyLetters() {
    printf("more than 8 distinct letters\n");
    FakePanel p;
    Hd44780Display d(p);
    d.begin();
    const char* l[2] = {"əƏıİöÖüÜç", ""};  // 9 distinct
    d.show(l, 2);
    CHECK(d.lastSubstituted() == 1);
    auto r = p.row(0);
    CHECK(r[8] == 'c');                   // the 9th falls back to its ASCII stand-in
    CHECK(r[0] == 0x0259 && r[7] == 0x00DC);
}

static void testRandomFrames() {
    printf("random frames: panel always shows what was asked\n");
    FakePanel p;
    Hd44780Display d(p);
    d.begin();
    srand(12345);
    const char* alphabet[] = {"a", "b", " ", "Z", "9", "ə", "Ə", "ı", "İ", "ö", "Ö", "ü", "Ü", "ç", "Ç", "ş", "Ş", "ğ", "Ğ"};
    int exact = 0, subst = 0;
    for (int f = 0; f < 3000; ++f) {
        char a[80] = "", b[80] = "";
        const int la = rand() % 18, lb = rand() % 18;
        const int k = 4 + rand() % 15;   // restrict the alphabet sometimes so frames stay within 8 letters
        for (int i = 0; i < la; ++i) strcat(a, alphabet[rand() % k]);
        for (int i = 0; i < lb; ++i) strcat(b, alphabet[rand() % k]);
        const char* l[2] = {a, b};
        d.show(l, 2);
        if (d.lastSubstituted() == 0) {
            ++exact;
            if (p.row(0) != utf8Cps(a) || p.row(1) != utf8Cps(b)) { CHECK(false); printf("   frame %d: '%s' / '%s'\n", f, a, b); break; }
        } else {
            ++subst;
        }
    }
    CHECK(exact > 1000);
    CHECK(p.timingViolations == 0);
    printf("   %d exact frames, %d frames that needed ASCII stand-ins\n", exact, subst);
}

// ---- screens ----------------------------------------------------------------------------------
struct CaptureDisplay : ITextDisplay {
    std::string lines[2];
    bool begin() override { return true; }
    uint8_t cols() const override { return 16; }
    uint8_t rows() const override { return 2; }
    void show(const char* const* l, uint8_t n) override { for (uint8_t i = 0; i < 2; ++i) lines[i] = i < n && l[i] ? l[i] : ""; }
    void setBacklight(bool) override {}
};

static SprintData sampleData() {
    SprintData s;
    makeSampleData(s);
    return s;
}

static void testScreens() {
    printf("text screens\n");
    CaptureDisplay d;
    SprintData s = sampleData();
    Pager pg;
    pg.setTaskRows(2);
    pg.rebuild(s, 0);
    const Date today{2026, 10, 4};

    // walk every page of the rotation: all lines fit 16 code points
    uint32_t t = 0;
    int shown = 0;
    bool sawTasks = false, sawDash = false;
    for (int i = 0; i < 40; ++i) {
        renderTextPage(d, s, today, pg, t);
        for (auto& ln : d.lines) CHECK(aztext::length(ln.c_str()) <= 16);
        const Page p = pg.current();
        if (p.kind == PageKind::Dashboard) {
            sawDash = true;
            CHECK(d.lines[0].size() >= 3 && d.lines[0].substr(d.lines[0].size() - 3) == "65%");
            CHECK(aztext::length(d.lines[0].c_str()) == 16);                        // right-aligned percentage
            CHECK(d.lines[1] == "26/40 =======...");
        }
        if (p.kind == PageKind::Tasks) {
            sawTasks = true;
            CHECK(!d.lines[0].empty());
        }
        ++shown;
        t += 7000;
        pg.tick(t);
    }
    CHECK(shown == 40 && sawDash && sawTasks);

    // task pages: 2 rows per page
    int taskPages = 0;
    Pager p2; p2.setTaskRows(2); p2.rebuild(s, 0);
    for (uint8_t i = 0; i < 16 && i < p2.pageCount(); ++i) {
        // count pages through rebuild-time kinds
    }
    uint32_t tt = 0;
    for (int i = 0; i < p2.pageCount(); ++i) { if (p2.current().kind == PageKind::Tasks) ++taskPages; tt += 7000; p2.tick(tt); }
    Pager p4; p4.rebuild(s, 0);
    int taskPages4 = 0; tt = 0;
    for (int i = 0; i < p4.pageCount(); ++i) { if (p4.current().kind == PageKind::Tasks) ++taskPages4; tt += 7000; p4.tick(tt); }
    CHECK(taskPages >= taskPages4 && taskPages > 0);

    // no data
    SprintData none; memset(&none, 0, sizeof none);
    Pager pn; pn.rebuild(none, 0);
    renderTextPage(d, none, today, pn, 0);
    CHECK(d.lines[0] == "Məlumat yoxdur");
}

static void testAlertChunks() {
    printf("alert chunks\n");
    CaptureDisplay d;
    SprintData s = sampleData();
    Pager pg; pg.rebuild(s, 0);
    const Date today{2026, 10, 4};
    pg.showAlert("Diqqət!", "Daily-nin vaxtidir. Standup 5 dəq sonra basliyir, hazir olun", 0);
    renderTextPage(d, s, today, pg, 0);
    const std::string a0 = d.lines[0] + "|" + d.lines[1];
    renderTextPage(d, s, today, pg, kAlertChunkMs);
    const std::string a1 = d.lines[0] + "|" + d.lines[1];
    CHECK(a0 != a1);                                   // second chunk of a long message
    for (auto& ln : d.lines) CHECK(aztext::length(ln.c_str()) <= 16);
    int chunks = 0;                                    // cycles back to the first chunk
    for (uint32_t k = 1; k <= 8; ++k) {
        renderTextPage(d, s, today, pg, kAlertChunkMs * k);
        if (d.lines[0] + "|" + d.lines[1] == a0) { chunks = static_cast<int>(k); break; }
    }
    CHECK(chunks >= 2);

    pg.showAlert("Diqqət!", "Gecikən task var", 0);   // 16 chars: one line, no cycling
    renderTextPage(d, s, today, pg, 0);
    CHECK(d.lines[0] == "Gecikən task var" && d.lines[1].empty());
}

static void testEndToEnd() {
    printf("screens through the HD44780 path\n");
    FakePanel p;
    Hd44780Display lcd(p);
    lcd.begin();
    SprintData s = sampleData();
    Pager pg; pg.setTaskRows(2); pg.rebuild(s, 0);
    const Date today{2026, 10, 4};
    uint32_t t = 0;
    for (int i = 0; i < 30; ++i) {
        renderTextPage(lcd, s, today, pg, t);
        t += 7000;
        pg.tick(t);
    }
    CHECK(p.timingViolations == 0);
    CHECK(p.cgramWrites > 0);   // the sample data has Azerbaijani letters
}

int main() {
    testInitAndAscii();
    testAzGlyphs();
    testTooManyLetters();
    testRandomFrames();
    testScreens();
    testAlertChunks();
    testEndToEnd();
    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
