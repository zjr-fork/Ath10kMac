# Native STA Front-End Implementation Plan

Status: design contract; production Apple-framework objects are not yet wired.

## Object ownership

- The PCI service remains the sole owner of the QCA9377 PCI function, BAR mapping,
  DMA mapper, CE rings, IRQ state, firmware, HTC/WMI runtime, scan engine and
  connection/key state.
- The native IO80211Controller is the framework-facing controller and provider.
  It delegates hardware work to the existing PCI/core service through a
  lifetime-checked bridge; it must not duplicate PCI ownership.
- The primary IO80211InfraProtocol object is a separate framework-facing
  interface. It owns only its interface registration/lifecycle state, request
  generations/cursors and callback coordination. It never frees PCI/core state.
- Controller stop first prevents new interface work, aborts outstanding
  generation-scoped operations, waits for the core's DMA/HTC ownership barriers,
  then detaches/releases the interface in the verified framework order.

## Initial station callback surface

The pinned Sequoia 15 reference header exposes these WCL pure virtuals:

| Callback | Declared input | Ath10kMac action |
|---|---|---|
| setWCL_SCAN_REQ | apple80211ScanRequest * | Decode using the existing Apple request decoder; copy into Ath10kNativeScanRequest; queue one native WMI scan generation |
| setWCL_SCAN_ABORT | void * | Resolve the interface's currently active WCL generation and request abort; never cancel a newer generation based on stale state |
| setWCL_ASSOCIATE | apple80211AssocCandidates * | Validate candidate list and selected security profile before invoking the existing association/key state machine |
| setWCL_JOIN_ABORT | void * | Abort only the matching in-flight join generation; stale/duplicate abort is not reported as a new successful operation |
| setWCL_LEAVE_NETWORK | apple80211_leave_network * | Request disconnect through the core; report completion only after link/key/controlled-port teardown reaches a terminal state |

These names and declared pointer types are ABI observations from the pinned
reference snapshot, not permission to copy its implementation. Compile against
the user's local 15.7 headers before declaring production overrides; do not
normalize similarly named aliases or overloads by guesswork.

## Callback rules

The pinned reference routes WCL entry points through an interface command gate before
switching on the operation. Ath10kMac should likewise serialize short interface
state transitions (active generation, stop flag, cursor reset) on an interface
work loop/gate. The gate action must only validate/copy input and enqueue the
hardware-core operation; it must not wait for firmware, scan completion or
association completion while holding the interface gate.

1. Treat every framework-supplied pointer as borrowed. Validate null, size,
   counts, enums, lengths, alignment and nested offsets before reading it.
   Never store the pointer or pass it to asynchronous work.
2. Convert the request immediately into Ath10kMac-owned value data. The existing
   native scan queue API already copies the request before dispatching to the
   WMI work loop; follow the same pattern for association and leave.
3. Each accepted scan/join receives a monotonically changing generation.
   Abort, completion and result callbacks must match that generation. A stale
   callback must not affect a later operation.
4. Return an error when the operation is unsupported, malformed, not ready, or
   cannot be queued. Do not return success merely because a callback was
   received.
5. Do not block a WCL callback waiting for firmware, DMA, HTC credit return,
   authentication, key installation or DHCP. Queue work and publish exactly one
   terminal result after the core transaction completes.
6. Scan-result enumeration is a separate cursor protocol. It may expose only
   records from a successfully completed generation. Result publication and
   ScanDone completion must not be conflated.
7. Join success is not equivalent to association request acceptance. The
   connected state is published only after the negotiated security context is
   installed and the controlled port is authorized.

## Existing core pieces to reuse

- Native WMI scan request serialization and CE/HTC TX ownership barriers.
- Apple scan request decoder and regulatory intersection.
- Generation-scoped scan status and BSS cache.
- Pointer-free scan cursor and WCL BeaconMetadata serializer.
- Exactly-once WCL scan lifecycle tracker.
  Both scan and join lifecycle guards now enforce serial-number generation ordering, reject reused/older/half-range-ambiguous generations, and preserve their generation high-water mark across interface reset.
- Pointer-free, generation-scoped WCL join lifecycle tracker. Association success, first beacon from the selected BSSID, and controlled-port authorization are independent gates; successful Join completion is permitted only after all three are observed for the same generation. Event order is not assumed, and duplicate/stale events are rejected.
- Link snapshot and capabilities value structures.
- Existing WPA2/CCMP key installation and controlled-port authorization.

## Existing core join-completion evidence

The existing station state machine in `src/kext/Ath10kNetwork.cpp` provides
three distinct observations that a future native interface bridge can consume;
they must not be collapsed into the initial association callback:

- **Association transaction:** the serial connection path advances through
  authentication, association response, peer association, vdev-up and the
  connection ECHO barrier. Only after the required replies/barrier does it
  advance into `LinkStage::Connected`. This is the earliest reliable point
  to call `markAssociation(generation, true)`; request acceptance or the
  802.11 association response alone is insufficient.
- **Selected-AP beacon:** `receiveStationManagement()` first validates both
  transmitter and BSSID addresses against the selected `s.bssid`, rejects
  frames from other channels, and increments `beaconSerial` for beacon /
  active-probe-response observations. For the Join lifecycle, only an actual
  Beacon subtype (not a probe response) should satisfy
  `markFirstBeacon()`. The frontend needs a per-association baseline so a
  beacon observed during the preceding scan cannot satisfy a new join.
- **Controlled-port authorization:** for WPA2, the core installs the PTK and
  GTK, consumes the matching HTT security indications, advances through the
  firmware ECHO barrier, and only then sets `portAuthorized=true` and
  increments `securityGeneration`. For an open network, the port is opened
  when the completed association transaction enters `LinkStage::Connected`.
  The frontend must observe the post-association transition, not infer
  authorization from a key command being queued or merely CE-consumed.

`ConnectionState::stationBeaconSerial` is now a separate monotonic counter for
actual Beacon subtype frames only; Probe Responses continue to increment the
existing liveness counter but do not advance this join-specific counter. The
new counter is preserved across `resetDisconnected()`, allowing a future Join
request to snapshot a baseline and reject pre-request Beacon observations.
The WCL join lifecycle accepts the Beacon event before or after the association
completion event, but requires the caller to prove both selected-BSSID match
and a post-begin Beacon serial.

The host-testable `Ath10kNativeScanObserver.hpp` now joins the core's
`Ath10kNativeScanStatus` to the interface's generation-scoped BSS cursor.
A matching pending status leaves the operation active; only a matching
SUCCEEDED status opens result enumeration. FAILED, CANCELLED, timeout and
explicit abort close the generation without exposing partial BSS records.
Late success after abort, stale status generations and stale result-cursor
generations are rejected. The helper owns no BSS memory and obtains records
only through the PCI service's value-copy API. It is not yet connected to
Apple WCL result callbacks or ScanDone message publication.

The host-testable `Ath10kNativeJoinObserver.hpp` now consumes only
`Ath10kNativeJoinSnapshot` values. The interface supplies its own operation
generation, selected BSSID, security mode and pre-join beacon/security serial
baselines. The observer rejects stale generations, malformed snapshots,
mismatched BSSIDs and pre-join beacons; it independently latches association,
a post-begin selected-AP Beacon and controlled-port authorization. It also
provides generation-checked abort and timeout terminals. WPA2 authorization
must also advance the security serial. Open networks use the
same authorization gate because the core explicitly opens their port when
the association reaches Connected. Counter comparisons use half-range serial
arithmetic that permits counter wrap through zero (operation generation zero
remains reserved). This helper is not yet connected to Apple WCL callbacks.

Native bridge actions are additionally gated by `_nativeCoreReady`, which
becomes true only after `prepareNetwork()` has completed and the shared
runtime worker has been armed; runtime shutdown clears it on the same gate.
This prevents an early native snapshot/scan from racing the one-time
pre-runtime initialization of connection state.

The snapshot action is serialized on `_wmiWorkLoop`; the runtime
initialization assigns that loop from `getWorkLoop()`, and the PCI service's
`getWorkLoop()` is the `_networkWorkLoop` created by the controller. Thus the
runtime connection state machine and snapshot action share the controller
work-loop gate once runtime is active. Early device/network preparation occurs
before native requests are accepted and is not a concurrent Join observation.

The PCI service now exposes `copyNativeJoinSnapshot()` as a value-only,
work-loop-serialized observation API. Its fixed 32-byte structure carries the
core link stage, association status, actual-Beacon serial, security generation,
disconnect stage, security/authorization flags, selected BSSID and channel.
It retains no frontend pointer and does not publish WCL messages. This gives a
future controller/interface bridge a single coherent snapshot instead of
reading mutable `ConnectionState` fields independently. The packed C/C++ ABI
is asserted by both bridge test translation units.

These are source-level observations of Ath10kMac's current core path, not a
claim that the native WCL bridge is already wired. The observer now translates
the pointer-free snapshot into lifecycle facts, but no production controller
or interface calls it yet. The interface must capture the beacon/security
baselines immediately before queueing each accepted Join and poll/observe only
while that same generation remains active.

## Not yet proven / implementation blockers

- Exact local 15.7 typedef spellings and override signatures for all selected WCL
  callbacks, including abort and association aliases.
- `apple80211AssocCandidates` is only forward-declared by the pinned Sequoia 15 interface header. Its concrete layout is not established by the currently audited public headers; no association decoder may be written from guessed offsets. A local 15.7 SDK declaration or another authoritative layout source is required.
- The precise event source and synchronization point that constitutes controlled-port authorization must be mapped from the existing Ath10kMac key-install/authorization path before wiring the Join lifecycle tracker.
- Whether the legacy IONetworkController::attachInterface registration path
  remains required with the exact local SDK and selected build target.
- Ownership/copy semantics of controller/interface postMessage payloads and
  the required lifetime of scan-result messages.
- Required WCL callback ordering for scan result enumeration and terminal done.
- Exact provider relationship and asynchronous detach completion on 15.7.
- Full Skywalk TX/RX packet ownership and native interface registration behavior.

No production callback should be wired until its signature and completion
contract is established. The compile probes verify base-class abstractness and
pinned base sizes; they do not verify these runtime semantics.
