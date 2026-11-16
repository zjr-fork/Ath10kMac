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
- Link snapshot and capabilities value structures.
- Existing WPA2/CCMP key installation and controlled-port authorization.

## Not yet proven / implementation blockers

- Exact local 15.7 typedef spellings and override signatures for all selected WCL
  callbacks, including abort and association aliases.
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
