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

## Native Join request gap and current status

The initial source audit found that the connection engine only accepted
personality-property SSID/BSSID values and automatically selected from the BSS
cache. That blocker is now partially closed: the shared ABI defines a bounded
pointer-free OPEN-only request, and the PCI core exposes
`queueNativeJoinRequest()` / `abortNativeJoinRequest()` on the WMI work loop.

The new admission path requires the exact generation of a successful native
scan, an idle station state, and one unambiguous BSS matching the requested
BSSID and SSID. It rejects privacy-protected, RSN/WPA, IBSS, malformed-rate and
ambiguous entries. It also rejects BSS records whose SSID length or rate count exceeds the fixed record capacities. It seeds the existing connection state with the selected AP;
the legacy selector is then used only for regulatory, channel and PHY/rate
validation, never as a fallback to another AP. The API has not yet been wired
to `setWCL_ASSOCIATE`, and its kernel-side behavior has not yet been built or
runtime-tested.

The PCI core now also exposes `requestNativeLeave(generation)`. It accepts only
the currently active native Join generation, is idempotent for an already
started native Leave, and routes through the existing cancellation/drain and
peer/vdev teardown state machine. Unlike Join-abort, Leave does not mark the
connection as an error. A stale generation or a competing non-native
cancellation is rejected. This is a core-side primitive only; the WCL leave
callback and its terminal completion message are not wired.

The admission path validates the firmware RF-chain mask into a local
value before mutating `ConnectionState`; an invalid mask therefore leaves the
connection state untouched rather than returning NotReady with a partially
seeded station record.

The shared Join/abort/Leave work-loop action now uses an explicit
`NativeJoinActionKind` plus a bounded stack argument record instead of
integer-to-pointer sentinels. The action validates the operation kind before
reading operation-specific fields, and checks that a queued request's copied
generation agrees with the request header before admission. The fixed-size
Join request is copied by value into the synchronous work-loop argument record,
so the action does not dereference or retain the caller's request pointer. All
three operations remain serialized work-loop actions. This removes an implicit
dispatch encoding and closes the caller-buffer lifetime/TOCTOU window, but does
not change the WCL integration or completion status.

The host-testable `Ath10kNativeLeaveObserver` now waits for the pointer-free
Join snapshot to reach the final reset state (Disabled, no disconnect stage,
unsecured, unauthorized and no cancellation still asserted) before allowing a
future LeaveComplete publisher to finish. Closing the controlled port, an
in-progress disconnect, or an idle-looking snapshot before the core clears its
cancellation flag is explicitly not completion. Observer startup also rejects
a baseline that is already disconnecting or has cancellation asserted, so it
cannot claim ownership of another teardown transaction. Stale generations and
malformed snapshots are rejected; out-of-range link/disconnect stages and
non-boolean flags are also rejected. Its
regression test is part of `scripts/test-native-bridge.sh`; the observer is
not yet connected to a production interface or WCL message publisher.

The separate WPA2 limitation remains unchanged. The core still lacks a verified
macOS credential/key-material ingress that drives the complete EAPOL/4-way
handshake and controlled-port path. The existing key installation primitives
do not establish operational WPA2. WPA2 remains rejected by the Join request
validator and absent from advertised capability flags until that path is proven.

Remaining work:
1. Wire the verified 15.7 WCL association callback to the pointer-free Join API,
   after proving the opaque candidate buffer extent and selected-candidate
   semantics; do not assume the pinned reference's decode bound is Apple's
   allocation contract.
2. Implement framework-facing Join completion/abort publication using the
   existing generation-scoped observer, preserving exactly-once terminal
   semantics and requiring association, selected-AP Beacon and controlled-port
   authorization.
3. Verify the Sequoia 15.7 key-material form, implement supported WPA2-PSK
   credential ingress and secret wiping, then prove EAPOL/4-way handshake and
   authorization end-to-end before advertising WPA2.
4. Continue with native interface registration and Skywalk TX/RX ownership.

## Pointer-free native Join request and core admission

The shared ABI defines a fixed 64-byte `Ath10kNativeJoinRequest`. Its operation
generation identifies the Join; `scan_generation` binds the selected AP to the
exact completed scan; BSSID and SSID identify the AP selected by macOS. The
validator accepts OPEN only, requires nonzero generations, a unicast nonzero
BSSID, and an SSID length of 1..32. Generation ordering uses unsigned
half-range serial arithmetic, including wraparound.

The PCI core now exposes `queueNativeJoinRequest()` and
`abortNativeJoinRequest()`, serialized on the WMI work loop. Admission requires
a successful, non-cancelled native scan with the exact requested scan generation,
an idle connection state, and an enabled network interface. It resolves exactly
one BSS by BSSID and SSID (with a bounded hidden-SSID exception), rejects
protected/WPA/RSN and non-infrastructure or incomplete-rate records, then seeds
the existing station state with the selected AP. The existing `selectOpenBSS()`
routine is reused as the final regulatory/channel/rate validator; it cannot
silently select another BSSID. Before preflight, Join admission seeds the same
firmware HT/VHT capability snapshot and RF-chain count that `serviceConnection()`
uses on its Scan-to-Start transition. Both paths now decode only the
WMI_INIT-defined contiguous masks `0x1` (one chain) and `0x3` (two chains);
zero, sparse, or wider masks fail closed instead of being misclassified as two
chains. The QCA9377 PCI INIT path caps the effective hardware-chain count at one,
so its INIT TX/RX masks and subsequent PDEV TX/RX mask commands must all remain
`0x1` even if a firmware report advertises a larger maximum; host regression
tests now check that parity. Both selections therefore negotiate against the
same validated runtime PHY inputs. Failed preflight leaves the station
disconnected.
Join generations must be newer than the last accepted generation; abort is
generation-checked and enters the existing cancellation/drain path.

The C and C++ host ABI tests cover the request size and offsets, open-only
validation, exact BSSID/SSID matching, hidden-SSID handling, protected/RSN and
non-infrastructure rejection, malformed request fields, and serial wrap/half-range
ordering. This is still not WCL callback wiring: the framework-facing interface
must own its own generation and exactly-once completion observer, and the
opaque Apple association-candidate buffer remains subject to the separate 15.7
ABI proof. WPA2 stays unadvertised and rejected until credential ingress and
the full EAPOL/key/controlled-port path are verified.

## Sequoia reference lead for association candidates (not yet approved for decoding)

The pinned Sequoia 15 header declares the WCL virtual as
`setWCL_ASSOCIATE(apple80211AssocCandidates*)` (CamelCase), while also
forward-declaring a distinct-looking `apple80211_assoc_candidates` spelling
for other interface declarations. The implementation must use the exact
override type selected by the 15.7 target header; do not normalize these names
or infer that the two incomplete struct tags are interchangeable. The
candidate payload remains opaque at the public header boundary.


A source-only inspection of the pinned itlwm commit found that its shared WCL
decoder treats the opaque candidate buffer as follows: candidate count at
`+0x214`, candidate records beginning at `+0x21c`, 18 bytes per record, and
the first record's BSSID at record offset `+6` (absolute `+0x222`). It also
reads SSID length/data at `+0x1c/+0x20`, lower/upper auth at `+0x10/+0x14`,
key length/cipher/index/data at `+0x44/+0x48/+0x4e/+0x50`, and RSN length/data
at `+0xd4/+0xd6`. Its call site supplies a decode bound of `0x2cc`.

These are useful reverse-engineering leads from the pinned Sequoia-compatible
reference, not declarations in Apple's public headers and not a validated
15.7 allocation contract. The decoder checks `count <= 10` and verifies
`length >= 0x218 + count * 18`; its call site supplies `0x2cc`, which is
exactly sufficient for ten 18-byte records beginning at `+0x218`. The first
record's BSSID is read at `+0x21c`, i.e. four bytes into that record. This
arithmetic is internally consistent, but the literal `0x2cc` is supplied by
the reference call site rather than obtained from an Apple-declared buffer
length. Before enabling a production decoder, establish that the 15.7 caller
really guarantees at least `0x2cc` readable bytes, plus the exact record
stride/alignment and whether key/RSN fields are always present. Add a local
SDK compile/layout probe and bounded synthetic tests for truncation, overflow,
malformed BSSID, unsupported auth, and inconsistent candidate counts. Never
dereference based only on a count field until the accessible extent is
independently established.

The candidate key is not automatically a WPA2 passphrase: the reference
decoder's supported personal path expects a 32-byte key material field with
specific cipher identifiers. Ath10kMac must separately establish which
credential form macOS supplies on 15.7 and map only the supported WPA2-PSK
case. Do not reinterpret a 32-byte PMK as a human passphrase or forward
unsupported WEP/enterprise/SAE modes into the current core.

The association-candidate structure remains a hard ABI gate. A separate
macOS-26-oriented third-party note reports offsets for that structure, but
those offsets are not evidence for Darwin 24 / Sequoia 15.7 and are expressly
excluded from this design. Only the pinned Sequoia headers/call sites or a
locally verified 15.7 ABI probe may authorize a decoder.

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

## Sequoia 15 message entry-point distinction (source audit)

The pinned Sequoia 15 reference headers expose two different `postMessage`
entry points with different owners and signatures:

- `IO80211Controller::postMessage(IO80211SkywalkInterface*, unsigned int,
  void*, unsigned long, bool)` returns `IOReturn`.
- `IO80211SkywalkInterface::postMessage(unsigned int, void*, unsigned long,
  bool)` returns `void`.

They are overloads on different framework objects, not interchangeable
wrappers. A future publisher must select the interface/controller entry point
from the actual framework contract and must not infer payload ownership,
copying, or completion semantics from the method name or return type.

The pinned Sequoia 15 reference's controller-side event forwarding calls the
four-argument interface overload for several ordinary link/country events.
Its non-WCL scan timer path posts `APPLE80211_M_SCAN_DONE` with a four-byte
local payload, while its WCL scan path calls a separate
`completeWCLScan()` helper. This establishes that the reference has distinct
legacy and WCL scan-completion paths; it does not establish the exact WCL
result/done payload contract for Ath10kMac.

The same header declares
`setWCL_LEAVE_NETWORK(apple80211_leave_network*)` and
`setWCL_JOIN_ABORT(void*)`. These declarations confirm the callback
spellings/types in the pinned Sequoia reference only. They do not establish
the 15.7 callback's buffer extent, completion event identifier, payload
lifetime, or whether a Leave completion must be sent through controller or
interface `postMessage`.

Implementation rule: do not add a production LeaveComplete or ScanDone
publisher until the selected 15.7 target's message identifier, payload shape,
receiver, synchronous-copy behavior, and ordering have independent evidence.
The current Leave observer remains a core-state gate only; it is not evidence
that a particular WCL completion message has been identified.


The Leave observer also preserves a generation high-water mark across
`reset()`. It rejects duplicate, older, zero and half-range-ambiguous
operation generations, while allowing valid RFC-1982 wraparound. Resetting
the interface observer therefore cannot make an old Leave completion token
valid again during the lifetime of that observer instance.
