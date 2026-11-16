# Sequoia 15.x IO80211 ABI pin

Status: source audit complete; compile/runtime validation on macOS 15.7 remains required.

## Pinned reference

- Repository: `https://github.com/laobamac/itlwm`
- Revision: `7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e`
- Target family: Sequoia
- Header selection: define `__IO80211_TARGET=__MAC_15_2`

At this revision, the wrapper headers in `include/Airport/` select the Sequoia ABI only when `__IO80211_TARGET == __MAC_15_2`; otherwise the WCL wrapper selects the macOS 26 headers. Do not include the generic wrapper headers with an undefined or guessed target. Do not copy Tahoe declarations into this driver.

The 12 `IO80211Controller` pure virtuals at this target are: `isCommandProhibited(int)`, `handleCardSpecific(IO80211SkywalkInterface*, unsigned long, void*, bool)`, the eight Apple80211 methods `getDRIVER_VERSION`, `getHARDWARE_VERSION`, `getCARD_CAPABILITIES`, `getPOWER`, `setPOWER`, `getCOUNTRY_CODE`, `setCOUNTRY_CODE`, `setGET_DEBUG_INFO` (each takes the interface pointer plus its Apple80211 data pointer), `getLogger() const -> CCLogStream*`, and `getFaultReporterFromDriver() -> CommonFaultReporter*`. The signatures and first-argument interface pointer are ABI-sensitive; do not substitute guessed overloads.

The 15.2 header set is the available Sequoia-family reference. The target OS remains macOS 15.7+; the final binary must still be validated on 15.7 because a 15.2 header snapshot is not itself proof of runtime compatibility with every later Sequoia build.

## ABI facts observed at the pinned revision

| Header / class | Sequoia declaration |
|---|---|
| `macOS15/IO80211SkywalkInterface.h` | `sizeof(IO80211SkywalkInterface) == 0x118`; one additional pure virtual `setMacAddress(ether_addr&)` |
| `macOS15/IO80211InfraInterface.h` | `sizeof(IO80211InfraInterface) == 0x120`; inherits Skywalk interface and supplies the link-state override |
| `macOS15/IO80211InfraProtocol.h` | `sizeof(IO80211InfraProtocol) == 0x120`; 190 pure virtual request methods |
| `macOS15/IO80211ControllerV2.h` | `IO80211Controller` derives from `IOEthernetController`; `sizeof(IO80211Controller) == 0x128`; 12 pure virtual methods, including `getLogger` and `getFaultReporterFromDriver` |

The generic wrapper `IO80211InfraProtocol.h` contains a different declaration set (231 pure virtual methods in the non-versioned file); it must not be used to infer the Sequoia vtable. The macOS15-specific protocol header is authoritative for this target.

## Licensing and source-use boundary

The pinned `laobamac/itlwm` revision's root `LICENSE` is GNU GPL version 2 (June 1991), while Ath10kMac is GPL-3.0. Treat itlwm as an ABI/research reference only: do not copy its implementation files or vendor its headers into this GPL-3.0 repository as part of this work. The native adapter must be independently implemented, and any future code reuse requires a separate compatibility review. Preserve the upstream URL, exact revision and license note in project provenance.

## Packet-path implication

The Sequoia `IO80211Controller` is derived from `IOEthernetController`, not from a raw-802.11 packet interface. This means the current controller's Ethernet-MSDU `outputPacket` / `inputPacket` data path may be retained when moving the service superclass to `IO80211Controller`; do not add MPDU reconstruction or raw-frame TX unless the exact Skywalk contract proves it is required. The native Wi-Fi behavior is supplied by the controller/protocol ABI, scan/MLME/security state and correct interface registration, not by merely passing raw 802.11 frames to Skywalk.

## Core bridge progress

The pointer-free ABI now defines a stable `Ath10kNativeLinkSnapshot` and link-state values. `Ath10kPCIDevice::copyNativeLinkSnapshot()` serializes reads on the WMI work-loop gate and exports only bounded SSID/BSSID data, channel, state, open/WPA2 mode, packet/byte counters, and an explicit `-128` RSSI sentinel when the firmware has not supplied a valid signal measurement. It does not invent RSSI or noise values.

## Frontend implementation implications

1. Include `<Airport/Apple80211.h>` with `AIRPORT_WCL` and `__IO80211_TARGET=__MAC_15_2` set consistently for every translation unit.
2. Implement the concrete controller against the 15.2 `IO80211Controller` abstract contract, including logger and fault-reporter methods with their Sequoia-specific return types.
3. Implement the concrete infrastructure interface against the 15.2 `IO80211InfraProtocol` contract (190 pure virtual methods in this snapshot). The reference subclass has many intentionally unsupported methods; unsupported features must return `kIOReturnUnsupported`, not fabricated success.
4. Implement `setMacAddress(ether_addr&)` required by the Sequoia Skywalk base class.
5. Preserve the WCL-specific scan, association, leave, join-abort, key and link-state entry points, but forward them to the versioned Ath10k bridge. They must not call PCI/WMI directly.
6. Keep the 15.2 header snapshot as an external reference only; do not copy or vendor its GPL-2.0-only header/source files into this GPL-3.0 repository.

## Validation gates

- Build the frontend with `__IO80211_TARGET=__MAC_15_2`; add compile-time checks for the four class sizes above.
- Verify that the concrete controller and interface classes are not abstract (instantiate them in a compile-only target or use a small derived-type assertion).
- Verify all vtable signatures against the pinned headers, including the WCL overloads and logger/fault reporter.
- On macOS 15.7+, verify the kext links, loads and attaches without ABI/vtable faults. Then test Wi-Fi UI scan and association separately from data-plane traffic.
