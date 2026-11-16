#pragma once

#include <IOKit/IOReturn.h>
#include <string.h>
#include <IOKit/80211/apple80211_var.h>
#include "../shared/Ath10kNativeBridge.h"

/*
 * Safe scalar-only adapter for the Sequoia Apple80211 scan-result ABI.
 *
 * The SDK exposes asr_ie_data as a caller-owned void pointer but does not
 * expose its capacity in apple80211_scan_result. Until the target caller's
 * allocation contract is known, this adapter preserves the pointer without
 * dereferencing it and reports no IE payload.
 */
static inline IOReturn ath10kFillAppleScanResultScalars(
    apple80211_scan_result *result,
    const Ath10kNativeBSSRecord *record,
    UInt32 appleChannelFlags)
{
    if (!result || !record)
        return kIOReturnBadArgument;

    void *callerIEBuffer = result->asr_ie_data;
    memset(result, 0, sizeof(*result));
    result->asr_ie_data = callerIEBuffer;
    result->asr_ie_len = 0;

    result->version = APPLE80211_VERSION;
    result->asr_channel.version = APPLE80211_VERSION;
    result->asr_channel.channel = record->channel;
    result->asr_channel.flags = appleChannelFlags;
    result->asr_noise = record->noise_dbm;
    result->asr_rssi = record->rssi_dbm;
    result->asr_beacon_int = record->beacon_interval;
    result->asr_cap = record->capabilities;
    for (UInt32 i = 0; i < sizeof(result->asr_bssid); ++i)
        result->asr_bssid[i] = record->bssid[i];

    UInt32 rateCount = record->rate_count;
    const UInt32 maxRates =
        (UInt32)(sizeof(result->asr_rates) / sizeof(result->asr_rates[0]));
    if (rateCount > maxRates)
        rateCount = maxRates;
    for (UInt32 i = 0; i < rateCount; ++i)
        result->asr_rates[i] = record->rates[i];
    result->asr_nrates = (UInt8)rateCount;

    UInt32 ssidLength = record->ssid_length;
    if (ssidLength > sizeof(result->asr_ssid))
        ssidLength = sizeof(result->asr_ssid);
    result->asr_ssid_len = (UInt8)ssidLength;
    for (UInt32 i = 0; i < ssidLength; ++i)
        result->asr_ssid[i] = record->ssid[i];

    /* asr_age is intentionally zero: the core snapshot has no age clock. */
    return kIOReturnSuccess;
}
