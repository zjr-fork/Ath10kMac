# Sequoia WCL scan request adapter contract

Status: the pointer-free decoder and WMI queue/abort entry points are implemented. The concrete `IO80211InfraProtocol` subclass and result publisher are still pending.

## Reference and licensing

- Reference repository: `laobamac/itlwm`
- Revision: `7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e`
- Sequoia header target: `__IO80211_TARGET=__MAC_15_2`
- The reference is GPL-2.0-only. This document records ABI observations; do not copy its WCL implementation or headers into Ath10kMac (GPL-3.0).

## Observed request layout

The pinned WCL adapter decodes an `apple80211ScanRequest` from a 0x1550-byte allocation. The fixed prefix is 0x58 bytes; the channel array follows as 12-byte `apple80211_channel` records.

| Offset | Field | Encoding |
|---:|---|---|
| 0x00 | request identifier | LE32 |
| 0x14 | BSSID | 6 bytes |
| 0x1c | SSID length | LE32, 0..32 |
| 0x20 | SSID | 32 raw bytes |
| 0x40 | scan type | LE32, values 1..4 |
| 0x54 | channel entry count | LE32, at most 400 |
| 0x58 | channel entries | 12 bytes each: version, channel, flags (LE32 each) |

Observed scan-type semantics: type 2 is passive; types 1, 3 and 4 request active scanning. An all-zero or broadcast BSSID is treated as no BSSID filter. An empty SSID is a wildcard request.

Channel entry filtering excludes channel zero, channels above 196, 6 GHz entries (flag 0x2000), and entries whose 2.4/5 GHz band flags conflict with the channel number. The QCA9377 plan and current regulatory policy are applied again by the core before WMI serialization.

## Independent bridge decoder

`ath10kNativeDecodeAppleScanRequest()` in `src/shared/Ath10kNativeBridge.h` implements this fixed-layout decode into ABI v1:

- Validates the request prefix, type, SSID length, channel count and complete channel-array bounds.
- Converts one Apple SSID filter to a one-entry bridge SSID list; empty SSID remains wildcard.
- Converts the passive/active type and optional unicast BSSID filter.
- Deduplicates the channel list and bounds it to 64 bridge channels.
- Rejects an explicitly supplied channel list if every entry was filtered out, rather than accidentally broadening it into a full-band scan.
- Assigns a nonzero generation supplied by the caller; the device queue replaces it with its own monotonic generation under the WMI work-loop gate.

The WCL adapter should call `queueNativeScanRequest()` after decoding and retain the returned generation. Its abort callback should call `abortNativeScanRequest(generation)`; `abortCurrentNativeScanRequest()` is also available when the Apple callback carries no generation. `getNativeScanStatus(generation)` reports pending/success/failure/cancel/timeout and result count under the WMI work-loop gate.

## Result path still required

The `WMIBSS` cache now retains bounded raw IE bytes in addition to BSSID, raw SSID, channel, SNR, capability bits, beacon interval, supported rates and parsed RSN data. `copyNativeBSSRecord()` exports one generation-checked record under the WMI work-loop gate; the frontend never receives a pointer into the mutable cache. The Apple `getSCAN_RESULT` iterator and completion notification still need implementation. RSSI is explicitly marked as an estimate using SNR minus the documented -95 dBm noise-floor convention; per-BSS noise is reported unavailable rather than invented.
