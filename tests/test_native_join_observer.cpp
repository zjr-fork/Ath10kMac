#include "../src/kext/Ath10kNativeJoinObserver.hpp"

static Ath10kNativeJoinSnapshot baseline(uint32_t beacon = 7,
                                          uint32_t security = 20) {
    Ath10kNativeJoinSnapshot s{};
    s.link_stage = ATH10K_JOIN_LINK_SCAN;
    s.station_beacon_serial = beacon;
    s.security_generation = security;
    return s;
}
static Ath10kNativeJoinSnapshot observation(uint32_t stage,
                                             uint32_t beacon,
                                             uint32_t security,
                                             bool secure,
                                             bool authorized,
                                             const uint8_t *bssid) {
    Ath10kNativeJoinSnapshot s{};
    s.link_stage = stage;
    s.station_beacon_serial = beacon;
    s.security_generation = security;
    s.secure = secure ? 1 : 0;
    s.port_authorized = authorized ? 1 : 0;
    for (uint32_t i = 0; i < 6; ++i) s.bssid[i] = bssid[i];
    return s;
}

int main() {
    const uint8_t ap[6] = {0x02,0x11,0x22,0x33,0x44,0x55};
    const uint8_t other[6] = {0x02,0xaa,0xbb,0xcc,0xdd,0xee};

    // Open network: a fresh selected-AP beacon may precede association completion.
    Ath10kNativeJoinObserver open;
    auto base = baseline();
    if (!open.begin(1, ap, false, base)) return 1;
    auto snap = observation(ATH10K_JOIN_LINK_ASSOC, 8, 20, false, false, ap);
    if (!open.observe(1, snap) || !open.beaconSeen()) return 2;
    snap = observation(ATH10K_JOIN_LINK_CONNECTED, 8, 20, false, true, ap);
    if (!open.observe(1, snap) || open.active() ||
        open.terminal() != Ath10kWCLJoinLifecycle::kSucceeded) return 3;

    // WPA2: association + beacon are insufficient until a newer security generation
    // and the controlled port are both visible in a core snapshot.
    Ath10kNativeJoinObserver secure;
    if (!secure.begin(10, ap, true, base)) return 4;
    snap = observation(ATH10K_JOIN_LINK_ASSOC, 8, 20, true, false, ap);
    if (!secure.observe(10, snap) || !secure.beaconSeen()) return 5;
    snap = observation(ATH10K_JOIN_LINK_CONNECTED, 8, 20, true, false, ap);
    if (!secure.observe(10, snap) || !secure.associationSeen() || !secure.active()) return 6;
    snap = observation(ATH10K_JOIN_LINK_CONNECTED, 8, 21, true, true, ap);
    if (!secure.observe(10, snap) || secure.active() ||
        secure.terminal() != Ath10kWCLJoinLifecycle::kSucceeded) return 7;

    // Stale pre-join beacons, wrong BSSIDs, and stale generations are ignored.
    Ath10kNativeJoinObserver guarded;
    if (!guarded.begin(20, ap, false, base)) return 8;
    snap = observation(ATH10K_JOIN_LINK_ASSOC, 7, 20, false, false, other);
    if (guarded.observe(20, snap) || guarded.beaconSeen()) return 9;
    snap = observation(ATH10K_JOIN_LINK_ASSOC, 8, 20, false, false, other);
    if (guarded.observe(20, snap) || guarded.beaconSeen()) return 10;
    snap = observation(ATH10K_JOIN_LINK_ASSOC, 8, 20, false, false, ap);
    if (guarded.observe(19, snap) || guarded.observe(20, snap) == false ||
        !guarded.beaconSeen()) return 11;

    // Failed and cancelled operations terminate exactly once.
    Ath10kNativeJoinObserver failed;
    if (!failed.begin(30, ap, true, base)) return 12;
    snap = observation(ATH10K_JOIN_LINK_FAILED, 8, 20, true, false, ap);
    if (!failed.observe(30, snap) ||
        failed.terminal() != Ath10kWCLJoinLifecycle::kFailed ||
        failed.active()) return 13;
    Ath10kNativeJoinObserver aborted;
    if (!aborted.begin(40, ap, false, base)) return 14;
    snap = observation(ATH10K_JOIN_LINK_ASSOC, 8, 20, false, false, ap);
    snap.cancel_requested = 1;
    if (!aborted.observe(40, snap) ||
        aborted.terminal() != Ath10kWCLJoinLifecycle::kAborted ||
        aborted.active()) return 15;

    // Serial wraparound is accepted; malformed snapshots and BSSIDs are rejected.
    Ath10kNativeJoinObserver wrap;
    base = baseline(0xffffffffu, 0xffffffffu);
    if (!wrap.begin(50, ap, true, base)) return 16;
    snap = observation(ATH10K_JOIN_LINK_ASSOC, 1, 0xffffffffu, true, false, ap);
    if (!wrap.observe(50, snap) || !wrap.beaconSeen()) return 17;
    snap = observation(ATH10K_JOIN_LINK_CONNECTED, 1, 0, true, true, ap);
    if (!wrap.observe(50, snap) ||
        wrap.terminal() != Ath10kWCLJoinLifecycle::kSucceeded) return 18;

    Ath10kNativeJoinObserver invalid;
    const uint8_t multicast[6] = {0x01,0,0,0,0,1};
    if (invalid.begin(60, multicast, false, baseline())) return 19;
    base = baseline();
    if (!invalid.begin(60, ap, false, base)) return 20;
    snap = observation(ATH10K_JOIN_LINK_CONNECTED, 8, 20, false, true, ap);
    snap.reserved = 1;
    if (invalid.observe(60, snap)) return 21;
    return 0;
}
