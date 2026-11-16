#include "../src/kext/Ath10kNativeScanObserver.hpp"

static Ath10kNativeScanStatus status(uint32_t generation, uint32_t state,
                                     uint32_t count = 0) {
    Ath10kNativeScanStatus s{};
    s.generation = generation;
    s.state = state;
    s.result_count = count;
    return s;
}

int main() {
    Ath10kNativeScanObserver scan;
    if (!scan.begin(10)) return 1;
    auto s = status(10, ATH10K_NATIVE_SCAN_PENDING, 3);
    if (!scan.observe(s) || !scan.active() ||
        scan.terminal() != Ath10kWCLScanLifecycle::kNone) return 2;
    if (scan.nextResult(10, nullptr) != Ath10kNativeScanCursor::kNotReady) return 3;
    s = status(9, ATH10K_NATIVE_SCAN_SUCCEEDED, 3);
    if (scan.observe(s) || !scan.active()) return 4;
    s = status(10, ATH10K_NATIVE_SCAN_SUCCEEDED, 3);
    if (!scan.observe(s) || scan.active() ||
        scan.terminal() != Ath10kWCLScanLifecycle::kSucceeded ||
        scan.resultCount() != 3) return 5;
    uint32_t index = 99;
    if (scan.nextResult(10, &index) != Ath10kNativeScanCursor::kRecord || index != 0) return 6;
    if (scan.nextResult(10, &index) != Ath10kNativeScanCursor::kRecord || index != 1) return 7;
    if (scan.nextResult(10, &index) != Ath10kNativeScanCursor::kRecord || index != 2) return 8;
    if (scan.nextResult(10, &index) != Ath10kNativeScanCursor::kEnd) return 9;
    if (scan.nextResult(11, &index) != Ath10kNativeScanCursor::kStaleGeneration) return 10;

    Ath10kNativeScanObserver aborted;
    if (!aborted.begin(20) || aborted.abort(19) || !aborted.abort(20)) return 11;
    if (aborted.active() || aborted.terminal() != Ath10kWCLScanLifecycle::kAborted ||
        aborted.nextResult(20, &index) != Ath10kNativeScanCursor::kNotReady) return 12;
    s = status(20, ATH10K_NATIVE_SCAN_SUCCEEDED, 2);
    if (aborted.observe(s)) return 13; // late success cannot resurrect an aborted scan

    Ath10kNativeScanObserver failed;
    if (!failed.begin(30)) return 14;
    s = status(30, ATH10K_NATIVE_SCAN_FAILED, 1);
    if (!failed.observe(s) || failed.terminal() != Ath10kWCLScanLifecycle::kFailed ||
        failed.nextResult(30, &index) != Ath10kNativeScanCursor::kNotReady) return 15;

    Ath10kNativeScanObserver timedOut;
    if (!timedOut.begin(40) || !timedOut.timeout(40) ||
        timedOut.terminal() != Ath10kWCLScanLifecycle::kTimedOut) return 16;

    Ath10kNativeScanObserver cancelled;
    if (!cancelled.begin(50)) return 17;
    s = status(50, ATH10K_NATIVE_SCAN_CANCELLED, 5);
    if (!cancelled.observe(s) ||
        cancelled.terminal() != Ath10kWCLScanLifecycle::kAborted) return 18;

    // Reset keeps the lifecycle high-water mark; an old generation is rejected.
    scan.reset();
    if (scan.begin(10) || !scan.begin(11)) return 19;
    s = status(11, ATH10K_NATIVE_SCAN_SUCCEEDED, 0);
    if (!scan.observe(s) || scan.nextResult(11, &index) != Ath10kNativeScanCursor::kEnd) return 20;
    return 0;
}
