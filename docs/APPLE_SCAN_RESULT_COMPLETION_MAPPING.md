# Sequoia Apple scan result and completion mapping

This note records the observable macOS 15 behavior at the pinned reference revision
`laobamac/itlwm@7204c69d1bfa7f5cc0f7864e08fb03e55d0ec29e`. It is an ABI/behavior
mapping only. No reference implementation code or third-party headers are copied into
Ath10kMac.

## Two distinct result surfaces

The Sequoia reference exposes two related but distinct scan-result paths:

1. **Apple80211 result getter**: `IO80211InfraProtocol::getSCAN_RESULT(apple80211_scan_result *)`
   is an iterator over the controller's BSS tree. The reference keeps a
   `fNextNodeToSend` cursor and a wrap/exhaustion flag, converts one BSS node into one
   `apple80211_scan_result`, and advances the cursor. It reports an end-of-results
   status after the final entry. The literal status value used by that reference must
   not be copied as an ABI constant; resolve the correct status against the local
   Sequoia SDK before implementing it.

2. **WCL asynchronous event stream**: `setWCL_SCAN_REQ(apple80211ScanRequest *)`
   starts a WCL scan, while scan results are published as WCL `ScanResult` messages
   carrying a `BeaconMetadata` prefix followed by validated information elements.
   Completion is a separate `ScanDone` message carrying an `IOReturn`. The WCL
   abort entry point is `setWCL_SCAN_ABORT(void *)`.

Do not conflate these paths. A successful WMI transaction is not by itself an
Apple-visible scan completion, and a result getter returning end-of-iteration is not
the asynchronous scan-completion notification.

## Request admission and lifecycle observations

At the pinned revision, WCL request and abort callbacks enter a command gate. The
request path:

- decodes and bounds-checks the Apple request before touching scan state;
- requires the interface to be running;
- rejects a concurrent scan, join, or background scan;
- rejects AUTH/ASSOC states;
- rejects an explicit channel list when no channel remains usable;
- records a pending request and arms a 20-second watchdog;
- distinguishes an already-running scan from a newly initiated scan, including a
  case where an existing scan must not satisfy the new request's completion.

The abort callback enters the same gate and terminates the pending WCL request with
an aborted status. The completion path is guarded by the pending flag, cancels the
watchdog, clears the requested flag, and posts exactly one terminal notification.
The scan-done firmware/MLME event schedules completion processing rather than
publishing completion inline from the event handler. A watchdog expiration is
reported as timeout, not success.

These observations support the following Ath10kMac rules:

- Keep WCL request admission and state changes serialized on the interface command
  gate; never block that gate while waiting for firmware.
- Retain the device-generated scan generation for every accepted request.
- Abort only the matching generation when available; otherwise abort the current
  native request.
- Do not notify Apple until the core reports a terminal state after the START/STOP
  descriptor and HTC-credit ownership barriers have been satisfied.
- Make completion idempotent. A late firmware event, timeout, abort, or interface
  shutdown must not post a second completion for the same generation.
- Reject or explicitly report busy/not-ready conditions; never silently broaden an
  invalid restricted-channel request into a full-band scan.

## BSS-to-Apple result conversion

The reference's Apple80211 getter converts one BSS node at a time and fills the
Apple result with the version, beacon interval, capabilities, channel and channel
flags, signal/noise fields, BSSID, SSID, supported rates, and information elements.
The WCL event path separately validates the complete IE stream and includes it after
its fixed metadata prefix.

Ath10kMac already has a bounded, generation-checked `Ath10kNativeBSSRecord` copy
API. The native frontend should convert from that value object only; it must never
retain a pointer into `_wmiBSS` or call into the mutable cache outside the WMI work
loop. The record already contains BSSID, SSID, channel/frequency, estimated RSSI,
capability bits, beacon interval, rates, and bounded raw IE bytes.

Conversion requirements:

- Validate every destination capacity and every source length before copying.
- Preserve hidden SSIDs as zero-length SSIDs; do not synthesize a name.
- Preserve the complete validated IE sequence when the Apple structure has capacity.
  If the Apple result ABI only permits a smaller IE payload, truncate only at a
  complete IE boundary and report the actual copied length.
- Do not invent per-BSS noise. The current bridge marks RSSI as estimated and uses
  zero for unavailable noise; preserve that distinction in the Apple conversion.
- Map channel number to Apple channel flags using the pinned Sequoia SDK definitions,
  not guessed band bits.
- Set the result version and all length/count fields before returning the record.
- Keep the iterator snapshot stable for the duration of one generation. Reset its
  cursor when a new generation is accepted; reject stale generation access.

## Pinned reference result-buffer layout

The pinned AirportItlwm revision's `apple80211_var.h` declares
`apple80211_scan_result` with a pointer-valued `asr_ie_data` field. It is **not**
an inline 1024-byte IE array. The declaration is not marked as a whole-structure
packed type; only the small `unk` member carries packed/aligned(1) attributes.
For the x86_64 ABI used by this project, the declaration implies these offsets:

| Offset | Field | Size |
|---:|---|---:|
| `0x00` | version | 4 |
| `0x04` | channel (version/channel/flags) | 12 |
| `0x10` | unknown | 2 |
| `0x12` | noise | 2 |
| `0x14` | SNR | 2 |
| `0x16` | RSSI | 2 |
| `0x18` | beacon interval | 2 |
| `0x1a` | capability | 2 |
| `0x1c` | BSSID | 6 |
| `0x22` | rate count | 1 |
| `0x23` | unknown byte | 1 |
| `0x24` | 15 rate values (32-bit each) | 60 |
| `0x60` | SSID length | 1 |
| `0x61` | SSID | 32 |
| `0x81` | packed unknown field | 2 |
| `0x83` | unknown byte | 1 |
| `0x84` | age | 4 |
| `0x88` | unknown 16-bit field | 2 |
| `0x8a` | IE length | 2 |
| `0x8c` | unknown 32-bit field | 4 |
| `0x90` | `asr_ie_data` pointer | 8 |
| `0x98` | end / structure size | — |

These offsets and the expected `sizeof == 0x98` are derived from the pinned
reference declaration for x86_64. They must be confirmed by compiling against the
local macOS 15.7 SDK before any converter uses them. The IE pointer is caller-owned
storage whose capacity is not encoded in the structure; never assume 1024 bytes
unless the caller's contract explicitly supplies that capacity.

The pinned reference's `convertNodeToScanResult` uses
`memcpy(result->asr_ie_data, ..., MIN(result->asr_ie_len, sizeof(result->asr_ie_data)))`.
Because `asr_ie_data` is a pointer in the pinned header, `sizeof` here is pointer
size, not the destination allocation capacity. This expression is not a valid
capacity contract and must not be copied. The reference also does not demonstrate
how the caller allocates or sizes that IE buffer. Before supporting IE copying in
the native getter, locate and verify the actual caller allocation/ownership contract
in the target macOS 15.7 path. Until then, populate scalar fields only and set
`asr_ie_len = 0` rather than dereferencing an unverified pointer.

The bridge currently exports up to 32 rate bytes and 512 IE bytes. The converter
must cap rates at the Apple structure's 15-entry limit, widen each rate byte into
the destination's 32-bit slot, and copy IE bytes only when the caller-provided
buffer capacity is independently known. It must set the actual copied IE length.
The bridge's 512-byte IE bound is an intentional current limitation, not a reason
to read beyond the record or write beyond the caller's buffer.


### Local compile probe

A repository-owned compile-only probe now checks the x86_64 offsets, pointer field,
15-rate capacity, and expected size against the locally installed
`MacKernelSDK/Headers/IOKit/80211/apple80211_var.h`:

```sh
sh scripts/check-apple-scan-result-abi.sh
# or
make check-apple-scan-result-abi
```

The probe deliberately does not vendor Apple headers and has not been executed in
this development environment. A passing result on the user's macOS 15.7 toolchain
is a prerequisite to implementing the result converter.

## Implementation sequence

1. Obtain the local macOS 15 SDK headers and verify the exact
   `apple80211_scan_result` size, field offsets, IE capacity, channel flag constants,
   and end-of-results return code. Do not vendor the headers.
2. Add compile-time assertions for every Apple result field used by the converter.
3. Add a small pure conversion helper that accepts an
   `Ath10kNativeBSSRecord` and a caller-owned Apple result buffer. Unit-test bounds,
   hidden SSID, rates, IE truncation, and unavailable noise.
4. Add a generation-scoped result cursor to the native interface. The cursor must
   iterate only a completed generation's stable BSS snapshot and must not outlive
   interface stop/free.
5. Wire `setWCL_SCAN_REQ` and `setWCL_SCAN_ABORT` through the command gate to the
   existing device queue/abort APIs. Poll or receive terminal status without blocking
   the gate.
6. On terminal success, publish the result records in deterministic order and then
   post one WCL `ScanDone` success. On abort, timeout, firmware failure, or shutdown,
   post the matching terminal status exactly once and do not report success.
7. Run bridge tests and the Sequoia SDK compile probe on macOS 15.7 before attempting
   a hardware scan. Hardware validation remains required.

## Explicitly unresolved

- The exact local SDK layout and capacity of `apple80211_scan_result` have not yet
  been compiler-verified in this repository.
- The reference's literal end-of-iterator status is not treated as a public ABI
  constant until confirmed in the local SDK.
- The exact WCL message transport/consumer contract for result ordering and batch
  sizing must be checked against the local Sequoia SDK/runtime before implementation.
- No macOS build, kext load, scan, or hardware test is claimed by this document.

## Result IE pointer ownership: reference implementation is not a capacity contract

A second pass compared both Sequoia-facing implementations in the pinned reference:

- The legacy Apple80211 command path obtains its result object from
  `fNextNodeToSend->verb`, zeroes that object, assigns the RSN IE pointer on
  pre-Monterey targets, and on newer targets copies into the pre-existing
  `result->asr_ie_data` pointer. This shows that the legacy path relies on
  per-node preallocated result storage, not on an inline IE array.
- The WCL Skywalk getter instead receives an `apple80211_scan_result *sr`
  argument and passes it to a converter. That converter calls
  `memcpy(result->asr_ie_data, ..., MIN(result->asr_ie_len,
  sizeof(result->asr_ie_data)))`. The local Sequoia header confirms
  `asr_ie_data` is a `void *`; therefore `sizeof(result->asr_ie_data)`
  is only the pointer width (8 bytes on x86_64), not the destination capacity.
  The implementation does not establish who allocated that buffer or its
  actual size. This expression is not a safe capacity check and must not be
  replicated.

Consequently the native frontend's initial `getSCAN_RESULT` implementation
must populate only the scalar/fixed-array fields whose capacities are defined
by `apple80211_scan_result` (including a rate count clamped to
`APPLE80211_MAX_RATES`) and set `asr_ie_len = 0`. It must not dereference
`asr_ie_data`, assign a pointer into the internal BSS cache, or advertise IE
bytes until the macOS 15.7 caller's allocation/ownership contract and an
explicit capacity source are established. This deliberately withholds scan IE
payloads rather than risking an out-of-bounds write or a dangling pointer.

The result iterator must also be per-interface and serialized with scan-cache
mutation. It should hold a generation and index, reset when a new scan
generation becomes visible, return one immutable record per call, and return
the framework's verified end-of-iteration status only after the final record.
It must not retain a raw pointer into the mutable BSS cache across calls.
