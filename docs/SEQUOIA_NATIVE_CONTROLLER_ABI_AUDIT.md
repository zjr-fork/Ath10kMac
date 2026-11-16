# Sequoia 15 Native Controller ABI Audit

Status: source-level audit only. No superclass migration or runtime ABI claim.

## Provenance and licensing boundary

Reference snapshot used for ABI inspection: `laobamac/itlwm`, revision `7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e`, headers under `include/Airport/macOS15` (`IO80211ControllerV2.h`, `IO80211SkywalkInterface.h`, `IO80211InfraInterface.h`, `IO80211InfraProtocol.h`). These headers are ABI reference material only. Do not copy implementation or header source into Ath10kMac; compile against the user's locally supplied Sequoia headers. Preserve the GPL-2.0-only reference / GPL-3.0 project boundary.

## Current baseline

`Ath10kPCIDevice` still derives from `IOEthernetController`. Existing PCI, firmware, WMI/HTT, connection, key-installation and Ethernet MSDU code is not automatically an Apple native Wi-Fi implementation.

The user confirmed a successful KEXT build and a clean `scripts/check-kext-symbols.py` result: no weak undefined imports and no undefined stack runtime probes. This validates those link/symbol checks only. It does not validate IO80211 vtables, object layout, interface registration, Skywalk packet ownership, WCL message lifetimes, or runtime scan/join behavior.

## Controller contract

The pinned `IO80211ControllerV2.h` declares `IO80211Controller : public IOEthernetController`. A concrete subclass must implement these 12 pure virtuals:

| Method | Return type | Requirement |
|---|---|---|
| `isCommandProhibited(int)` | `bool` | Real command gating; do not blanket-allow unsupported operations |
| `handleCardSpecific(IO80211SkywalkInterface*, unsigned long, void*, bool)` | `SInt32` | Dispatch only explicitly supported operations |
| `getDRIVER_VERSION(...)` | `IOReturn` | Real driver version |
| `getHARDWARE_VERSION(...)` | `IOReturn` | Actual QCA9377/hw revision |
| `getCARD_CAPABILITIES(...)` | `IOReturn` | Only implemented capabilities |
| `getPOWER(...)`, `setPOWER(...)` | `IOReturn` | Real radio/power state or unsupported |
| `getCOUNTRY_CODE(...)`, `setCOUNTRY_CODE(...)` | `IOReturn` | Validated regulatory state; no fake success |
| `setGET_DEBUG_INFO(...)` | `IOReturn` | Supported diagnostics only |
| `getLogger() const` | `CCLogStream *` | Valid object with verified ownership/lifetime |
| `getFaultReporterFromDriver()` | `CommonFaultReporter *` | Valid object with verified ownership/lifetime |

The abbreviated Apple80211 parameter types and exact qualifiers must be checked by compiling against the local Sequoia headers. In particular, `handleCardSpecific` returns `SInt32`, not `IOReturn`.

## Interface contract

The inheritance chain in the pinned headers is:

`IO80211InfraProtocol -> IO80211InfraInterface -> IO80211SkywalkInterface`

- `IO80211InfraProtocol` declares 190 pure virtual methods in this snapshot.
- `IO80211SkywalkInterface` adds inherited pure virtual `setMacAddress(ether_addr&)`.
- `IO80211InfraInterface` adds no further pure virtuals in this snapshot.
- The existing generator checks a generated subclass with 190 InfraProtocol stubs plus the inherited Skywalk stub using `std::is_abstract`.

The 190 count is a pinned-header invariant, not a guarantee for every Sequoia 15.x point release. The probe now checks the complete ordered list of all 190 method names, not just the count; the controller probe likewise checks the ordered 12-method list. This catches same-count reordering, which matters because virtual slot order is ABI. The local header root remains the compile authority, and any point-release difference must be audited rather than silently accepted. Methods outside the initial STA scope (AP, AWDL, P2P, ranging, NAN, offloads, telemetry, vendor features) must return the documented unsupported status or a safe no-op only where the contract permits it. Never report success for an operation not performed.

## Initial station path

Prioritize interface create/start/stop and MAC; scan request/abort/result enumeration/completion; associate/join, join abort and leave; link state, SSID, BSSID, channel, RSSI and security reporting; then key installation and controlled-port authorization after the Apple interface lifecycle is wired.

The header contains WCL entry points including `setWCL_SCAN_REQ`, `setWCL_SCAN_ABORT`, `setWCL_ASSOCIATE`, `setWCL_JOIN_ABORT`, and `setWCL_LEAVE_NETWORK`. Their declarations do not establish payload ownership or callback semantics.

## Open ABI/lifecycle hazards

1. `std::is_abstract` proves only that no pure virtual remains. It does not prove vtable slot compatibility, object size, base offsets, hidden ABI aliases, or compatibility across 15.x builds.
2. The controller and primary `IO80211InfraProtocol` object are separate framework-managed objects. Establish creation, registration, detach, stop and free ordering before wiring requests.
3. Verify valid object type, ownership and lifetime for `getLogger()` and `getFaultReporterFromDriver()`.
4. `postMessage(IO80211SkywalkInterface*, unsigned int, void*, unsigned long, bool)` is declared, but buffer-copy/retention and completion semantics remain unproven. Do not pass stack or short-lived buffers until verified.
5. Existing Ethernet MSDU TX/RX may be reusable only after verifying Skywalk attach and packet ownership. A native Wi-Fi front end is not a raw-MPDU socket.
6. Do not migrate `Ath10kPCIDevice` to `IO80211Controller` yet.

## Next engineering gate

A temporary compile-only generator and shell target for the 12 controller pure virtuals have now been added alongside the existing InfraProtocol probe. Their fixture test is wired into the Makefile, and the full milestone invokes both compile probes. They still need to be run against the user's local Sequoia headers and current kernel SDK. If both generated subclasses are concrete, inspect local declarations for interface factory/registration, primary-interface access, WCL message lifetime and teardown callbacks. Only after that should the production controller/interface pair and lifecycle state machine be designed.

Passing compile probes is source-level compatibility evidence, not proof of runtime ABI or Wi-Fi operation.


## Virtual-slot drift guard

The temporary probe generators now enforce both count and ordered method identity for
the pinned controller and infrastructure protocol contracts. A count-only check could
miss a header revision that removes one pure virtual and adds another, or reorders
methods while leaving the count unchanged. The ordered checks reject those cases before
the generated concrete subclass is compiled. This is still not a complete vtable/layout
proof: inherited non-pure virtuals, compiler ABI details, object size, and runtime
framework expectations remain separate gates.


## Controller/interface relationship confirmed by declarations

The pinned controller header exposes the following non-pure framework entry points:

- `getPrimarySkywalkInterface()` returns an `IO80211SkywalkInterface *` for the primary interface.
- `attachInterface(OSObject *, IOService *)` and `detachInterface(OSObject *, bool)` are controller-side interface lifecycle hooks.
- `postMessage(IO80211SkywalkInterface *, unsigned int, void *, unsigned long, bool)` is controller-side WCL message publication.
- The controller itself overrides the ordinary `IOEthernetController` service lifecycle (`init`, `start`, `stop`, `free`).

These declarations establish where the framework-facing relationship lives, but do **not** establish who allocates the primary interface, whether the driver must call `attachInterface`, what the `OSObject *` argument represents, or when detach completes relative to provider stop/free. The implementation must not guess those semantics or manually allocate a second primary interface based only on the accessor.

The same header pins `sizeof(IO80211Controller) == 0x128`; the Skywalk and InfraProtocol headers pin `0x118` and `0x120` respectively. These are reference-header assertions, not proof that a subclass compiled against a different local point release has matching member offsets. The local Sequoia compile probe must check these sizes directly, and runtime registration still needs validation on 15.7.

**Integration rule:** treat the controller and primary infra interface as two framework-managed objects with distinct lifetimes. Before implementing either production class, trace the local header/API contract for primary-interface creation, attachment, start ordering, stop ordering, and detach completion. Keep the PCI device/core as the hardware provider; do not let the interface own or free the PCI device's DMA/WMI state.
