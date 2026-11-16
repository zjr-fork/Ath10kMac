# Ath10k WMI scan request mapping

Status: request serializer and WMI-work-loop queue/cancel path implemented on the development branch. Kernel compilation and hardware validation are still pending.

## Reference

- Linux ath10k upstream: `torvalds/linux`
- Revision inspected: `aebb7274476403040d582c61478df8f4a7bf9cd3`
- Files: `drivers/net/wireless/ath/ath10k/wmi.h`, `wmi-tlv.h`, `wmi-tlv.c`
- Use as protocol reference only. Do not copy Linux source into this repository.

## WMI-TLV START_SCAN layout in the current Ath10kMac builder

The current frame has an 8-byte HTC header, a 4-byte WMI command ID, then the WMI TLV command. The fixed command body is 100 bytes and is followed by four TLVs:

1. Channel array: `ARRAY_UINT32`, each entry is a little-endian frequency in MHz.
2. SSID array: `ARRAY_FIXED_STRUCT`, each element is `wmi_ssid { le32 ssid_len; u8 ssid[32]; }` (36 bytes).
3. BSSID array: `ARRAY_FIXED_STRUCT`, each element is an 8-byte `wmi_mac_addr` representation; the first six bytes carry the MAC address.
4. IE array: `ARRAY_BYTE`, currently empty.

Within the current frame buffer, the fixed body fields are:
- `f + 76`: burst duration
- `f + 80`: channel count
- `f + 84`: BSSID count
- `f + 88`: SSID count
- `f + 92`: IE byte count
- `f + 96`: probe count
- `f + 100`: source MAC address
- `f + 108`: source MAC mask
- `f + 116`: channel-array TLV header
- `f + 120`: channel-frequency array

The existing builder emits an empty SSID array, one broadcast BSSID, and the policy-derived fixed channel list. The current scan-control flags are `0x31`: passive scan, channel-stat event, and the probe-request filter bit. Linux ath10k documents that WMI-TLV inverts the `WMI_SCAN_FILTER_PROBE_REQ` bit relative to some firmware interfaces; preserve this inversion when changing flags.

## Required request translation

- Validate the bridge request before touching the shared WMI TX buffer.
- If the request has no channel list, use the current regulatory-policy list. If it supplies channels, map each channel number through the existing QCA9377 channel/frequency table and intersect it with the regulatory policy. Never pass caller-provided frequencies directly to firmware.
- Emit exactly `ssid_count` 36-byte SSID entries and set the fixed command's `num_ssids` accordingly. Copy only each declared SSID length; do not treat SSIDs as C strings.
- Keep `num_bssids = 1`; use the requested unicast BSSID when present, otherwise use the broadcast address. The BSSID TLV element remains eight bytes.
- Map the bridge's `active_dwell_ms` and `passive_dwell_ms` directly to the corresponding common fields; zero means use the driver's tested default. The bridge validator accepts explicit dwell values from 40 through 2000 ms, matching the upstream-documented firmware minimum and a bounded host limit.
- Match the upstream WMI-TLV defaults of `num_probes = 3` and `probe_delay = 5 ms`; the global PASSIVE flag suppresses probe transmission for passive scans. Active scans clear PASSIVE and omit DFS channels. Preserve the documented probe-filter inversion and the existing event subscription/requestor-ID semantics.
- Compute frame size from the actual channel and SSID counts, check the HTC maximum message size and DMA buffer capacity before writing, and clear the entire frame before populating it.
- Keep one WMI scan in flight. Do not mutate/reuse the shared buffer while CE3 owns it. On abort, preserve the existing ordering: START_SCAN descriptor consumed and HTC credit returned before STOP_SCAN is submitted.
- Keep the random-probe MAC address/mask fields zero until random-address scanning is explicitly negotiated; ordinary scans use the firmware VDEV address.
- A successful WMI transaction requires correlated STARTED/COMPLETED events and transport ownership completion. An empty BSS cache is a valid result, not proof that scan submission failed. Firmware failure and timeout are reported separately from success; timeout requests STOP_SCAN and retains ownership until its completion/credit barrier.

## Current gap

`buildWMIScanRequest()` now serializes the versioned bridge request, and `queueNativeScanRequest()` / `abortNativeScanRequest()` copy and serialize requests on the WMI work loop. The existing `serviceWMIScan()` uses the same CE3 descriptor, HTC credit, event correlation, timeout and STOP_SCAN lifecycle for native requests; the boot-time scan continues through the original builder. Active requests omit DFS channels because this WMI command uses one global passive flag. The Apple80211/WCL frontend does not call these entry points yet, and no native scan result has been published to macOS.
