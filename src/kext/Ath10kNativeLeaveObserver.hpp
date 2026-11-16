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

    Ath10kNativeLeaveObserver() : _active(false), _generation(0) {}

    bool begin(uint32_t generation, const Ath10kNativeJoinSnapshot &baseline) {
        if (_active || !generation ||
            baseline.link_stage > 11 || baseline.disconnect_stage > 7 ||
            baseline.secure > 1 || baseline.port_authorized > 1 ||
            baseline.cancel_requested > 1 || baseline.reserved != 0 ||
            baseline.link_stage == 0)
            return false;
        _generation = generation;
        _active = true;
        return true;
    }

    bool observe(uint32_t generation, const Ath10kNativeJoinSnapshot &snapshot,
                 Result &result) {
        if (!_active || !generation || generation != _generation ||
            snapshot.link_stage > 11 || snapshot.disconnect_stage > 7 ||
            snapshot.secure > 1 || snapshot.port_authorized > 1 ||
            snapshot.cancel_requested > 1 || snapshot.reserved != 0)
            return false;
        result = kPending;
        // A Leave completion is not the request being accepted, nor the
        // controlled port being closed. Wait for the core's final reset after
        // peer/key removal, vdev stop and the disconnect ECHO barrier.
        if (snapshot.link_stage == 0 && snapshot.disconnect_stage == 0 &&
            !snapshot.secure && !snapshot.port_authorized) {
            _active = false;
            result = kSucceeded;
        }
        return true;
    }

    void reset() { _active = false; _generation = 0; }
    bool active() const { return _active; }
    uint32_t generation() const { return _generation; }

private:
    bool _active;
    uint32_t _generation;
};
