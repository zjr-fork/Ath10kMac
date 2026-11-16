#include "../src/kext/Ath10kWCLScanLifecycle.hpp"

int main() {
    Ath10kWCLScanLifecycle state;
    if (state.begin(0)) return 1;
    if (!state.begin(41)) return 2;
    if (state.begin(42) || state.begin(40)) return 3;
    if (!state.acceptsResult(41) || state.acceptsResult(42)) return 4;
    if (state.finish(42, Ath10kWCLScanLifecycle::kSucceeded)) return 5;
    if (state.finish(41, Ath10kWCLScanLifecycle::kNone)) return 6;
    if (!state.finish(41, Ath10kWCLScanLifecycle::kAborted)) return 7;
    if (state.pending() || state.terminal() != Ath10kWCLScanLifecycle::kAborted) return 8;
    if (state.acceptsResult(41) || state.finish(41, Ath10kWCLScanLifecycle::kSucceeded)) return 9;
    if (!state.begin(42)) return 10;
    if (state.begin(41) || !state.acceptsResult(42)) return 11;
    if (!state.finish(42, Ath10kWCLScanLifecycle::kTimedOut)) return 12;
    if (state.finish(42, Ath10kWCLScanLifecycle::kFailed)) return 13;
    if (!state.begin(43)) return 14;
    if (!state.finish(43, Ath10kWCLScanLifecycle::kSucceeded)) return 15;
    if (!state.begin(44)) return 16;
    state.reset();
    if (state.pending() || state.generation() != 44 ||
        state.terminal() != Ath10kWCLScanLifecycle::kNone) return 17;
    if (state.acceptsResult(44) || state.begin(44)) return 18;
    if (!state.begin(45)) return 19;

    Ath10kWCLScanLifecycle wrap;
    if (!wrap.begin(0xffffffffu)) return 20;
    if (!wrap.finish(0xffffffffu, Ath10kWCLScanLifecycle::kAborted)) return 21;
    if (!wrap.begin(1)) return 22;
    return 0;
}
