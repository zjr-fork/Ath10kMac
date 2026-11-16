#pragma once

#include <stdint.h>
#include "Ath10kWCLJoinLifecycle.hpp"
#include "../shared/Ath10kNativeBridge.h"

/*
 * Converts pointer-free core snapshots into generation-scoped WCL join facts.
 * The owner must serialize begin()/observe()/reset() on the interface gate.
 * This helper owns no IOKit objects and retains no framework pointers.
 */
enum Ath10kNativeJoinLinkStage : uint32_t {
    ATH10K_JOIN_LINK_DISABLED = 0,
    ATH10K_JOIN_LINK_SCAN = 1,
    ATH10K_JOIN_LINK_START = 2,
    ATH10K_JOIN_LINK_PEER = 3,
    ATH10K_JOIN_LINK_AUTH = 4,
    ATH10K_JOIN_LINK_ASSOC = 5,
    ATH10K_JOIN_LINK_PEER_ASSOC = 6,
    ATH10K_JOIN_LINK_AUTHORIZE = 7,
    ATH10K_JOIN_LINK_UP = 8,
    ATH10K_JOIN_LINK_BARRIER = 9,
    ATH10K_JOIN_LINK_CONNECTED = 10,
    ATH10K_JOIN_LINK_FAILED = 11
};

class Ath10kNativeJoinObserver {
public:
    using Terminal = Ath10kWCLJoinLifecycle::Terminal;

    Ath10kNativeJoinObserver()
        : _active(false), _secure(false), _coreStarted(false),
          _associationSeen(false), _beaconSeen(false), _authorizationSeen(false),
          _baselineBeacon(0), _baselineSecurity(0), _generation(0) {
        for (uint32_t i = 0; i < 6; ++i) _bssid[i] = 0;
    }

    bool begin(uint32_t generation, const uint8_t selectedBSSID[6],
               bool secure, const Ath10kNativeJoinSnapshot &baseline) {
        if (_active || !generation || !validBSSID(selectedBSSID) ||
            baseline.secure > 1 || baseline.port_authorized > 1 ||
            baseline.cancel_requested > 1 || baseline.reserved != 0 ||
            baseline.link_stage > ATH10K_JOIN_LINK_SCAN ||
            baseline.disconnect_stage != 0 || baseline.cancel_requested ||
            baseline.port_authorized || baseline.secure)
            return false;
        if (!_lifecycle.begin(generation)) return false;
        _generation = generation;
        _secure = secure;
        _baselineBeacon = baseline.station_beacon_serial;
        _baselineSecurity = baseline.security_generation;
        _coreStarted = false;
        _associationSeen = _beaconSeen = _authorizationSeen = false;
        for (uint32_t i = 0; i < 6; ++i) _bssid[i] = selectedBSSID[i];
        _active = true;
        return true;
    }

    bool observe(uint32_t generation, const Ath10kNativeJoinSnapshot &snapshot) {
        if (!_active || generation != _generation ||
            !_lifecycle.accepts(generation) ||
            snapshot.link_stage > ATH10K_JOIN_LINK_FAILED ||
            snapshot.disconnect_stage > 7 ||
            snapshot.secure > 1 || snapshot.port_authorized > 1 ||
            snapshot.cancel_requested > 1 || snapshot.reserved != 0)
            return false;

        if (snapshot.cancel_requested || snapshot.disconnect_stage != 0) {
            const bool finished = _lifecycle.finish(generation,
                Ath10kWCLJoinLifecycle::kAborted);
            if (finished) _active = false;
            return finished;
        }
        if (snapshot.link_stage >= ATH10K_JOIN_LINK_START)
            _coreStarted = true;
        else if (_coreStarted) {
            const bool finished = _lifecycle.finish(generation,
                Ath10kWCLJoinLifecycle::kAborted);
            if (finished) _active = false;
            return finished;
        }
        if (snapshot.link_stage == ATH10K_JOIN_LINK_FAILED) {
            if (!_associationSeen) {
                _lifecycle.markAssociation(generation, false);
                _associationSeen = true;
            }
            const bool finished = _lifecycle.finish(generation,
                Ath10kWCLJoinLifecycle::kFailed);
            if (finished) _active = false;
            return finished;
        }
        if ((snapshot.link_stage >= ATH10K_JOIN_LINK_START) &&
            !sameBSSID(snapshot.bssid, _bssid))
            return false;
        if ((snapshot.link_stage >= ATH10K_JOIN_LINK_START) &&
            (snapshot.secure != (_secure ? 1u : 0u))) {
            const bool failed = _lifecycle.finish(generation,
                Ath10kWCLJoinLifecycle::kFailed);
            if (failed) _active = false;
            return failed;
        }

        if (!_beaconSeen &&
            Ath10kWCLJoinLifecycle::isNewerSerial(
                snapshot.station_beacon_serial, _baselineBeacon) &&
            sameBSSID(snapshot.bssid, _bssid)) {
            _beaconSeen = _lifecycle.markFirstBeacon(
                generation, true, snapshot.station_beacon_serial, _baselineBeacon);
        }

        if (!_associationSeen && snapshot.link_stage == ATH10K_JOIN_LINK_CONNECTED) {
            _associationSeen = true;
            if (snapshot.association_status != 0) {
                _lifecycle.markAssociation(generation, false);
                const bool failed = _lifecycle.finish(generation,
                    Ath10kWCLJoinLifecycle::kFailed);
                if (failed) _active = false;
                return failed;
            }
            _lifecycle.markAssociation(generation, true);
        }

        const bool securityBarrier = !_secure ||
            Ath10kWCLJoinLifecycle::isNewerSerial(
                snapshot.security_generation, _baselineSecurity);
        if (!_authorizationSeen && snapshot.port_authorized && securityBarrier)
            _authorizationSeen = _lifecycle.markPortAuthorized(generation);

        if (_lifecycle.readyToComplete(generation)) {
            const bool finished = _lifecycle.finish(generation,
                Ath10kWCLJoinLifecycle::kSucceeded);
            if (finished) _active = false;
            return finished;
        }
        return true;
    }

    void reset() {
        _lifecycle.reset();
        _active = false;
        _coreStarted = false;
        _associationSeen = _beaconSeen = _authorizationSeen = false;
        _generation = 0;
        _baselineBeacon = _baselineSecurity = 0;
        _secure = false;
        for (uint32_t i = 0; i < 6; ++i) _bssid[i] = 0;
    }

    bool active() const { return _active; }
    uint32_t generation() const { return _generation; }
    Terminal terminal() const { return _lifecycle.terminal(); }
    bool beaconSeen() const { return _beaconSeen; }
    bool associationSeen() const { return _associationSeen; }
    bool authorizationSeen() const { return _authorizationSeen; }

private:
    static bool validBSSID(const uint8_t *bssid) {
        if (!bssid || (bssid[0] & 1u)) return false;
        uint8_t any = 0;
        for (uint32_t i = 0; i < 6; ++i) any |= bssid[i];
        return any != 0;
    }

    static bool sameBSSID(const uint8_t *a, const uint8_t *b) {
        for (uint32_t i = 0; i < 6; ++i)
            if (a[i] != b[i]) return false;
        return true;
    }

    Ath10kWCLJoinLifecycle _lifecycle;
    bool _active;
    bool _secure;
    bool _coreStarted;
    bool _associationSeen;
    bool _beaconSeen;
    bool _authorizationSeen;
    uint32_t _baselineBeacon;
    uint32_t _baselineSecurity;
    uint32_t _generation;
    uint8_t _bssid[6];
};
