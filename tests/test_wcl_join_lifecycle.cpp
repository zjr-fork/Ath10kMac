#include "../src/kext/Ath10kWCLJoinLifecycle.hpp"

int main() {
    Ath10kWCLJoinLifecycle state;

    if (state.begin(0)) return 1;
    if (!state.begin(100)) return 2;
    if (state.begin(101)) return 3;
    if (!state.accepts(100) || state.accepts(101)) return 4;

    // Success must not be reported on request acceptance or association alone.
    if (state.finish(100, Ath10kWCLJoinLifecycle::kSucceeded)) return 5;
    if (!state.markAssociation(100, true)) return 6;
    if (state.readyToComplete(100)) return 7;
    if (state.markFirstBeacon(100, false)) return 8;
    if (!state.markFirstBeacon(100, true)) return 9;
    if (state.readyToComplete(100)) return 10;

    // Duplicate events are rejected and cannot change the state.
    if (state.markAssociation(100, true)) return 11;
    if (state.markFirstBeacon(100, true)) return 12;
    if (!state.markPortAuthorized(100)) return 13;
    if (!state.readyToComplete(100)) return 14;
    if (!state.finish(100, Ath10kWCLJoinLifecycle::kSucceeded)) return 15;
    if (state.pending() || state.terminal() != Ath10kWCLJoinLifecycle::kSucceeded) return 16;
    if (state.markPortAuthorized(100) || state.finish(100, Ath10kWCLJoinLifecycle::kFailed)) return 17;

    // A later generation starts clean; old-generation callbacks are harmless.
    if (!state.begin(101)) return 17;
    if (state.accepts(100) || state.markFirstBeacon(100, true) || state.markPortAuthorized(100)) return 18;
    if (state.readyToComplete(101)) return 19;
    if (!state.markPortAuthorized(101)) return 20;
    if (!state.markFirstBeacon(101, true)) return 21;
    if (!state.markAssociation(101, true)) return 22;
    if (!state.readyToComplete(101)) return 23;
    if (!state.finish(101, Ath10kWCLJoinLifecycle::kSucceeded)) return 24;

    // A failed association is not silently converted to success.
    if (!state.begin(102)) return 25;
    if (!state.markAssociation(102, false)) return 26;
    if (state.markAssociation(102, true)) return 27;
    if (state.readyToComplete(102)) return 28;
    if (!state.finish(102, Ath10kWCLJoinLifecycle::kFailed)) return 29;

    // Abort/timeout terminal states are exactly once and generation-scoped.
    if (!state.begin(103)) return 30;
    if (!state.finish(103, Ath10kWCLJoinLifecycle::kAborted)) return 31;
    if (state.finish(103, Ath10kWCLJoinLifecycle::kTimedOut)) return 32;
    if (!state.begin(104)) return 33;
    if (!state.finish(104, Ath10kWCLJoinLifecycle::kTimedOut)) return 34;

    state.reset();
    if (state.pending() || state.generation() ||
        state.terminal() != Ath10kWCLJoinLifecycle::kNone) return 35;
    if (state.accepts(104)) return 36;
    return 0;
}
