#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../shared/Ath10kNativeBridge.h"

/*
 * macOS 15.2 WCL ScanResult payload prefix. This is an independently declared
 * wire-layout adapter based on observed field offsets, not a copied SDK/header.
 * The payload is BeaconMetadata followed immediately by the validated IE bytes.
 */
struct Ath10kWCLBeaconMetadata {
    uint32_t ieLength;
    uint16_t channelSpec;
    uint8_t ssid[32];
    uint8_t ssidLength;
    uint8_t channel;
    uint8_t reserved0;
    uint8_t bssid[6];
    uint8_t reserved1;
    int32_t rssi;
    int16_t noise;
    int16_t snr;
    uint16_t interval;
    uint16_t capability;
    uint32_t flags;
};

static_assert(sizeof(Ath10kWCLBeaconMetadata) == 64, "Sequoia WCL BeaconMetadata size");
static_assert(offsetof(Ath10kWCLBeaconMetadata, rssi) == 48, "WCL RSSI offset");
static_assert(offsetof(Ath10kWCLBeaconMetadata, flags) == 60, "WCL flags offset");

enum : uint32_t {
    ATH10K_WCL_MESSAGE_SCAN_RESULT = 201,
    ATH10K_WCL_MESSAGE_SCAN_DONE = 237,
    ATH10K_WCL_BEACON_SSID_PRESENT = 1u << 1,
    ATH10K_WCL_BEACON_RSSI_VALID = 1u << 14
};

inline bool ath10kWCLValidIEs(const uint8_t *ies, uint32_t length) {
    if (length && !ies) return false;
    uint32_t offset = 0;
    while (offset < length) {
        if (length - offset < 2) return false;
        uint32_t elementLength = uint32_t(ies[offset + 1]) + 2u;
        if (elementLength > length - offset) return false;
        offset += elementLength;
    }
    return true;
}

/*
 * QCA9377 is a 2.4/5 GHz device. Use the captured frequency, not just the
 * channel number, to select the WCL band bits; reject unknown or inconsistent
 * channel/frequency pairs instead of publishing a guessed channel spec.
 */
inline bool ath10kWCLChannelSpec(const Ath10kNativeBSSRecord *record,
                                uint16_t *channelSpec) {
    if (!record || !channelSpec || !record->channel ||
        record->channel > 196 || !record->frequency_mhz)
        return false;
    if (record->frequency_mhz >= 2400 && record->frequency_mhz <= 2500 &&
        record->channel <= 14) {
        *channelSpec = static_cast<uint16_t>(record->channel | 0x1000u);
        return true;
    }
    if (record->frequency_mhz >= 4900 && record->frequency_mhz <= 5900 &&
        record->channel > 14) {
        *channelSpec = static_cast<uint16_t>(record->channel | 0xd000u);
        return true;
    }
    return false;
}

/*
 * Returns the exact payload byte count on success, zero on invalid input or
 * insufficient capacity. No partial payload is emitted. The caller owns the
 * destination and must send it only after validating the associated scan
 * generation and terminal/event policy.
 */
inline uint32_t ath10kBuildWCLScanResult(const Ath10kNativeBSSRecord *record,
                                        uint8_t *destination,
                                        uint32_t capacity) {
    if (!record || !destination ||
        record->ssid_length > sizeof(record->ssid) ||
        record->ie_length > sizeof(record->ies) ||
        !ath10kWCLValidIEs(record->ies, record->ie_length))
        return 0;

    uint16_t channelSpec = 0;
    if (!ath10kWCLChannelSpec(record, &channelSpec)) return 0;

    const uint32_t total = uint32_t(sizeof(Ath10kWCLBeaconMetadata)) + record->ie_length;
    if (capacity < total) return 0;

    Ath10kWCLBeaconMetadata metadata{};
    metadata.ieLength = record->ie_length;
    metadata.channel = static_cast<uint8_t>(record->channel);
    metadata.channelSpec = channelSpec;
    metadata.ssidLength = record->ssid_length;
    memcpy(metadata.ssid, record->ssid, record->ssid_length);
    memcpy(metadata.bssid, record->bssid, sizeof(metadata.bssid));
    metadata.rssi = record->rssi_dbm;
    metadata.noise = record->noise_dbm;
    metadata.interval = record->beacon_interval;
    metadata.capability = record->capabilities;
    /* The Sequoia WCL publisher sets SSIDPresent even for hidden SSIDs. */
    metadata.flags |= ATH10K_WCL_BEACON_SSID_PRESENT;
    /* Estimated RSSI is still a usable RSSI measurement for WCL. The native
     * record uses -128 only for unavailable RSSI; do not invert the estimated
     * flag into an RSSI-validity test. */
    if (record->rssi_dbm != INT8_MIN)
        metadata.flags |= ATH10K_WCL_BEACON_RSSI_VALID;

    memcpy(destination, &metadata, sizeof(metadata));
    if (record->ie_length)
        memcpy(destination + sizeof(metadata), record->ies, record->ie_length);
    return total;
}
