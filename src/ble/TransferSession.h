#pragma once
#include <stddef.h>
#include <stdint.h>

// Receives one file from the web app, frame by frame (docs/ble-protocol.md).
// Pure logic: no Arduino, no BLE, no storage. Tested on the host.
//
//   onFrame()  for every write to the RX characteristic
//   tick()     periodically; drops a half-received file after kTimeoutMs of silence
//   When onFrame() returns Complete the file is verified (length + CRC) and data()/size() are valid
//   until finish() is called. Frames that arrive meanwhile are answered with "busy".
class TransferSession {
public:
    static constexpr size_t kMaxFile = 8192;
    static constexpr uint32_t kTimeoutMs = 5000;

    enum Status : uint8_t {
        kOk = 0, kLength = 1, kCrc = 2, kRejected = 3, kStorage = 4, kSequence = 5, kTooLarge = 6,
    };
    enum class State : uint8_t { Idle, Receiving, Complete };

    struct Reaction {
        enum Kind : uint8_t { None, Reply, Complete } kind;
        uint8_t status;  // for Reply
        uint8_t fileId;
    };

    Reaction onFrame(const uint8_t* f, size_t n, uint32_t nowMs);
    Reaction tick(uint32_t nowMs);  // may return None; a timeout is silent (the web app has its own timeout)
    void finish();                  // call after the file was applied; back to Idle
    void abort() { state_ = State::Idle; }

    State state() const { return state_; }
    const uint8_t* data() const { return buf_; }
    size_t size() const { return len_; }
    uint8_t fileId() const { return fileId_; }

    static uint32_t crc32(const uint8_t* p, size_t n);

private:
    Reaction reply(uint8_t status) const { return Reaction{Reaction::Reply, status, fileId_}; }
    void fail(uint8_t status, Reaction& r) { state_ = State::Idle; r = reply(status); }

    State state_ = State::Idle;
    uint8_t buf_[kMaxFile + 1] = {0};  // +1: always NUL terminated, so text helpers are safe
    size_t len_ = 0, got_ = 0;
    uint32_t crc_ = 0, lastMs_ = 0;
    uint8_t seq_ = 0, fileId_ = 0;
};
