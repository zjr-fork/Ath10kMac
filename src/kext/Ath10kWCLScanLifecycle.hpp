#pragma once

#include <stdint.h>

/*
 * Per-interface WCL notification guard. It owns no timer or IOKit object:
 * callers serialize every method on the interface command gate and translate
 * the returned terminal value to the appropriate Apple IOReturn/message.
 */
class Ath10kWCLScanLifecycle {
public:
    enum Terminal : uint8_t {
        kNone = 0,
        kSucceeded = 1,
        kAborted = 2,
        kTimedOut = 3,
        kFailed = 4
    };

    Ath10kWCLScanLifecycle()
        : _generation(0), _pending(false), _terminal(kNone) {}

    static bool isNewerGeneration(uint32_t candidate, uint32_t current) {
        return candidate && static_cast<int32_t>(candidate - current) > 0;
    }

    bool begin(uint32_t generation) {
        if (!generation || _pending ||
            (_generation && !isNewerGeneration(generation, _generation)))
            return false;
        _generation = generation;
        _pending = true;
        _terminal = kNone;
        return true;
    }

    bool acceptsResult(uint32_t generation) const {
        return generation && generation == _generation && _pending &&
               _terminal == kNone;
    }

    bool finish(uint32_t generation, Terminal terminal) {
        if (!generation || generation != _generation || !_pending ||
            _terminal != kNone || terminal == kNone)
            return false;
        _terminal = terminal;
        _pending = false;
        return true;
    }

    void reset() {
        // Preserve the generation high-water mark so delayed callbacks from
        // a stopped scan cannot be confused with a reused generation.
        _pending = false;
        _terminal = kNone;
    }

    uint32_t generation() const { return _generation; }
    bool pending() const { return _pending; }
    Terminal terminal() const { return _terminal; }

private:
    uint32_t _generation;
    bool _pending;
    Terminal _terminal;
};
