#pragma once

#include <stdint.h>
#include "../shared/Ath10kNativeBridge.h"

/*
 * Waits for the PCI core's ordered disconnect transaction to finish before a
 * future WCL LeaveComplete publisher may notify the framework. The owner must
 * serialize all methods on the interface gate. This helper owns no IOKit
 * objects and retains no framework pointers.
 */
class Ath10kNativeLeaveObserver {
public:
    enum Result : uint8_t { kPending = 0, kSucceeded = 1 };
    enum : uint32_t {
        kDisabledLinkStage = 0,
        kFailedLinkStage = 11,
        kLastDisconnectStage = 7
    };

    Ath10kNativeLeaveObserver() : _active(false), _generation(0), _lastGeneration(0) {}

    bool begin(uint32_t generation, const Ath10kNativeJoinSnapshot &baseline) {
        if (_active || !generation ||
            (_lastGeneration && !isNewerGeneration(generation, _lastGeneration)) ||
            baseline.link_stage > kFailedLinkStage || baseline.disconnect_stage > kLastDisconnectStage ||
            baseline.secure > 1 || baseline.port_authorized > 1 ||
            baseline.cancel_requested > 1 || baseline.reserved != 0 ||
            baseline.link_stage == kDisabledLinkStage)
            return false;
        _generation = generation;
        _lastGeneration = generation;
        _active = true;
        return true;
    }

    bool observe(uint32_t generation, const Ath10kNativeJoinSnapshot &snapshot,
                 Result &result) {
        if (!_active || !generation || generation != _generation ||
            snapshot.link_stage > kFailedLinkStage || snapshot.disconnect_stage > kLastDisconnectStage ||
            snapshot.secure > 1 || snapshot.port_authorized > 1 ||
            snapshot.cancel_requested > 1 || snapshot.reserved != 0)
            return false;
        result = kPending;
        // A Leave completion is not the request being accepted, nor the
        // controlled port being closed. Wait for the core's final reset after
        // peer/key removal, vdev stop and the disconnect ECHO barrier.
        if (snapshot.link_stage == kDisabledLinkStage && snapshot.disconnect_stage == 0 &&
            !snapshot.secure && !snapshot.port_authorized && !snapshot.cancel_requested) {
            _active = false;
            result = kSucceeded;
        }
        return true;
    }

    void reset() { _active = false; _generation = 0; }
    bool active() const { return _active; }
    uint32_t generation() const { return _generation; }

private:
    static bool isNewerGeneration(uint32_t candidate, uint32_t current) {
        const uint32_t delta = candidate - current;
        return candidate != 0 && delta != 0 && delta < 0x80000000u;
    }

    bool _active;
    uint32_t _generation;
    uint32_t _lastGeneration;
};
