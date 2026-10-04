#pragma once
// Automatic page rotation. Pure C++: time is passed in (millis()), so the
// logic is unit-tested on a PC.
//
// Pages are generated from the data by "providers" (see Pager.cpp). Pages with
// nothing to show are never created, so they are skipped automatically.
#include <stdint.h>

#include "SprintData.h"

enum class PageKind : uint8_t {
    NoData,     // nothing received yet
    Dashboard,  // % + bar + days left
    Sprint,     // dates, days left, done / open
    Stats,      // index 0: top assignee, index 1: status counts + pace
    Attention,  // overdue / blocked (only exists when there is something)
    Tasks,      // open task list, kTaskRowsPerPage rows per page
    Alert,      // interrupt page, never part of the rotation
};

struct Page {
    PageKind kind;
    uint8_t index;     // position among pages of the same kind
    uint8_t count;     // how many pages of that kind
    uint16_t dwellMs;  // how long to stay before rotating
};

class Pager {
public:
    static constexpr uint8_t kMaxPages = 16;
    static constexpr uint8_t kTaskRowsPerPage = 4;
    static constexpr uint32_t kDefaultAlertMs = 8000;

    // Regenerates the page list after new data arrives. Restarts at page 0 and
    // requests a redraw. An active alert keeps showing until it expires.
    void rebuild(const SprintData& data, uint32_t nowMs);

    // Call from loop() as often as you like. Returns true when the screen must
    // be redrawn (page changed, alert started/ended, data rebuilt).
    bool tick(uint32_t nowMs);

    // Freezes rotation (e.g. while a BLE transfer is running).
    void setPaused(bool paused, uint32_t nowMs);

    // Interrupts the rotation with a message for durationMs, then returns to the
    // page that was showing. Text is copied. Used by sound/alert rules.
    void showAlert(const char* title, const char* message, uint32_t nowMs,
                   uint32_t durationMs = kDefaultAlertMs);

    Page current() const;
    bool alertActive() const { return alertActive_; }
    const char* alertTitle() const { return alertTitle_; }
    const char* alertMessage() const { return alertMsg_; }
    uint8_t pageCount() const { return count_; }

private:
    void add(PageKind kind, uint8_t index, uint8_t count, uint16_t dwellMs);

    Page pages_[kMaxPages] = {};
    uint8_t count_ = 0;
    uint8_t idx_ = 0;
    uint32_t since_ = 0;
    bool paused_ = false;
    bool redraw_ = true;

    bool alertActive_ = false;
    uint32_t alertSince_ = 0;
    uint32_t alertDur_ = 0;
    char alertTitle_[24] = {};
    char alertMsg_[64] = {};
};
