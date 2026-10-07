#include "TransferSession.h"

namespace {
constexpr uint8_t kBegin = 1, kData = 2, kEnd = 3, kAbort = 4;
}

uint32_t TransferSession::crc32(const uint8_t* p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

TransferSession::Reaction TransferSession::onFrame(const uint8_t* f, size_t n, uint32_t nowMs) {
    Reaction r{Reaction::None, 0, fileId_};
    if (!f || n < 2) { fail(kSequence, r); return r; }
    const uint8_t type = f[0], seq = f[1];

    if (state_ == State::Complete) return reply(kTooLarge);  // busy: previous file not applied yet
    lastMs_ = nowMs;

    switch (type) {
    case kBegin: {
        if (n != 9) { fail(kSequence, r); return r; }
        fileId_ = f[2];
        const size_t len = static_cast<size_t>(f[3]) | (static_cast<size_t>(f[4]) << 8);
        if (len > kMaxFile || len == 0) { fail(kTooLarge, r); return r; }
        len_ = len;
        got_ = 0;
        crc_ = static_cast<uint32_t>(f[5]) | (static_cast<uint32_t>(f[6]) << 8) |
               (static_cast<uint32_t>(f[7]) << 16) | (static_cast<uint32_t>(f[8]) << 24);
        seq_ = seq;  // BEGIN's own seq (0 from the web app); DATA continues from it
        state_ = State::Receiving;
        return r;
    }
    case kData: {
        if (state_ != State::Receiving || seq != static_cast<uint8_t>(seq_ + 1)) { fail(kSequence, r); return r; }
        const size_t chunk = n - 2;
        if (got_ + chunk > len_) { fail(kLength, r); return r; }  // more bytes than BEGIN announced
        for (size_t i = 0; i < chunk; i++) buf_[got_ + i] = f[2 + i];
        got_ += chunk;
        seq_ = seq;
        return r;
    }
    case kEnd: {
        if (state_ != State::Receiving || seq != static_cast<uint8_t>(seq_ + 1)) { fail(kSequence, r); return r; }
        if (got_ != len_) { fail(kLength, r); return r; }
        buf_[len_] = 0;
        if (crc32(buf_, len_) != crc_) { fail(kCrc, r); return r; }
        state_ = State::Complete;
        return Reaction{Reaction::Complete, kOk, fileId_};
    }
    case kAbort:
        state_ = State::Idle;
        return r;
    default:
        fail(kSequence, r);
        return r;
    }
}

TransferSession::Reaction TransferSession::tick(uint32_t nowMs) {
    if (state_ == State::Receiving && static_cast<uint32_t>(nowMs - lastMs_) >= kTimeoutMs) state_ = State::Idle;
    return Reaction{Reaction::None, 0, fileId_};
}

void TransferSession::finish() { state_ = State::Idle; }
