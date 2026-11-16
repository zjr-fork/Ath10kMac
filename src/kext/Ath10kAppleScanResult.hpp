#pragma once

#include <IOKit/IOReturn.h>
#include <string.h>
#include <stdint.h>
#include <IOKit/80211/apple80211_var.h>
#include "../shared/Ath10kNativeBridge.h"

/*
 * Apple80211 structure version. The public ioctl ABI defines this as 1;
 * apple80211_var.h does not itself export the macro.
 */
static constexpr uint32_t kAth10kApple80211Version = 1;

/*
 * Safe scalar-only adapter for the Sequoia Apple80211 scan-result ABI.
 *
 * The SDK exposes asr_ie_data as a caller-owned void pointer but does not
 * expose its capacity in apple80211_scan_result. Until the target caller's
 * allocation contract is known, this adapter clears the pointer and reports
 * no IE payload; it never reads or dereferences caller-owned IE storage.
 */
static inline IOReturn ath10kFillAppleScanResultScalars(
    apple80211_scan_result *result,
    const Ath10kNativeBSSRecord *record,
    uint32_t appleChannelFlags)
{
    if (!result || !record)
        return kIOReturnBadArgument;

    memset(result, 0, sizeof(*result));
    result->asr_ie_data = nullptr;
    result->asr_ie_len = 0;

    result->version = kAth10kApple80211Version;
    result->asr_channel.version = kAth10kApple80211Version;
    result->asr_channel.channel = record->channel;
    result->asr_channel.flags = appleChannelFlags;
    result->asr_noise = record->noise_dbm;
    result->asr_rssi = record->rssi_dbm;
    result->asr_beacon_int = record->beacon_interval;
    result->asr_cap = record->capabilities;
    for (uint32_t i = 0; i < sizeof(result->asr_bssid); ++i)
        result->asr_bssid[i] = record->bssid[i];

    uint32_t rateCount = record->rate_count;
    const uint32_t maxRates =
        (uint32_t)(sizeof(result->asr_rates) / sizeof(result->asr_rates[0]));
    if (rateCount > maxRates)
        rateCount = maxRates;
    for (uint32_t i = 0; i < rateCount; ++i)
        result->asr_rates[i] = record->rates[i];
    result->asr_nrates = (uint8_t)rateCount;

    uint32_t ssidLength = record->ssid_length;
    if (ssidLength > sizeof(result->asr_ssid))
        ssidLength = sizeof(result->asr_ssid);
    result->asr_ssid_len = (uint8_t)ssidLength;
    for (uint32_t i = 0; i < ssidLength; ++i)
        result->asr_ssid[i] = record->ssid[i];

    /* asr_age is intentionally zero: the core snapshot has no age clock. */
    return kIOReturnSuccess;
}
