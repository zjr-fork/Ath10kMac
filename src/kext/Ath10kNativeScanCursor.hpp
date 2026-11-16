#pragma once

#include <stdint.h>
#include "../shared/Ath10kNativeBridge.h"

/*
 * Per-interface, generation-scoped iterator state for Apple scan results.
 *
 * This object owns no BSS pointers and performs no IOKit calls. The frontend
 * must call it under its command gate, then obtain each item through
 * Ath10kPCIDevice::copyNativeBSSRecord(). A new generation invalidates the
 * previous cursor; callers must never retain a copied record pointer.
 */
class Ath10kNativeScanCursor {
public:
    enum NextResult : uint8_t {
        kRecord = 0,
        kEnd = 1,
        kNotReady = 2,
        kStaleGeneration = 3
    };

    Ath10kNativeScanCursor() : _generation(0), _index(0), _count(0), _active(false) {}

    void reset() {
        _generation = 0;
        _index = 0;
        _count = 0;
        _active = false;
    }

    bool begin(uint32_t generation, uint32_t scanState, uint32_t resultCount) {
        reset();
        if (!generation || scanState != ATH10K_NATIVE_SCAN_SUCCEEDED)
            return false;
        _generation = generation;
        _count = resultCount;
        _active = true;
        return true;
    }

    NextResult next(uint32_t generation, uint32_t *indexOut) {
        if (!generation || generation != _generation)
            return kStaleGeneration;
        if (!_active || !indexOut)
            return kNotReady;
        if (_index >= _count)
            return kEnd;
        *indexOut = _index++;
        return kRecord;
    }

    bool active() const { return _active; }
    uint32_t generation() const { return _generation; }
    uint32_t count() const { return _count; }
    uint32_t position() const { return _index; }

private:
    uint32_t _generation;
    uint32_t _index;
    uint32_t _count;
    bool _active;
};
