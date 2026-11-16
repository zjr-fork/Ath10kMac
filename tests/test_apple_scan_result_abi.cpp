#include <stddef.h>
#include <stdint.h>
#include <IOKit/80211/apple80211_var.h>
#include "../src/kext/Ath10kAppleScanResult.hpp"

static_assert(sizeof(void *) == 8, "This ABI probe targets x86_64");
static_assert(offsetof(apple80211_scan_result, version) == 0x00, "version offset");
static_assert(offsetof(apple80211_scan_result, asr_channel) == 0x04, "channel offset");
static_assert(offsetof(apple80211_scan_result, asr_noise) == 0x12, "noise offset");
static_assert(offsetof(apple80211_scan_result, asr_snr) == 0x14, "SNR offset");
static_assert(offsetof(apple80211_scan_result, asr_rssi) == 0x16, "RSSI offset");
static_assert(offsetof(apple80211_scan_result, asr_beacon_int) == 0x18, "beacon interval offset");
static_assert(offsetof(apple80211_scan_result, asr_cap) == 0x1a, "capability offset");
static_assert(offsetof(apple80211_scan_result, asr_bssid) == 0x1c, "BSSID offset");
static_assert(offsetof(apple80211_scan_result, asr_nrates) == 0x22, "rate count offset");
static_assert(offsetof(apple80211_scan_result, asr_rates) == 0x24, "rates offset");
static_assert(sizeof(((apple80211_scan_result *)0)->asr_rates) / sizeof(uint32_t) == 15,
              "Apple scan result rate capacity");
static_assert(offsetof(apple80211_scan_result, asr_ssid_len) == 0x60, "SSID length offset");
static_assert(offsetof(apple80211_scan_result, asr_ssid) == 0x61, "SSID offset");
static_assert(offsetof(apple80211_scan_result, asr_age) == 0x84, "age offset");
static_assert(offsetof(apple80211_scan_result, asr_ie_len) == 0x8a, "IE length offset");
static_assert(offsetof(apple80211_scan_result, asr_ie_data) == 0x90, "IE pointer offset");
static_assert(sizeof(apple80211_scan_result) == 0x98, "scan result size");

int main() { return 0; }
