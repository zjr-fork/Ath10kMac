#include "../src/kext/Ath10kWCLJoinLifecycle.hpp"

int main() {
    Ath10kWCLJoinLifecycle state;

    if (state.begin(0)) return 1;
    if (!state.begin(100)) return 2;
    if (state.begin(101) || state.begin(99)) return 3;
    if (!state.accepts(100) || state.accepts(101)) return 4;

    // A post-begin beacon may precede the association-complete event.
    if (state.finish(100, Ath10kWCLJoinLifecycle::kSucceeded)) return 5;
    if (state.markFirstBeacon(100, true, false)) return 6;
    if (state.markFirstBeacon(100, false, true)) return 7;
    if (!state.markFirstBeacon(100, true, true)) return 8;
    if (state.readyToComplete(100)) return 9;
    if (!state.markAssociation(100, true)) return 10;
    if (state.readyToComplete(100)) return 11;

    // Authorization may arrive later; duplicates and wrong BSSIDs are ignored.
    if (state.markAssociation(100, true)) return 12;
    if (state.markFirstBeacon(100, true, true)) return 13;
    if (!state.markPortAuthorized(100)) return 14;
    if (!state.readyToComplete(100)) return 15;
    if (!state.finish(100, Ath10kWCLJoinLifecycle::kSucceeded)) return 16;
    if (state.pending() || state.terminal() != Ath10kWCLJoinLifecycle::kSucceeded) return 17;
    if (state.markPortAuthorized(100) || state.finish(100, Ath10kWCLJoinLifecycle::kFailed)) return 18;

    // Old-generation callbacks and non-monotonic generations cannot affect a new join.
    if (!state.begin(101)) return 19;
    if (state.begin(100) || state.accepts(100)) return 20;
    if (state.markFirstBeacon(100, true, true) || state.markPortAuthorized(100)) return 21;
    if (state.readyToComplete(101)) return 22;
    if (!state.markAssociation(101, true)) return 23;
    if (!state.markFirstBeacon(101, true, true)) return 24;
    if (!state.markPortAuthorized(101)) return 25;
    if (!state.readyToComplete(101)) return 26;
    if (!state.finish(101, Ath10kWCLJoinLifecycle::kSucceeded)) return 27;

    // A failed association is latched and cannot be upgraded or timed out.
    if (!state.begin(102)) return 28;
    if (!state.markAssociation(102, false)) return 29;
    if (state.markAssociation(102, true)) return 30;
    if (!state.markFirstBeacon(102, true, true)) return 31;
    if (state.finish(102, Ath10kWCLJoinLifecycle::kTimedOut)) return 32;
    if (state.readyToComplete(102)) return 33;
    if (!state.finish(102, Ath10kWCLJoinLifecycle::kFailed)) return 34;

    // Abort/timeout terminal states are exactly once and generation-scoped.
    if (!state.begin(103)) return 35;
    if (!state.finish(103, Ath10kWCLJoinLifecycle::kAborted)) return 36;
    if (state.finish(103, Ath10kWCLJoinLifecycle::kTimedOut)) return 37;
    if (!state.begin(104)) return 38;
    if (!state.finish(104, Ath10kWCLJoinLifecycle::kTimedOut)) return 39;

    // Generation wrap skips zero and remains monotonic under serial arithmetic.
    Ath10kWCLJoinLifecycle wrap;
    if (!wrap.begin(0xffffffffu)) return 40;
    if (!wrap.finish(0xffffffffu, Ath10kWCLJoinLifecycle::kAborted)) return 41;
    if (!wrap.begin(1)) return 42;
    if (wrap.begin(0x80000001u)) return 43;

    // Reset invalidates callbacks but preserves the generation high-water mark.
    state.reset();
    if (state.pending() || state.generation() != 104 ||
        state.terminal() != Ath10kWCLJoinLifecycle::kNone) return 44;
    if (state.accepts(104) || state.begin(104)) return 45;
    if (!state.begin(105)) return 46;
    return 0;
}
