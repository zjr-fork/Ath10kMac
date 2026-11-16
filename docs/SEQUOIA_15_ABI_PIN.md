# Sequoia 15.x IO80211 ABI pin

Status: source audit complete; compile/runtime validation on macOS 15.7 remains required.

## Pinned reference

- Repository: `https://github.com/laobamac/itlwm`
- Revision: `7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e`
- Target family: Sequoia
- Header selection: define `__IO80211_TARGET=__MAC_15_2`

At this revision, the wrapper headers in `include/Airport/` select the Sequoia ABI only when `__IO80211_TARGET == __MAC_15_2`; otherwise the WCL wrapper selects the macOS 26 headers. Do not include the generic wrapper headers with an undefined or guessed target. Do not copy Tahoe declarations into this driver.

The 15.2 header set is the available Sequoia-family reference. The target OS remains macOS 15.7+; the final binary must still be validated on 15.7 because a 15.2 header snapshot is not itself proof of runtime compatibility with every later Sequoia build.

## ABI facts observed at the pinned revision

| Header / class | Sequoia declaration |
|---|---|
| `macOS15/IO80211SkywalkInterface.h` | `sizeof(IO80211SkywalkInterface) == 0x118`; one additional pure virtual `setMacAddress(ether_addr&)` |
| `macOS15/IO80211InfraInterface.h` | `sizeof(IO80211InfraInterface) == 0x120`; inherits Skywalk interface and supplies the link-state override |
| `macOS15/IO80211InfraProtocol.h` | `sizeof(IO80211InfraProtocol) == 0x120`; 190 pure virtual request methods |
| `macOS15/IO80211ControllerV2.h` | `sizeof(IO80211Controller) == 0x128`; 12 pure virtual methods, including `getLogger` and `getFaultReporterFromDriver` |

The generic wrapper `IO80211InfraProtocol.h` contains a different declaration set (190+ methods in the non-versioned file); it must not be used to infer the Sequoia vtable. The macOS15-specific protocol header is authoritative for this target.

## Frontend implementation implications

1. Include `<Airport/Apple80211.h>` with `AIRPORT_WCL` and `__IO80211_TARGET=__MAC_15_2` set consistently for every translation unit.
2. Implement the concrete controller against the 15.2 `IO80211Controller` abstract contract, including logger and fault-reporter methods with their Sequoia-specific return types.
3. Implement the concrete infrastructure interface against the 15.2 `IO80211InfraProtocol` contract. The reference subclass has many intentionally unsupported methods; unsupported features must return `kIOReturnUnsupported`, not fabricated success.
4. Implement `setMacAddress(ether_addr&)` required by the Sequoia Skywalk base class.
5. Preserve the WCL-specific scan, association, leave, join-abort, key and link-state entry points, but forward them to the versioned Ath10k bridge. They must not call PCI/WMI directly.
6. Keep the 15.2 header snapshot pinned as a reference dependency and record its license/provenance before copying any declarations into this repository.

## Validation gates

- Build the frontend with `__IO80211_TARGET=__MAC_15_2`; add compile-time checks for the four class sizes above.
- Verify that the concrete controller and interface classes are not abstract (instantiate them in a compile-only target or use a small derived-type assertion).
- Verify all vtable signatures against the pinned headers, including the WCL overloads and logger/fault reporter.
- On macOS 15.7+, verify the kext links, loads and attaches without ABI/vtable faults. Then test Wi-Fi UI scan and association separately from data-plane traffic.
