//
//  ConnectionCommandQueue.hpp
//
//  Lock-free SPSC ring buffer for deferring Module::inputs mutations
//  from the main thread to the audio render thread.
//
//  Producer: main/UI thread (VoiceManager::connectModules, ::clearInput)
//  Consumer: audio render thread (VoiceManager::process → drainCommandQueue)
//

#pragma once

#include <array>
#include <atomic>
#include "pm_compat.h"

// ---------------------------------------------------------------------------
// Command types
// ---------------------------------------------------------------------------

enum class VoiceCommandType : uint8_t {
    Connect,     // wire upstream → downstream for all polyphonic voices
    ClearInput,  // remove an input slot from a downstream module
};

struct VoiceCommand {
    VoiceCommandType    type;
    AUParameterAddress  upstreamID;    // base address of upstream module   (Connect only)
    AUParameterAddress  downstreamID;  // base address of downstream module (Connect only)
    AUParameterAddress  inputAddress;  // full input parameter address (both command types)
};

// ---------------------------------------------------------------------------
// SPSC ring buffer
//
// N must be a power of two.  With typical preset sizes (~5–20 connections)
// and a polyphonic voice pool of ≤32 voices, 256 slots is ample headroom.
// 1024 gives extra safety during rapid patch-editing sessions.
// ---------------------------------------------------------------------------

template <size_t N>
class ConnectionCommandQueue {
    static_assert((N & (N - 1)) == 0, "N must be a power of 2");

    std::array<VoiceCommand, N> buffer_;

    // writeIdx is written by the producer and read by the consumer.
    // readIdx is written by the consumer and read by the producer.
    alignas(64) std::atomic<size_t> writeIdx_{0};
    alignas(64) std::atomic<size_t> readIdx_{0};

public:
    ConnectionCommandQueue() = default;

    // Called from main thread only.
    // Returns false (and drops the command) if the queue is full.
    bool push(const VoiceCommand& cmd) noexcept {
        const size_t w    = writeIdx_.load(std::memory_order_relaxed);
        const size_t next = (w + 1) & (N - 1);
        if (next == readIdx_.load(std::memory_order_acquire)) {
            // Queue full — should never happen in normal use.
            return false;
        }
        buffer_[w] = cmd;
        writeIdx_.store(next, std::memory_order_release);
        return true;
    }

    // Called from render thread only.
    // Returns false when the queue is empty.
    bool pop(VoiceCommand& cmd) noexcept {
        const size_t r = readIdx_.load(std::memory_order_relaxed);
        if (r == writeIdx_.load(std::memory_order_acquire)) {
            return false; // empty
        }
        cmd = buffer_[r];
        readIdx_.store((r + 1) & (N - 1), std::memory_order_release);
        return true;
    }
};
