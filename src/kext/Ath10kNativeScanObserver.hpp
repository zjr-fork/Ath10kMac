#pragma once

#include <stdint.h>
#include "Ath10kNativeScanCursor.hpp"
#include "Ath10kWCLScanLifecycle.hpp"
#include "../shared/Ath10kNativeBridge.h"

/*
 * Joins the core's generation-scoped scan status to the interface's result
 * cursor. Results become iterable only after the matching core generation
 * reaches SUCCEEDED. The owner serializes all methods on its interface gate.
 */
class Ath10kNativeScanObserver {
public:
    using Terminal = Ath10kWCLScanLifecycle::Terminal;
    using NextResult = Ath10kNativeScanCursor::NextResult;

    Ath10kNativeScanObserver() : _active(false), _generation(0) {}

    bool begin(uint32_t generation) {
        if (_active || !_lifecycle.begin(generation)) return false;
        _generation = generation;
        _active = true;
        _cursor.reset();
        return true;
    }

    bool observe(const Ath10kNativeScanStatus &status) {
        if (!_active || status.generation != _generation ||
            !_lifecycle.pending() || status.state > ATH10K_NATIVE_SCAN_TIMED_OUT)
            return false;

        switch (status.state) {
            case ATH10K_NATIVE_SCAN_PENDING:
                return true;
            case ATH10K_NATIVE_SCAN_SUCCEEDED:
                if (!_lifecycle.finish(_generation,
                                       Ath10kWCLScanLifecycle::kSucceeded))
                    return false;
                if (!_cursor.begin(_generation, status.state, status.result_count)) {
                    _lifecycle.reset();
                    _active = false;
                    return false;
                }
                _active = false;
                return true;
            case ATH10K_NATIVE_SCAN_CANCELLED:
                return finish(Ath10kWCLScanLifecycle::kAborted);
            case ATH10K_NATIVE_SCAN_TIMED_OUT:
                return finish(Ath10kWCLScanLifecycle::kTimedOut);
            case ATH10K_NATIVE_SCAN_FAILED:
                return finish(Ath10kWCLScanLifecycle::kFailed);
            default:
                return false;
        }
    }

    bool abort(uint32_t generation) {
        if (!_active || generation != _generation ||
            !_lifecycle.acceptsResult(generation))
            return false;
        return finish(Ath10kWCLScanLifecycle::kAborted);
    }

    bool timeout(uint32_t generation) {
        if (!_active || generation != _generation ||
            !_lifecycle.acceptsResult(generation))
            return false;
        return finish(Ath10kWCLScanLifecycle::kTimedOut);
    }

    NextResult nextResult(uint32_t generation, uint32_t *indexOut) {
        if (generation != _generation || !_generation)
            return Ath10kNativeScanCursor::kStaleGeneration;
        if (_lifecycle.terminal() != Ath10kWCLScanLifecycle::kSucceeded)
            return Ath10kNativeScanCursor::kNotReady;
        return _cursor.next(generation, indexOut);
    }

    void reset() {
        _lifecycle.reset();
        _cursor.reset();
        _active = false;
        _generation = 0;
    }

    bool active() const { return _active; }
    uint32_t generation() const { return _generation; }
    Terminal terminal() const { return _lifecycle.terminal(); }
    uint32_t resultCount() const { return _cursor.count(); }
    uint32_t resultPosition() const { return _cursor.position(); }

private:
    bool finish(Terminal terminal) {
        if (!_lifecycle.finish(_generation, terminal)) return false;
        _cursor.reset();
        _active = false;
        return true;
    }

    Ath10kWCLScanLifecycle _lifecycle;
    Ath10kNativeScanCursor _cursor;
    bool _active;
    uint32_t _generation;
};
