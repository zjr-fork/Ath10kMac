# Native Wi-Fi on macOS Sequoia: implementation contract

Status: design baseline for the first native-Wi-Fi implementation. This document does not claim that a native controller or Skywalk data path has been implemented or tested.

Target: Intel x86_64, macOS Sequoia 15.7+, QCA9377 PCI function `168c:0042`, Samsung subsystem `144d:4133`. macOS 26/Tahoe is out of scope. The Ethernet-style interface remains a diagnostic fallback only; it is not the product endpoint.

## 1. Architectural boundary

Keep two logical components in the same KEXT, but use one PCI service object rather than adding a second hardware-owning service:

- **Ath10kCore** owns the PCI function, BAR/DMA, CE rings, firmware/board data, HTC, WMI, HTT, scan transactions, association state machine, RX/TX descriptors, and firmware key transactions.
- **Native Apple frontend** changes the PCI service superclass from `IOEthernetController` to Sequoia `IO80211Controller` (which itself derives from `IOEthernetController`) and adds a child `QcaAirportInfraInterface : IO80211InfraProtocol` for Apple80211/WCL/Skywalk methods. It must not duplicate PCI, firmware, or HTC/WMI ownership.

This preserves the existing Ethernet-MSDU packet path while making the service a real IO80211 controller. The frontend owns interface registration and Apple control-plane translation; the core owns all radio transactions.

The boundary is a narrow, versioned C ABI with fixed-width integers and caller-owned buffers. No IOKit C++ object, OSObject, mbuf, Skywalk object, STL type, exception, or compiler-dependent C++ layout crosses it. Every asynchronous request carries a monotonically increasing generation. Events from an older generation are discarded.

Keep the current Ethernet frontend available during bring-up, but only one frontend may own the radio at a time. A lease/owner transition must quiesce queues and complete or cancel outstanding DMA transactions before changing owner.

## 2. Core API surface

The first bridge revision should expose only these operations:

| Operation | Contract |
|---|---|
| acquire/release | Exclusive frontend lease; release drains or cancels outstanding work |
| get-capabilities | Hardware, firmware, bands, channel/rate limits, supported ciphers and frame mode |
| scan-start / scan-abort | Asynchronous request with SSID filters, channel list, scan type, generation and deadline |
| scan-snapshot | Copy a bounded, stable BSS snapshot; no pointers into mutable cache |
| associate / join-abort / disconnect | Asynchronous MLME operations keyed by generation and BSSID |
| set-key / remove-key | Validate cipher, key type, key index, length and RSC; return only after firmware transaction and required echo barrier |
| tx-submit / rx-drain | Explicit ownership transfer and completion IDs; no implicit packet lifetime |
| get-link-snapshot | SSID/BSSID/channel/RSSI/state/counters from a synchronized snapshot |

All functions return a stable status enum. All output structures include `size` and `version`; unknown versions and undersized structures fail closed. Key bytes are write-only inputs: never copied into diagnostics, IORegistry, or logs.

## 3. Scan transaction rules

The current `WMIScanState` already tracks generation, scan ID, START_SCAN TX consumption, HTC credit return, STARTED/terminal events, cancellation and STOP_SCAN ownership. Reuse that transport and transaction machinery; do not create a second WMI ring or bypass the runtime work-loop gate.

The current `buildWMIScan()` is an internal all-SSID/broadcast-BSSID scan with a fixed regulatory channel list. It is not yet a system-request adapter. The native coordinator must:

1. Validate and copy the caller's SSID filters, BSSID filter, channel list, active/passive mode and dwell limits into bounded storage.
2. Assign a fresh generation and WMI scan ID; reject concurrent scans with a busy status.
3. Build the WMI command from the validated request, while intersecting requested channels with the existing regulatory policy.
4. Publish results only from a stable BSS-cache snapshot; preserve binary SSIDs and IE bytes without assuming UTF-8 or NUL termination.
5. Abort by setting cancellation for that generation. Submit STOP_SCAN only after START_SCAN descriptor consumption and HTC credit return; do not recycle the shared WMI TX buffer before ownership is returned.
6. Complete only after correlated firmware terminal event and transport ownership completion. Ignore stale events from previous generations.

Do not expose the current boot-time fast scan as a successful system scan. Internal bring-up scanning and user-requested scans need separate request origins and completion notifications.

## 4. Association and security

The existing connection state machine is ordered: VDEV start → peer create → authentication → association → peer association/authorization → VDEV up → firmware echo barrier → connected. The native MLME adapter submits requests asynchronously and reports state changes; it must not block an Apple80211 callback waiting for the entire association.

For WPA2, keep the controlled port closed until pairwise and group key installation has completed and the firmware echo barrier has succeeded. Key installation must validate cipher/key ID/key length/key type/RSC, and preserve the existing TKIP group-key MIC ordering. Re-key follows the same asynchronous transaction model. Never publish key material or PMK/PTK/GTK to the frontend.

WPA3/SAE, enterprise EAP, AP mode, AWDL and DFS operation remain unsupported unless separately implemented and validated; do not advertise them in capabilities.

## 5. Native IO80211 packet contract and data path

The pinned Sequoia ABI declares `IO80211Controller : IOEthernetController`. Its packet methods therefore retain the Ethernet-controller contract. AirportItlwm's `outputPacket(mbuf_t)` queues network mbufs through its BSD `if_snd` / `if_start` path, also confirming that the native Wi-Fi controller boundary is not a generic raw-MPDU socket. Ath10k firmware already owns MLME, encryption and 802.11 framing, so the existing Ethernet-MSDU TX/RX path is the intended reusable data plane behind an `IO80211Controller`, subject to verifying the precise Skywalk input ownership and interface registration.

Before wiring the frontend:

1. Keep `Ath10kPCIDevice`'s Ethernet-MSDU TX/RX core behind the `IO80211Controller` service and replace only the macOS-facing interface registration and control plane.
2. Confirm the exact `IO80211SkywalkInterface` input packet ownership and the `createInterface` / `configureInterface` lifecycle on Sequoia.
3. Confirm QCA9377 firmware RX decapsulation mode from HTT descriptors and packet captures. Current `deliverNetworkRX()` decodes firmware RX into Ethernet MSDUs; current `outputPacket()` accepts Ethernet MSDUs and submits them to HTT.
4. Do not implement raw 802.11 RX/TX conversion unless the selected Apple interface explicitly requires raw MPDUs. Keep management and EAPOL policy explicit.

The adapter owns packet objects only for the documented interface lifetime. Core owns DMA buffers until CE/HTT completion proves device consumption. Completion, cancellation and teardown must be idempotent and generation-scoped.

## 6. Sequoia ABI validation gates

Compile only against the Sequoia 15.x headers from the pinned AirportItlwm reference, not Tahoe 26 headers. Before implementing the concrete subclass:

- Enumerate every pure virtual method required by the exact `IO80211Controller`, `IO80211InfraProtocol`, and Skywalk interfaces in that SDK snapshot.
- Verify vtable signatures, enum values, structure packing, callback ownership and WCL request lifetimes against the same version.
- Keep all version-specific Apple ABI declarations in the frontend target; do not leak private Apple types into Ath10kCore.
- Add compile-time size/offset assertions for every cross-component bridge structure.

Do not infer that an interface is complete because a few familiar methods such as `getSSID`, `getCHANNEL` or `setCIPHER_KEY` compile.

## 7. Implementation sequence and acceptance

1. Pin the exact Sequoia 15.x ABI reference revision and license; keep it external and do not vendor GPL-2.0-only itlwm headers into this GPL-3.0 repository.
2. Add the versioned bridge header and compile-time layout tests, with no hardware behavior changed.
3. Extract core operations behind the bridge while retaining the current Ethernet frontend; regression-test existing QCA9377 behavior.
4. Add native scan request/result/abort mapping and test generation, stale-event, timeout and cancellation cases.
5. Add native MLME and key mapping; test open and WPA2 association, re-key and controlled-port failure paths.
6. Confirm firmware frame mode, then implement native RX/TX and packet ownership tests.
7. Add the Sequoia IO80211/Skywalk frontend and validate on the target machine.

Release acceptance requires a real macOS Wi-Fi interface (not an Ethernet service), visible scan results in macOS Wi-Fi UI, association/disassociation, WPA2, DHCP, ARP, bidirectional IPv4/IPv6 traffic, suspend/wake recovery, and no DMA/packet ownership leaks. A successful compile alone is not acceptance.

## 8. Current audit findings

- The repository currently exposes `Ath10kPCIDevice : IOEthernetController`; its `outputPacket` and `deliverNetworkRX` are Ethernet-MSDU paths. The AirportItlwm reference uses an `IO80211Controller` with a BSD `if_snd` / `if_start` path, so the correct Sequoia packet boundary must be confirmed before deciding whether to reuse these MSDU paths or add raw 802.11 conversion.
- WMI START_SCAN/STOP_SCAN, correlated event parsing, BSS cache and CE3/HTC-credit ownership are already present and should be reused.
- `buildWMIScan()` currently hardcodes broadcast BSSID, empty SSID filters, fixed channels and passive scan flags.
- The current user-client `Scan` operation resets the internal BSS cache and increments a generation; it is not an Apple80211/WCL scan request implementation.
- HTT RX currently normalizes/decapsulates data for Ethernet delivery; the existing Ethernet TX path calls `buildHTTPacket()` with Ethernet payload. Neither is proof of a native 802.11 data path.
- Existing WPA2 key install, re-key, controlled-port gating and echo-barrier behavior should be preserved behind the bridge rather than rewritten in the frontend.


## 9. Repository progress

- Added `src/shared/Ath10kNativeBridge.h`: ABI v1 enums and bounded, pointer-free scan, BSS, key and link structures. C and C++ compile-time assertions pin the packed sizes and offsets. Pure validation helpers reject malformed scan filters, dwell values, generations, key indexes and unsupported TKIP pairwise keys.
- Added standalone C and C++ layout/sanity tests plus `scripts/test-native-bridge.sh`, which runs them with the active Xcode toolchain on the target Mac. Equivalent C and C++ validation snippets were compiled and executed in the development environment; the repository script still needs to be run on the target Mac.
- Added `docs/SEQUOIA_15_ABI_PIN.md`: pinned `laobamac/itlwm` revision `7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e`, identified the `__IO80211_TARGET=__MAC_15_2` selection path and Sequoia ABI sizes/abstract-method surface. The reference is GPL-2.0-only, so its headers and implementation are not vendored into this GPL-3.0 project.
- Added `docs/WMI_SCAN_REQUEST_MAPPING.md`: mapped WMI-TLV START_SCAN fields against Linux ath10k upstream revision `aebb7274476403040d582c61478df8f4a7bf9cd3`.
- Added `buildWMIScanRequest()` to serialize bounded SSID/BSSID/channel and active/passive parameters; active scans exclude DFS channels and retain default firmware VDEV MAC behavior.
- Added work-loop serialized `queueNativeScanRequest()`, generation-scoped `abortNativeScanRequest()` and current-request `abortCurrentNativeScanRequest()` entry points. `serviceWMIScan()` now dispatches queued native requests through the existing CE3/HTC/event/STOP_SCAN state machine without changing the boot-time scan path.
- Added `ath10kNativeDecodeAppleScanRequest()` and `docs/APPLE_WCL_SCAN_REQUEST_MAPPING.md` to independently decode the pinned Sequoia WCL scan request into the bridge ABI, including bounded channel filtering and passive/active mode mapping.
- Extended `WMIBSS` to retain up to 512 bytes of validated raw IEs (falling back to the RSN IE if the complete IE stream is larger). Added `copyNativeBSSRecord()` to export generation-checked BSS snapshots under the WMI work-loop gate, including SSID, BSSID, channel/frequency, rates, capabilities, beacon interval and an explicitly marked SNR-derived RSSI estimate. Added `getNativeScanStatus()` with pending/success/failure/cancel/timeout states; the host deadline now requests STOP_SCAN and waits for descriptor consumption plus HTC credit return before declaring timeout completion.
- Added `tests/test_native_scan_builder.cpp` and a Makefile syntax-check target so the Sequoia kernel headers and WMI builder can be checked together on the target Mac.
- The ABI and scan coordinator are not yet called by an Apple80211/WCL frontend; scan results are not yet translated into macOS scan records or completion events. Association, keys, native RX/TX and the Skywalk frontend remain unimplemented. The bridge is still a same-kext integration contract, not a cross-kext transport. Do not load this header as a driver or infer native Wi-Fi support from its presence.
- The bridge layout uses packed little-endian fields. Callers must validate `version`, `size`, `operation`, `payload_bytes`, all counts and all per-field bounds before accessing payloads; copy packed multi-byte values to aligned locals before arithmetic.
