#include "Pager.h"

void Pager::add(PageKind kind, uint8_t index, uint8_t count, uint16_t dwellMs) {
    if (count_ >= kMaxPages) return;
    pages_[count_++] = Page{kind, index, count, dwellMs};
}

void Pager::rebuild(const SprintData& data, uint32_t nowMs) {
    count_ = 0;

    if (!data.valid) {
        add(PageKind::NoData, 0, 1, 0);
    } else {
        // Providers, in display order. Adding a statistic = adding a line here
        // plus a renderer in Screens.cpp.
        add(PageKind::Dashboard, 0, 1, 6000);
        add(PageKind::Sprint, 0, 1, 4000);
        add(PageKind::Stats, 0, 2, 4500);  // top assignee
        add(PageKind::Stats, 1, 2, 4500);  // status counts + pace
        if (data.stats.overdue > 0 || data.stats.blocked > 0) {
            add(PageKind::Attention, 0, 1, 4000);
        }
        const uint8_t open = openTaskCount(data);
        const uint8_t pages = (open + taskRows_ - 1) / taskRows_;
        for (uint8_t i = 0; i < pages; ++i) add(PageKind::Tasks, i, pages, 4500);
    }

    idx_ = 0;
    since_ = nowMs;
    redraw_ = true;
}

bool Pager::tick(uint32_t nowMs) {
    if (alertActive_) {
        if (static_cast<uint32_t>(nowMs - alertSince_) >= alertDur_) {
            alertActive_ = false;
            since_ = nowMs;  // the page we return to gets its full dwell time
            redraw_ = true;
        }
    } else if (!paused_ && count_ > 1) {
        if (static_cast<uint32_t>(nowMs - since_) >= pages_[idx_].dwellMs) {
            idx_ = static_cast<uint8_t>((idx_ + 1) % count_);
            since_ = nowMs;
            redraw_ = true;
        }
    }
    const bool r = redraw_;
    redraw_ = false;
    return r;
}

void Pager::setPaused(bool paused, uint32_t nowMs) {
    if (paused_ == paused) return;
    paused_ = paused;
    if (!paused_) since_ = nowMs;  // full dwell after resuming
}

void Pager::showAlert(const char* title, const char* message, uint32_t nowMs,
                      uint32_t durationMs) {
    copyStr(alertTitle_, sizeof(alertTitle_), title);
    copyStr(alertMsg_, sizeof(alertMsg_), message);
    alertActive_ = true;
    alertSince_ = nowMs;
    alertDur_ = durationMs;
    redraw_ = true;
}

Page Pager::current() const {
    if (alertActive_) return Page{PageKind::Alert, 0, 1, 0};
    if (count_ == 0) return Page{PageKind::NoData, 0, 1, 0};
    return pages_[idx_];
}
