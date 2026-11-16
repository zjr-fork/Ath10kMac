#pragma once

#include <stdint.h>

/*
 * Pointer-free WCL join lifecycle state. The interface frontend serializes
 * every method on its command gate. This class owns no IOKit objects, timers,
 * firmware state, or borrowed Apple request pointers.
 *
 * A successful join notification is eligible only after three independent
 * facts have been observed for the same generation:
 *   1. firmware/MLME association completed successfully;
 *   2. the first beacon from the selected BSSID was received;
 *   3. the controlled port/key authorization barrier completed.
 *
 * Event order is deliberately unconstrained. The caller publishes the
 * terminal WCL completion only when readyToComplete() becomes true.
 */
class Ath10kWCLJoinLifecycle {
public:
    enum Terminal : uint8_t {
        kNone = 0,
        kSucceeded = 1,
        kAborted = 2,
        kTimedOut = 3,
        kFailed = 4
    };

    Ath10kWCLJoinLifecycle()
        : _generation(0), _pending(false), _associationReported(false),
          _associationSucceeded(false), _firstBeacon(false),
          _portAuthorized(false), _terminal(kNone) {}

    static bool isNewerGeneration(uint32_t candidate, uint32_t current) {
        return candidate && static_cast<int32_t>(candidate - current) > 0;
    }

    bool begin(uint32_t generation) {
        if (!generation || _pending ||
            (_generation && !isNewerGeneration(generation, _generation)))
            return false;
        _generation = generation;
        _pending = true;
        _associationReported = false;
        _associationSucceeded = false;
        _firstBeacon = false;
        _portAuthorized = false;
        _terminal = kNone;
        return true;
    }

    bool accepts(uint32_t generation) const {
        return generation && generation == _generation && _pending &&
               _terminal == kNone;
    }

    bool markAssociation(uint32_t generation, bool succeeded) {
        if (!accepts(generation) || _associationReported)
            return false;
        _associationReported = true;
        _associationSucceeded = succeeded;
        return true;
    }

    // The caller must compare the received BSSID with the selected peer.
    bool markFirstBeacon(uint32_t generation, bool matchesSelectedBSSID) {
        if (!matchesSelectedBSSID || !accepts(generation) ||
            !_associationSucceeded || _firstBeacon)
            return false;
        _firstBeacon = true;
        return true;
    }

    bool markPortAuthorized(uint32_t generation) {
        if (!accepts(generation) || _portAuthorized)
            return false;
        _portAuthorized = true;
        return true;
    }

    bool readyToComplete(uint32_t generation) const {
        return accepts(generation) && _associationSucceeded &&
               _firstBeacon && _portAuthorized;
    }

    bool finish(uint32_t generation, Terminal terminal) {
        if (!accepts(generation) || terminal == kNone)
            return false;
        if (terminal == kSucceeded && !readyToComplete(generation))
            return false;
        if (_associationReported && !_associationSucceeded &&
            terminal != kFailed)
            return false;
        _terminal = terminal;
        _pending = false;
        return true;
    }

    void reset() {
        _generation = 0;
        _pending = false;
        _associationReported = false;
        _associationSucceeded = false;
        _firstBeacon = false;
        _portAuthorized = false;
        _terminal = kNone;
    }

    uint32_t generation() const { return _generation; }
    bool pending() const { return _pending; }
    bool associationSucceeded() const { return _associationSucceeded; }
    bool firstBeaconReceived() const { return _firstBeacon; }
    bool portAuthorized() const { return _portAuthorized; }
    Terminal terminal() const { return _terminal; }

private:
    uint32_t _generation;
    bool _pending;
    bool _associationReported;
    bool _associationSucceeded;
    bool _firstBeacon;
    bool _portAuthorized;
    Terminal _terminal;
};
