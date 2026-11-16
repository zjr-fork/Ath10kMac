# Sequoia 15.2 IO80211 frontend ABI audit

Status: source-level ABI inventory only. No native controller/interface subclass is implemented by this document, and no kernel build or runtime validation is claimed.

## Pinned reference

- Repository: `laobamac/itlwm`
- Revision: `7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e`
- Target selection: `__IO80211_TARGET == __MAC_15_2`
- Relevant declarations:
  - `include/Airport/macOS15/IO80211ControllerV2.h`
  - `include/Airport/macOS15/IO80211SkywalkInterface.h`
  - `include/Airport/macOS15/IO80211InfraInterface.h`
  - `include/Airport/macOS15/IO80211InfraProtocol.h`

The reference is GPL-2.0-only. This audit records ABI facts and counts only; do not vendor its headers or copy implementation into this GPL-3.0 project.

## Declaration inventory

The pinned Sequoia 15.2 declarations contain:

| Type | Virtual declarations | Pure virtual declarations |
|---|---:|---:|
| `IO80211Controller` | 87 | 12 |
| `IO80211SkywalkInterface` | 134 | 1 |
| `IO80211InfraInterface` | 56 | 0 |
| `IO80211InfraProtocol` | 190 | 190 |

The 190 count is from the macOS15-specific `IO80211InfraProtocol.h`, not the generic compatibility header or macOS26 header. A concrete infra interface must satisfy those 190 pure virtual declarations and the inherited pure virtual `IO80211SkywalkInterface::setMacAddress(ether_addr&)`. The 56 methods declared by `IO80211InfraInterface` are overrides of its Skywalk base contract; they do not reduce the 190 pure virtual methods introduced by `IO80211InfraProtocol`.

The controller remains a separate abstract class with 12 pure virtual requirements. The 12-method controller contract and the 190-method infra-protocol contract must not be conflated.

## Consequences for the implementation

1. Keep the PCI/radio implementation in `Ath10kPCIDevice`; the eventual controller front end must preserve the exact Sequoia inheritance and vtable contract.
2. Implement a dedicated `QcaAirportInfraInterface : IO80211InfraProtocol`. Do not try to make the PCI device itself satisfy the infra-protocol vtable.
3. Use a generated declaration inventory or a compile-time subclass check against the actual Sequoia SDK headers on the target Mac. A hand-counted stub list is not sufficient.
4. Unsupported features must return `kIOReturnUnsupported` (or the exact documented no-op behavior where Apple requires it); never return success with uninitialized or fabricated data.
5. Implement the minimum useful station path first: interface init/start/stop, MAC address, link state, scan request/abort/result iteration, association/join abort/leave, state/SSID/BSSID/channel/RSSI, and cipher-key installation. All unrelated AP, AWDL, P2P, ranging, NAN, offload and telemetry methods remain explicitly unsupported.
6. Keep WCL callbacks nonblocking. Scan/association operations enqueue generation-scoped work in Ath10kCore; Apple-facing completion notifications must be emitted only after the core transaction reaches a terminal state.
7. Do not switch `Ath10kPCIDevice`'s superclass or change interface registration until the macOS 15.7 SDK compile verifies all controller overrides, the concrete infra subclass, and the Skywalk packet ownership contract.

## Scan-result integration boundary

The core currently exports bounded `Ath10kNativeBSSRecord` snapshots (BSSID, SSID, channel/frequency, estimated RSSI, capabilities, beacon interval, rates and raw IEs) plus generation-scoped completion status. The frontend still needs to:

- Convert one copied snapshot at a time into the exact Sequoia `apple80211_scan_result` layout.
- Preserve the Apple iterator/wrap semantics and end-of-results return code.
- Keep the result list stable for the lifetime of the scan-result iteration, or explicitly snapshot it in the frontend; never expose pointers into the mutable core BSS cache.
- Post the Apple scan completion event exactly once for the matching generation; stale completion events must be discarded.
- Mark SNR-derived RSSI as estimated and do not invent noise values.

## Validation gate

Before the first native interface load, compile against the actual macOS 15.7 SDK and verify:

- All 12 controller pure virtual methods are implemented with exact signatures.
- All 190 infra-protocol pure virtual methods and inherited Skywalk pure virtuals are implemented.
- Class sizes and critical vtable method order match the pinned ABI.
- Interface registration, BSD ifnet creation, Skywalk packet ownership, and teardown are verified.
- A native Wi-Fi interface is registered; an Ethernet-only service is not acceptance.

## Repeatable local audit

A small checker is now available at `scripts/audit-sequoia-io80211-abi.py`. It does not vendor or fetch any third-party headers. Point it at a local directory containing the four pinned macOS15 header files:

```sh
python3 scripts/audit-sequoia-io80211-abi.py --header-root /path/to/macOS15
```

The default mode fails closed if any of the four declaration counts differ from the pinned 15.2 inventory. Use `--no-pinned-counts` only to inspect a different SDK/header revision; that mode reports counts and must not be treated as proof of 15.2 ABI compatibility.

This is a declaration-count guard, not a C++ compiler or vtable-layout proof. The eventual concrete classes must still be compiled against the target SDK/header set, and class abstractness must be checked by the compiler. The checker intentionally does not generate method bodies or copy declarations from the GPL-2.0 reference tree.
