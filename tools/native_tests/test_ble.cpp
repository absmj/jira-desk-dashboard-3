// Host tests for the BLE receiver logic: TransferSession (framing, CRC, timeout) and
// applyUpload (validate -> store -> swap). No BLE stack, no flash: those are hardware-only.
// usage: test_ble <frames.txt from gen_frames.js>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../../src/app/AlertLoader.h"
#include "../../src/app/SprintLoader.h"
#include "../../src/ble/TransferSession.h"
#include "../../src/ble/UploadApplier.h"

static int g_failed = 0, g_checks = 0;
#define CHECK(c)                                                  \
    do {                                                          \
        ++g_checks;                                               \
        if (!(c)) { ++g_failed; printf("  FAIL line %d: %s\n", __LINE__, #c); } \
    } while (0)

using Bytes = std::vector<uint8_t>;

struct MemStore : IFileStore {
    std::string sprintFile, alertsFile;
    bool fail = false;
    int writes = 0;
    bool writeAtomic(const char* path, const uint8_t* d, size_t n) override {
        if (fail) return false;
        ++writes;
        (strstr(path, "sprint") ? sprintFile : alertsFile).assign(reinterpret_cast<const char*>(d), n);
        return true;
    }
};

static Bytes unhex(const std::string& h) {
    Bytes b;
    for (size_t i = 0; i + 1 < h.size(); i += 2) b.push_back(static_cast<uint8_t>(strtoul(h.substr(i, 2).c_str(), nullptr, 16)));
    return b;
}

// Builds frames in C++ (to craft broken ones the web app would never send).
static std::vector<Bytes> makeFrames(uint8_t id, const std::string& text, uint32_t crc) {
    std::vector<Bytes> fr;
    Bytes b = {1, 0, id, static_cast<uint8_t>(text.size() & 255), static_cast<uint8_t>(text.size() >> 8),
               static_cast<uint8_t>(crc), static_cast<uint8_t>(crc >> 8), static_cast<uint8_t>(crc >> 16), static_cast<uint8_t>(crc >> 24)};
    fr.push_back(b);
    uint8_t seq = 1;
    for (size_t i = 0; i < text.size(); i += 18) {
        Bytes d = {2, seq++};
        for (size_t k = i; k < text.size() && k < i + 18; k++) d.push_back(static_cast<uint8_t>(text[k]));
        fr.push_back(d);
    }
    fr.push_back({3, seq});
    return fr;
}
static uint32_t crcOf(const std::string& s) { return TransferSession::crc32(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }

static TransferSession::Reaction feed(TransferSession& t, const std::vector<Bytes>& fr, uint32_t now = 0) {
    TransferSession::Reaction r{TransferSession::Reaction::None, 0, 0};
    for (const Bytes& f : fr) {
        r = t.onFrame(f.data(), f.size(), now);
        if (r.kind != TransferSession::Reaction::None) break;
    }
    return r;
}

static const char* kGoodSprint = R"({"v":1,"sprint":{"name":"S","start":"2026-10-01","end":"2026-10-18","done":1,"total":2},
 "stats":{"topAssignee":{"name":"A","count":1},"byStatus":{"todo":1,"inProgress":0,"done":1},"overdue":0,"blocked":0,"pace":"onTrack"},
 "tasks":[{"k":"P-1","t":"x","s":"TD"}]})";

static void testCrc() {
    printf("crc32\n");
    CHECK(TransferSession::crc32(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xCBF43926u);
}

static void testSession() {
    printf("session\n");
    static TransferSession t;
    std::string text = kGoodSprint;
    auto r = feed(t, makeFrames(1, text, crcOf(text)));
    CHECK(r.kind == TransferSession::Reaction::Complete && r.fileId == 1);
    CHECK(t.size() == text.size() && memcmp(t.data(), text.data(), text.size()) == 0 && t.data()[t.size()] == 0);

    // busy until finish()
    Bytes begin = makeFrames(1, text, crcOf(text))[0];
    r = t.onFrame(begin.data(), begin.size(), 0);
    CHECK(r.kind == TransferSession::Reaction::Reply && r.status == 6);
    t.finish();
    CHECK(t.state() == TransferSession::State::Idle);

    // bad CRC
    r = feed(t, makeFrames(1, text, crcOf(text) ^ 1));
    CHECK(r.kind == TransferSession::Reaction::Reply && r.status == 2);
    // fewer bytes than announced
    auto fr = makeFrames(1, text, crcOf(text));
    fr.erase(fr.begin() + 2);  // drop a DATA frame -> sequence gap
    r = feed(t, fr);
    CHECK(r.kind == TransferSession::Reaction::Reply && r.status == 5);
    // announced length longer than what arrives
    fr = makeFrames(1, text, crcOf(text));
    fr[0][3] = static_cast<uint8_t>((text.size() + 5) & 255);
    r = feed(t, fr);
    CHECK(r.kind == TransferSession::Reaction::Reply && r.status == 1);
    // announced length shorter than what arrives
    fr = makeFrames(1, text, crcOf(text));
    fr[0][3] = static_cast<uint8_t>((text.size() - 5) & 255);
    r = feed(t, fr);
    CHECK(r.kind == TransferSession::Reaction::Reply && r.status == 1);
    // too large
    Bytes big = {1, 0, 1, 0x01, 0x20 + 1, 0, 0, 0, 0};  // 0x2101 = 8449 > 8192
    r = t.onFrame(big.data(), big.size(), 0);
    CHECK(r.kind == TransferSession::Reaction::Reply && r.status == 6);
    // DATA without BEGIN, garbage, truncated, unknown type
    Bytes d = {2, 1, 'a'};
    r = t.onFrame(d.data(), d.size(), 0);
    CHECK(r.kind == TransferSession::Reaction::Reply && r.status == 5);
    Bytes one = {1};
    r = t.onFrame(one.data(), one.size(), 0);
    CHECK(r.kind == TransferSession::Reaction::Reply && r.status == 5);
    Bytes unk = {9, 0};
    r = t.onFrame(unk.data(), unk.size(), 0);
    CHECK(r.kind == TransferSession::Reaction::Reply && r.status == 5);
    // sequence wrap: 300 DATA frames of 1 byte are > 8192? no, so use 18-byte frames: ~460 frames wraps 256
    std::string longText(8000, 'x');
    r = feed(t, makeFrames(2, longText, crcOf(longText)));
    CHECK(r.kind == TransferSession::Reaction::Complete);
    t.finish();
    // exactly the maximum
    std::string maxText(TransferSession::kMaxFile, 'y');
    r = feed(t, makeFrames(2, maxText, crcOf(maxText)));
    CHECK(r.kind == TransferSession::Reaction::Complete && t.size() == TransferSession::kMaxFile);
    t.finish();
    // ABORT, then restart works
    auto good = makeFrames(1, text, crcOf(text));
    t.onFrame(good[0].data(), good[0].size(), 0);
    Bytes ab = {4, 1};
    t.onFrame(ab.data(), ab.size(), 0);
    CHECK(t.state() == TransferSession::State::Idle);
    CHECK(feed(t, good).kind == TransferSession::Reaction::Complete);
    t.finish();
    // a second BEGIN restarts a half-received file
    t.onFrame(good[0].data(), good[0].size(), 0);
    t.onFrame(good[1].data(), good[1].size(), 0);
    CHECK(feed(t, good).kind == TransferSession::Reaction::Complete);
    t.finish();
}

static void testTimeout() {
    printf("timeout\n");
    TransferSession t;
    std::string text = kGoodSprint;
    auto fr = makeFrames(1, text, crcOf(text));
    t.onFrame(fr[0].data(), fr[0].size(), 1000);
    t.tick(1000 + TransferSession::kTimeoutMs - 1);
    CHECK(t.state() == TransferSession::State::Receiving);
    t.onFrame(fr[1].data(), fr[1].size(), 5000);  // activity resets the timer
    t.tick(5000 + TransferSession::kTimeoutMs - 1);
    CHECK(t.state() == TransferSession::State::Receiving);
    t.tick(5000 + TransferSession::kTimeoutMs);
    CHECK(t.state() == TransferSession::State::Idle);
    // millis() wraparound
    t.onFrame(fr[0].data(), fr[0].size(), 0xFFFFFF00u);
    t.tick(0xFFFFFF00u + 100);  // wraps past 0: only 100 ms elapsed
    CHECK(t.state() == TransferSession::State::Receiving);
    t.tick(0xFFFFFF00u + TransferSession::kTimeoutMs);
    CHECK(t.state() == TransferSession::State::Idle);
    // a completed file waiting to be applied is NOT dropped by the timeout
    TransferSession u;
    CHECK(feed(u, makeFrames(1, text, crcOf(text)), 0).kind == TransferSession::Reaction::Complete);
    u.tick(100000);
    CHECK(u.state() == TransferSession::State::Complete);
}

static void testApply() {
    printf("applyUpload\n");
    static SprintData sprint;
    static AlertConfig alerts;
    MemStore st;
    std::string good = kGoodSprint;
    auto u8 = [](const std::string& s) { return reinterpret_cast<const uint8_t*>(s.data()); };

    CHECK(applyUpload(kFileSprint, u8(good), good.size(), sprint, alerts, st) == 0);
    CHECK(sprint.valid && strcmp(sprint.name, "S") == 0 && st.sprintFile == good);

    // not JSON / wrong structure: live data and stored file untouched
    std::string bad = "{\"v\":1,";
    CHECK(applyUpload(kFileSprint, u8(bad), bad.size(), sprint, alerts, st) == 3);
    std::string wrong = "{\"v\":2}";
    CHECK(applyUpload(kFileSprint, u8(wrong), wrong.size(), sprint, alerts, st) == 3);
    CHECK(strcmp(sprint.name, "S") == 0 && st.sprintFile == good && st.writes == 1);

    // valid new data but the flash write fails: live data must not change
    std::string renamed = good;
    renamed.replace(renamed.find("\"S\""), 3, "\"NEW\"");
    st.fail = true;
    CHECK(applyUpload(kFileSprint, u8(renamed), renamed.size(), sprint, alerts, st) == 4);
    CHECK(strcmp(sprint.name, "S") == 0 && st.sprintFile == good);
    st.fail = false;
    CHECK(applyUpload(kFileSprint, u8(renamed), renamed.size(), sprint, alerts, st) == 0);
    CHECK(strcmp(sprint.name, "NEW") == 0 && st.sprintFile == renamed);

    // alerts: the sprint is left alone
    std::string al = R"({"v":1,"rules":[{"id":"r1","when":{"time":"09:55"},"audio":"0001.mp3","repeat":"daily","text":"hi"}]})";
    CHECK(applyUpload(kFileAlerts, u8(al), al.size(), sprint, alerts, st) == 0);
    CHECK(alerts.count == 1 && strcmp(sprint.name, "NEW") == 0 && st.alertsFile == al);
    std::string alBad = R"({"v":1,"rules":"nope"})";
    CHECK(applyUpload(kFileAlerts, u8(alBad), alBad.size(), sprint, alerts, st) == 3);
    CHECK(alerts.count == 1);

    CHECK(applyUpload(7, u8(good), good.size(), sprint, alerts, st) == 3);  // unknown file id
}

// Frames produced by web/protocol.js for the real data files -> firmware accepts them.
static void testWebFrames(const char* path) {
    printf("frames from web/protocol.js\n");
    FILE* f = fopen(path, "r");
    CHECK(f != nullptr);
    if (!f) return;
    static TransferSession t;
    static SprintData sprint;
    static AlertConfig alerts;
    MemStore st;
    char line[256];
    std::string cur;
    int files = 0, maxLen = 0;
    auto flush = [&](std::vector<Bytes>& fr) {
        if (fr.empty()) return;
        auto r = feed(t, fr);
        CHECK(r.kind == TransferSession::Reaction::Complete);
        if (r.kind == TransferSession::Reaction::Complete) {
            CHECK(applyUpload(t.fileId(), t.data(), t.size(), sprint, alerts, st) == 0);
            ++files;
            t.finish();
        }
        fr.clear();
    };
    std::vector<Bytes> fr;
    std::string name;
    while (fgets(line, sizeof line, f)) {
        char nm[32], hex[128];
        if (sscanf(line, "%31s %127s", nm, hex) != 2) continue;
        if (name != nm) { flush(fr); name = nm; }
        fr.push_back(unhex(hex));
        if (static_cast<int>(fr.back().size()) > maxLen) maxLen = static_cast<int>(fr.back().size());
    }
    flush(fr);
    fclose(f);
    CHECK(files == 2);
    CHECK(maxLen <= 20);
    CHECK(sprint.valid && alerts.count > 0);
}

int main(int argc, char** argv) {
    testCrc();
    testSession();
    testTimeout();
    testApply();
    if (argc > 1) testWebFrames(argv[1]); else printf("(no frames file given: skipping web cross-check)\n");
    printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
