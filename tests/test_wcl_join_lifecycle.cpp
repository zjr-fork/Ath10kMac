#include "../src/kext/Ath10kWCLJoinLifecycle.hpp"

int main() {
    Ath10kWCLJoinLifecycle state;

    if (state.begin(0)) return 1;
    if (!state.begin(100)) return 2;
    if (state.begin(101)) return 3;
    if (state.begin(99)) return 4;
    if (!state.accepts(100) || state.accepts(101)) return 5;

    // Success must not be reported on request acceptance or association alone.
    if (state.finish(100, Ath10kWCLJoinLifecycle::kSucceeded)) return 5;
    if (state.markFirstBeacon(100, true)) return 6;
    if (!state.markAssociation(100, true)) return 7;
    if (state.readyToComplete(100)) return 8;
    if (state.markFirstBeacon(100, false)) return 9;
    if (!state.markFirstBeacon(100, true)) return 10;
    if (state.readyToComplete(100)) return 11;

    // Duplicate events are rejected and cannot change the state.
    if (state.markAssociation(100, true)) return 12;
    if (state.markFirstBeacon(100, true)) return 13;
    if (!state.markPortAuthorized(100)) return 14;
    if (!state.readyToComplete(100)) return 15;
    if (!state.finish(100, Ath10kWCLJoinLifecycle::kSucceeded)) return 16;
    if (state.pending() || state.terminal() != Ath10kWCLJoinLifecycle::kSucceeded) return 17;
    if (state.markPortAuthorized(100) || state.finish(100, Ath10kWCLJoinLifecycle::kFailed)) return 18;

    // A later generation starts clean; old-generation callbacks are harmless.
    if (!state.begin(101)) return 17;
    if (state.accepts(100) || state.markFirstBeacon(100, true) || state.markPortAuthorized(100)) return 19;
    if (state.readyToComplete(101)) return 20;
    if (!state.markPortAuthorized(101)) return 21;
    if (!state.markFirstBeacon(101, true)) return 22;
    if (!state.markAssociation(101, true)) return 23;
    if (!state.readyToComplete(101)) return 24;
    if (!state.finish(101, Ath10kWCLJoinLifecycle::kSucceeded)) return 25;

    // A failed association is not silently converted to success.
    if (!state.begin(102)) return 25;
    if (!state.markAssociation(102, false)) return 26;
    if (state.markAssociation(102, true)) return 28;
    if (state.markFirstBeacon(102, true)) return 29;
    if (state.finish(102, Ath10kWCLJoinLifecycle::kTimedOut)) return 30;
    if (state.readyToComplete(102)) return 31;
    if (!state.finish(102, Ath10kWCLJoinLifecycle::kFailed)) return 32;

    // Abort/timeout terminal states are exactly once and generation-scoped.
    if (!state.begin(103)) return 33;
    if (!state.finish(103, Ath10kWCLJoinLifecycle::kAborted)) return 34;
    if (state.finish(103, Ath10kWCLJoinLifecycle::kTimedOut)) return 35;
    if (!state.begin(104)) return 36;
    if (!state.finish(104), Ath10kWCLJoinLifecycle::kTimedOut)) return 37;

    state.reset();
    if (state.pending() || state.generation() ||
        state.terminal() != Ath10kWCLJoinLifecycle::kNone) return 38;
    if (state.accepts(104)) return 39;
    return 0;
}
