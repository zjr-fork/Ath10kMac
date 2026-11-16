#include "../src/kext/Ath10kNativeLeaveObserver.hpp"
#include <assert.h>
#include <string.h>

static Ath10kNativeJoinSnapshot snapshot(uint32_t stage, uint32_t disconnect,
                                         bool secure, bool authorized,
                                         bool cancel) {
    Ath10kNativeJoinSnapshot s{};
    s.link_stage = stage;
    s.disconnect_stage = disconnect;
    s.secure = secure ? 1 : 0;
    s.port_authorized = authorized ? 1 : 0;
    s.cancel_requested = cancel ? 1 : 0;
    return s;
}

int main() {
    Ath10kNativeLeaveObserver observer;
    Ath10kNativeLeaveObserver::Result result = Ath10kNativeLeaveObserver::kPending;
    auto connected = snapshot(10, 0, true, true, false);
    assert(!observer.begin(0, connected));
    auto alreadyDisconnecting = snapshot(10, 1, false, false, false);
    assert(!observer.begin(7, alreadyDisconnecting));
    auto alreadyCancelled = snapshot(10, 0, false, false, true);
    assert(!observer.begin(7, alreadyCancelled));
    assert(observer.begin(7, connected));
    assert(observer.active() && observer.generation() == 7);

    // Stale generations and malformed snapshots cannot complete the Leave.
    auto idle = snapshot(0, 0, false, false, false);
    assert(!observer.observe(6, idle, result));
    auto malformed = idle;
    malformed.reserved = 1;
    assert(!observer.observe(7, malformed, result));
    malformed = idle;
    malformed.link_stage = 12;
    assert(!observer.observe(7, malformed, result));
    malformed = idle;
    malformed.disconnect_stage = 8;
    assert(!observer.observe(7, malformed, result));
    malformed = idle;
    malformed.port_authorized = 2;
    assert(!observer.observe(7, malformed, result));
    assert(observer.active());

    // Port closure is only an intermediate state; the core still owns teardown.
    auto draining = snapshot(10, 1, false, false, false);
    assert(observer.observe(7, draining, result));
    assert(result == Ath10kNativeLeaveObserver::kPending && observer.active());

    auto cancelled = snapshot(10, 1, false, false, true);
    assert(observer.observe(7, cancelled, result));
    assert(result == Ath10kNativeLeaveObserver::kPending && observer.active());

    // An otherwise-idle snapshot still cannot complete while cancellation is
    // asserted; the core must publish its fully reset state.
    auto resetting = snapshot(0, 0, false, false, true);
    assert(observer.observe(7, resetting, result));
    assert(result == Ath10kNativeLeaveObserver::kPending && observer.active());

    // Only the final Disabled + no-disconnect + unsecured + uncancelled snapshot completes.
    assert(observer.observe(7, idle, result));
    assert(result == Ath10kNativeLeaveObserver::kSucceeded && !observer.active());
    assert(!observer.observe(7, idle, result));

    // A new generation may begin after the previous operation is terminal.
    assert(observer.begin(8, connected));
    auto stillConnected = snapshot(10, 0, true, true, true);
    assert(observer.observe(8, stillConnected, result));
    assert(result == Ath10kNativeLeaveObserver::kPending && observer.active());
    observer.reset();
    assert(!observer.active() && observer.generation() == 0);
    // Reset must not erase the anti-replay high-water mark.
    assert(!observer.begin(8, connected));
    assert(!observer.begin(7, connected));
    assert(observer.begin(9, connected));
    observer.reset();
    // RFC-1982 wrap is accepted, while the ambiguous half-range is rejected.
    Ath10kNativeLeaveObserver wrapped;
    assert(wrapped.begin(0xfffffffeu, connected));
    wrapped.reset();
    assert(wrapped.begin(1u, connected));
    wrapped.reset();
    assert(!wrapped.begin(0x80000001u, connected));
    return 0;
}
