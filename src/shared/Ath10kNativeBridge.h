#pragma once

/*
 * Versioned, pointer-free message ABI shared by Ath10kCore and the
 * Sequoia-specific native Wi-Fi frontend.
 *
 * This file deliberately contains no IOKit, Skywalk, mbuf, OSObject, STL,
 * compiler-owned bool, or kernel object declarations. It defines wire/layout
 * contracts only; transport and lifetime rules are specified separately.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ATH10K_NATIVE_ABI_VERSION       1u
#define ATH10K_NATIVE_MAX_SSID          32u
#define ATH10K_NATIVE_MAX_SCAN_SSIDS    16u
#define ATH10K_NATIVE_MAX_SCAN_CHANNELS 64u
#define ATH10K_NATIVE_MAX_BSS_IE_BYTES  512u
#define ATH10K_NATIVE_MAX_KEY_BYTES     32u

#if defined(__GNUC__)
#define ATH10K_NATIVE_PACKED __attribute__((packed))
#else
#define ATH10K_NATIVE_PACKED
#endif

typedef enum Ath10kNativeOperation {
    ATH10K_NATIVE_OP_ACQUIRE = 1,
    ATH10K_NATIVE_OP_RELEASE = 2,
    ATH10K_NATIVE_OP_GET_CAPABILITIES = 3,
    ATH10K_NATIVE_OP_SCAN_START = 4,
    ATH10K_NATIVE_OP_SCAN_ABORT = 5,
    ATH10K_NATIVE_OP_SCAN_SNAPSHOT = 6,
    ATH10K_NATIVE_OP_ASSOCIATE = 7,
    ATH10K_NATIVE_OP_JOIN_ABORT = 8,
    ATH10K_NATIVE_OP_DISCONNECT = 9,
    ATH10K_NATIVE_OP_SET_KEY = 10,
    ATH10K_NATIVE_OP_REMOVE_KEY = 11,
    ATH10K_NATIVE_OP_GET_LINK_SNAPSHOT = 12
} Ath10kNativeOperation;

typedef enum Ath10kNativeStatus {
    ATH10K_NATIVE_STATUS_OK = 0,
    ATH10K_NATIVE_STATUS_PENDING = 1,
    ATH10K_NATIVE_STATUS_BUSY = 2,
    ATH10K_NATIVE_STATUS_NOT_READY = 3,
    ATH10K_NATIVE_STATUS_NOT_SUPPORTED = 4,
    ATH10K_NATIVE_STATUS_INVALID_ARGUMENT = 5,
    ATH10K_NATIVE_STATUS_NO_MEMORY = 6,
    ATH10K_NATIVE_STATUS_TIMEOUT = 7,
    ATH10K_NATIVE_STATUS_CANCELLED = 8,
    ATH10K_NATIVE_STATUS_STALE_GENERATION = 9,
    ATH10K_NATIVE_STATUS_IO_ERROR = 10,
    ATH10K_NATIVE_STATUS_SECURITY_ERROR = 11
} Ath10kNativeStatus;

typedef enum Ath10kNativeScanType {
    ATH10K_NATIVE_SCAN_PASSIVE = 0,
    ATH10K_NATIVE_SCAN_ACTIVE = 1
} Ath10kNativeScanType;

typedef enum Ath10kNativeKeyType {
    ATH10K_NATIVE_KEY_PAIRWISE = 0,
    ATH10K_NATIVE_KEY_GROUP = 1,
    ATH10K_NATIVE_KEY_IGTK = 2
} Ath10kNativeKeyType;

typedef enum Ath10kNativeCipher {
    ATH10K_NATIVE_CIPHER_NONE = 0,
    ATH10K_NATIVE_CIPHER_CCMP_128 = 1,
    ATH10K_NATIVE_CIPHER_TKIP = 2
} Ath10kNativeCipher;

typedef enum Ath10kNativeCapabilityFlags {
    ATH10K_NATIVE_CAP_STATION = 1u << 0,
    ATH10K_NATIVE_CAP_SCAN_ACTIVE = 1u << 1,
    ATH10K_NATIVE_CAP_SCAN_PASSIVE = 1u << 2,
    ATH10K_NATIVE_CAP_WPA2_PSK = 1u << 3,
    ATH10K_NATIVE_CAP_BAND_2GHZ = 1u << 4,
    ATH10K_NATIVE_CAP_BAND_5GHZ = 1u << 5,
    ATH10K_NATIVE_CAP_HT = 1u << 6,
    ATH10K_NATIVE_CAP_VHT = 1u << 7
} Ath10kNativeCapabilityFlags;

typedef enum Ath10kNativeCipherFlags {
    ATH10K_NATIVE_CAP_CIPHER_CCMP128_PAIRWISE = 1u << 0,
    ATH10K_NATIVE_CAP_CIPHER_TKIP_GROUP = 1u << 1
} Ath10kNativeCipherFlags;

typedef enum Ath10kNativeLinkState {
    ATH10K_NATIVE_LINK_IDLE = 0,
    ATH10K_NATIVE_LINK_SCANNING = 1,
    ATH10K_NATIVE_LINK_ASSOCIATING = 2,
    ATH10K_NATIVE_LINK_CONNECTED = 3,
    ATH10K_NATIVE_LINK_DISCONNECTING = 4,
    ATH10K_NATIVE_LINK_FAILED = 5
} Ath10kNativeLinkState;

typedef enum Ath10kNativeScanResultState {
    ATH10K_NATIVE_SCAN_PENDING = 0,
    ATH10K_NATIVE_SCAN_SUCCEEDED = 1,
    ATH10K_NATIVE_SCAN_FAILED = 2,
    ATH10K_NATIVE_SCAN_CANCELLED = 3,
    ATH10K_NATIVE_SCAN_TIMED_OUT = 4
} Ath10kNativeScanResultState;

typedef enum Ath10kNativeBSSFlags {
    ATH10K_NATIVE_BSS_HIDDEN = 1u << 0,
    ATH10K_NATIVE_BSS_PRIVACY = 1u << 1,
    ATH10K_NATIVE_BSS_HAS_RSN = 1u << 2,
    ATH10K_NATIVE_BSS_RSSI_ESTIMATED = 1u << 3,
    ATH10K_NATIVE_BSS_RATES_VALID = 1u << 4,
    ATH10K_NATIVE_BSS_HAS_WPA = 1u << 5
} Ath10kNativeBSSFlags;

#pragma pack(push, 1)

typedef struct ATH10K_NATIVE_PACKED Ath10kNativeMessageHeader {
    uint16_t version;
    uint16_t size;
    uint16_t operation;
    uint16_t flags;
    uint32_t generation;
    int32_t status;
    uint32_t payload_bytes;
} Ath10kNativeMessageHeader;

typedef struct ATH10K_NATIVE_PACKED Ath10kNativeSSID {
    uint8_t length;
    uint8_t bytes[ATH10K_NATIVE_MAX_SSID];
} Ath10kNativeSSID;

typedef struct ATH10K_NATIVE_PACKED Ath10kNativeScanRequest {
    Ath10kNativeMessageHeader header;
    uint8_t scan_type;
    uint8_t ssid_count;
    uint8_t has_bssid;
    uint8_t bssid[6];
    uint16_t channel_count;
    uint16_t active_dwell_ms;
    uint16_t passive_dwell_ms;
    Ath10kNativeSSID ssids[ATH10K_NATIVE_MAX_SCAN_SSIDS];
    uint16_t channels[ATH10K_NATIVE_MAX_SCAN_CHANNELS];
} Ath10kNativeScanRequest;

typedef enum Ath10kNativeSecurityMode {
    ATH10K_NATIVE_SECURITY_OPEN = 0,
    ATH10K_NATIVE_SECURITY_WPA2_PSK = 1
} Ath10kNativeSecurityMode;

/* Pointer-free Join request. scan_generation binds the selected AP to the
 * exact completed BSS snapshot from which the frontend chose it. Version 1
 * deliberately accepts OPEN only; WPA2 remains rejected until credential
 * ingress and the complete EAPOL/key/port-authorization path are verified. */
typedef struct ATH10K_NATIVE_PACKED Ath10kNativeJoinRequest {
    Ath10kNativeMessageHeader header;
    uint32_t scan_generation;
    uint8_t security_mode;
    uint8_t ssid_length;
    uint8_t bssid[6];
    uint8_t ssid[ATH10K_NATIVE_MAX_SSID];
} Ath10kNativeJoinRequest;

typedef struct ATH10K_NATIVE_PACKED Ath10kNativeScanStatus {
    uint32_t generation;
    uint32_t state;
    uint32_t reason;
    uint32_t result_count;
} Ath10kNativeScanStatus;

/*
 * BSS records are enumerable only for a nonzero, successfully completed
 * native scan generation. A successful empty scan is still a valid result
 * set; result_count may therefore be zero.
 */
static inline int ath10kNativeScanStatusHasResults(const Ath10kNativeScanStatus *status) {
    return status != 0 && status->generation != 0 &&
        status->state == ATH10K_NATIVE_SCAN_SUCCEEDED;
}

typedef struct ATH10K_NATIVE_PACKED Ath10kNativeBSSRecord {
    uint8_t bssid[6];
    uint8_t ssid_length;
    uint8_t flags;
    uint16_t channel;
    uint16_t frequency_mhz;
    int8_t rssi_dbm;
    int8_t noise_dbm;
    uint16_t ie_length;
    uint8_t ssid[ATH10K_NATIVE_MAX_SSID];
    uint8_t ies[ATH10K_NATIVE_MAX_BSS_IE_BYTES];
    uint16_t capabilities;
    uint16_t beacon_interval;
    uint8_t rate_count;
    uint8_t rates[32];
} Ath10kNativeBSSRecord;

typedef struct ATH10K_NATIVE_PACKED Ath10kNativeKeyRequest {
    Ath10kNativeMessageHeader header;
    uint8_t key_type;
    uint8_t cipher;
    uint8_t key_index;
    uint8_t key_length;
    uint8_t peer[6];
    uint8_t rsc[8];
    uint8_t key[ATH10K_NATIVE_MAX_KEY_BYTES];
} Ath10kNativeKeyRequest;

typedef struct ATH10K_NATIVE_PACKED Ath10kNativeCapabilities {
    Ath10kNativeMessageHeader header;
    uint32_t feature_flags;
    uint32_t cipher_flags;
    uint16_t channel_count;
    uint16_t max_scan_channels;
    uint8_t max_ssid_length;
    uint8_t max_scan_ssids;
    uint16_t max_bss_ie_bytes;
    uint16_t channels[ATH10K_NATIVE_MAX_SCAN_CHANNELS];
} Ath10kNativeCapabilities;

typedef struct ATH10K_NATIVE_PACKED Ath10kNativeLinkSnapshot {
    Ath10kNativeMessageHeader header;
    uint8_t state;
    uint8_t ssid_length;
    uint8_t bssid[6];
    uint16_t channel;
    int8_t rssi_dbm;
    uint8_t security;
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint64_t tx_bytes;
    uint64_t rx_bytes;
    uint8_t ssid[ATH10K_NATIVE_MAX_SSID];
} Ath10kNativeLinkSnapshot;

/* Internal core-to-native-frontend observation. Counters are monotonic serials;
 * consumers compare them with their per-operation baselines using RFC-1982
 * half-range ordering. This is not a user-client request structure. */
typedef struct ATH10K_NATIVE_PACKED Ath10kNativeJoinSnapshot {
    uint32_t link_stage;
    uint32_t association_status;
    uint32_t station_beacon_serial;
    uint32_t security_generation;
    uint32_t disconnect_stage;
    uint8_t secure;
    uint8_t port_authorized;
    uint8_t cancel_requested;
    uint8_t reserved;
    uint8_t bssid[6];
    uint16_t channel;
} Ath10kNativeJoinSnapshot;

#pragma pack(pop)

/* Validation helpers are pure and side-effect free. They never log or copy
 * key bytes. Regulatory channel acceptance remains a core-driver decision. */
static inline int ath10kNativeHeaderValid(const Ath10kNativeMessageHeader *h,
                                          uint16_t operation,
                                          uint16_t minimum_size) {
    return h &&
        h->version == ATH10K_NATIVE_ABI_VERSION &&
        h->operation == operation &&
        h->generation != 0 &&
        h->size == minimum_size &&
        h->size >= sizeof(Ath10kNativeMessageHeader) &&
        h->payload_bytes == (uint32_t)(h->size - sizeof(Ath10kNativeMessageHeader));
}

static inline int ath10kNativeGenerationIsNewer(uint32_t candidate,uint32_t current) {
    if (!candidate) return 0;
    const uint32_t delta=candidate-current;
    return delta!=0 && delta<0x80000000u;
}

static inline int ath10kNativeValidateJoinRequest(const Ath10kNativeJoinRequest *r) {
    if (!r || !ath10kNativeHeaderValid(&r->header, ATH10K_NATIVE_OP_ASSOCIATE,
                                       (uint16_t)sizeof(*r))) return 0;
    if (r->header.flags || !r->scan_generation ||
        r->security_mode != ATH10K_NATIVE_SECURITY_OPEN ||
        !r->ssid_length || r->ssid_length > ATH10K_NATIVE_MAX_SSID) return 0;
    uint8_t any = 0;
    for (uint32_t i = 0; i < 6; ++i) any |= r->bssid[i];
    if (!any || (r->bssid[0] & 1u)) return 0;
    return 1;
}

static inline int ath10kNativeOpenJoinMatchesBSS(const Ath10kNativeJoinRequest *request,
                                                        const Ath10kNativeBSSRecord *bss) {
    if (!ath10kNativeValidateJoinRequest(request) || !bss) return 0;
    if (bss->flags & (ATH10K_NATIVE_BSS_PRIVACY|ATH10K_NATIVE_BSS_HAS_RSN|
                      ATH10K_NATIVE_BSS_HAS_WPA)) return 0;
    if (bss->ssid_length > ATH10K_NATIVE_MAX_SSID ||
        !(bss->capabilities & 1u) || (bss->capabilities & 2u) ||
        !bss->beacon_interval || !bss->rate_count ||
        bss->rate_count > sizeof(bss->rates) ||
        !(bss->flags & ATH10K_NATIVE_BSS_RATES_VALID)) return 0;
    for (uint32_t i=0;i<6;++i) if (bss->bssid[i]!=request->bssid[i]) return 0;
    if ((bss->flags & ATH10K_NATIVE_BSS_HIDDEN) && !bss->ssid_length) return 1;
    if (bss->ssid_length!=request->ssid_length) return 0;
    for (uint32_t i=0;i<request->ssid_length;++i)
        if (bss->ssid[i]!=request->ssid[i]) return 0;
    return 1;
}

static inline int ath10kNativeValidateScanRequest(const Ath10kNativeScanRequest *r) {
    if (!r || !ath10kNativeHeaderValid(&r->header, ATH10K_NATIVE_OP_SCAN_START,
                                       (uint16_t)sizeof(*r))) return 0;
    if (r->header.flags || r->scan_type > ATH10K_NATIVE_SCAN_ACTIVE ||
        r->ssid_count > ATH10K_NATIVE_MAX_SCAN_SSIDS ||
        r->channel_count > ATH10K_NATIVE_MAX_SCAN_CHANNELS ||
        r->has_bssid > 1 ||
        (r->active_dwell_ms && (r->active_dwell_ms < 40 || r->active_dwell_ms > 2000)) ||
        (r->passive_dwell_ms && (r->passive_dwell_ms < 40 || r->passive_dwell_ms > 2000)))
        return 0;
    if (r->has_bssid) {
        uint8_t any = 0;
        for (uint32_t i = 0; i < 6; ++i) any |= r->bssid[i];
        if (!any || (r->bssid[0] & 1u)) return 0;
    }
    for (uint32_t i = 0; i < r->ssid_count; ++i)
        if (!r->ssids[i].length || r->ssids[i].length > ATH10K_NATIVE_MAX_SSID)
            return 0;
    for (uint32_t i = 0; i < r->channel_count; ++i)
        if (!r->channels[i]) return 0;
    return 1;
}

/* Sequoia 15.x WCL scan-request wire layout, decoded without Apple C++ ABI
 * objects crossing the core boundary. The frontend passes the fixed request
 * allocation size it was given by IO80211 (0x1550 bytes in the pinned ABI). */
#define ATH10K_NATIVE_APPLE_SCAN_MIN_BYTES 0x58u
#define ATH10K_NATIVE_APPLE_SCAN_MAX_BYTES 0x1550u
#define ATH10K_NATIVE_APPLE_SCAN_MAX_CHANNEL_ENTRIES 400u

static inline uint32_t ath10kNativeReadLE32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline int ath10kNativeDecodeAppleScanRequest(const void *buffer,
                                                      uint32_t length,
                                                      uint32_t generation,
                                                      Ath10kNativeScanRequest *out) {
    if (!buffer || !out || !generation ||
        length < ATH10K_NATIVE_APPLE_SCAN_MIN_BYTES ||
        length > ATH10K_NATIVE_APPLE_SCAN_MAX_BYTES) return 0;
    const uint8_t *p = (const uint8_t *)buffer;
    const uint32_t ssid_length = ath10kNativeReadLE32(p + 0x1c);
    const uint32_t scan_type = ath10kNativeReadLE32(p + 0x40);
    const uint32_t channel_entries = ath10kNativeReadLE32(p + 0x54);
    if (ssid_length > ATH10K_NATIVE_MAX_SSID || scan_type < 1 || scan_type > 4 ||
        channel_entries > ATH10K_NATIVE_APPLE_SCAN_MAX_CHANNEL_ENTRIES ||
        channel_entries > (length - ATH10K_NATIVE_APPLE_SCAN_MIN_BYTES) / 12u)
        return 0;

    uint8_t *dst = (uint8_t *)out;
    for (uint32_t i = 0; i < sizeof(*out); ++i) dst[i] = 0;
    out->header.version = ATH10K_NATIVE_ABI_VERSION;
    out->header.size = (uint16_t)sizeof(*out);
    out->header.operation = ATH10K_NATIVE_OP_SCAN_START;
    out->header.generation = generation;
    out->header.payload_bytes = (uint32_t)(sizeof(*out) - sizeof(out->header));
    out->scan_type = scan_type == 2 ? ATH10K_NATIVE_SCAN_PASSIVE : ATH10K_NATIVE_SCAN_ACTIVE;

    if (ssid_length) {
        out->ssid_count = 1;
        out->ssids[0].length = (uint8_t)ssid_length;
        for (uint32_t i = 0; i < ssid_length; ++i) out->ssids[0].bytes[i] = p[0x20 + i];
    }

    uint8_t bssid_nonzero = 0, bssid_not_broadcast = 0;
    for (uint32_t i = 0; i < 6; ++i) {
        out->bssid[i] = p[0x14 + i];
        bssid_nonzero |= out->bssid[i];
        bssid_not_broadcast |= (uint8_t)(out->bssid[i] ^ 0xffu);
    }
    if (bssid_nonzero && bssid_not_broadcast) {
        if (out->bssid[0] & 1u) return 0;
        out->has_bssid = 1;
    } else {
        for (uint32_t i = 0; i < 6; ++i) out->bssid[i] = 0;
    }

    for (uint32_t i = 0; i < channel_entries; ++i) {
        const uint8_t *entry = p + ATH10K_NATIVE_APPLE_SCAN_MIN_BYTES + 12u * i;
        const uint32_t channel = ath10kNativeReadLE32(entry + 4);
        const uint32_t flags = ath10kNativeReadLE32(entry + 8);
        if (!channel || channel > 196 || (flags & 0x2000u) ||
            ((flags & 0x8u) && channel > 14) ||
            ((flags & 0x10u) && channel <= 14)) continue;
        int duplicate = 0;
        for (uint32_t j = 0; j < out->channel_count; ++j)
            if (out->channels[j] == channel) duplicate = 1;
        if (duplicate) continue;
        if (out->channel_count >= ATH10K_NATIVE_MAX_SCAN_CHANNELS) return 0;
        out->channels[out->channel_count++] = (uint16_t)channel;
    }
    if (channel_entries && !out->channel_count) return 0;
    return ath10kNativeValidateScanRequest(out);
}

static inline int ath10kNativeValidateKeyRequest(const Ath10kNativeKeyRequest *r) {
    if (!r || !ath10kNativeHeaderValid(&r->header, ATH10K_NATIVE_OP_SET_KEY,
                                       (uint16_t)sizeof(*r))) return 0;
    if (r->header.flags || r->key_index > 3 ||
        r->key_type > ATH10K_NATIVE_KEY_GROUP) return 0;
    if (r->cipher == ATH10K_NATIVE_CIPHER_CCMP_128)
        return r->key_length == 16;
    if (r->cipher == ATH10K_NATIVE_CIPHER_TKIP)
        return r->key_type == ATH10K_NATIVE_KEY_GROUP && r->key_length == 32;
    return 0;
}

#if defined(__cplusplus)
static_assert(sizeof(Ath10kNativeMessageHeader) == 20, "native ABI header size");
static_assert(sizeof(Ath10kNativeSSID) == 33, "native ABI SSID size");
static_assert(sizeof(Ath10kNativeJoinRequest) == 64, "native Join request size");
static_assert(offsetof(Ath10kNativeJoinRequest, scan_generation) == 20, "native Join scan generation offset");
static_assert(offsetof(Ath10kNativeJoinRequest, bssid) == 26, "native Join BSSID offset");
static_assert(offsetof(Ath10kNativeJoinRequest, ssid) == 32, "native Join SSID offset");
static_assert(sizeof(Ath10kNativeScanStatus) == 16, "native scan status size");
static_assert(offsetof(Ath10kNativeScanRequest, ssids) == 35, "native ABI scan SSID offset");
static_assert(offsetof(Ath10kNativeScanRequest, channels) == 563, "native ABI scan channel offset");
static_assert(sizeof(Ath10kNativeScanRequest) == 691, "native ABI scan request size");
static_assert(sizeof(Ath10kNativeBSSRecord) == 597, "native ABI BSS record size");
static_assert(offsetof(Ath10kNativeBSSRecord, capabilities) == 560, "native ABI BSS capabilities offset");
static_assert(offsetof(Ath10kNativeBSSRecord, rates) == 565, "native ABI BSS rates offset");
static_assert(sizeof(Ath10kNativeKeyRequest) == 70, "native ABI key request size");
static_assert(sizeof(Ath10kNativeCapabilities) == 164, "native capabilities size");
static_assert(sizeof(Ath10kNativeLinkSnapshot) == 88, "native ABI link snapshot size");
static_assert(sizeof(Ath10kNativeJoinSnapshot) == 32, "native ABI join snapshot size");
#else
_Static_assert(sizeof(Ath10kNativeMessageHeader) == 20, "native ABI header size");
_Static_assert(sizeof(Ath10kNativeSSID) == 33, "native ABI SSID size");
_Static_assert(sizeof(Ath10kNativeJoinRequest) == 64, "native Join request size");
_Static_assert(offsetof(Ath10kNativeJoinRequest, scan_generation) == 20, "native Join scan generation offset");
_Static_assert(offsetof(Ath10kNativeJoinRequest, bssid) == 26, "native Join BSSID offset");
_Static_assert(offsetof(Ath10kNativeJoinRequest, ssid) == 32, "native Join SSID offset");
_Static_assert(sizeof(Ath10kNativeScanStatus) == 16, "native scan status size");
_Static_assert(offsetof(Ath10kNativeScanRequest, ssids) == 35, "native ABI scan SSID offset");
_Static_assert(offsetof(Ath10kNativeScanRequest, channels) == 563, "native ABI scan channel offset");
_Static_assert(sizeof(Ath10kNativeScanRequest) == 691, "native ABI scan request size");
_Static_assert(sizeof(Ath10kNativeBSSRecord) == 597, "native ABI BSS record size");
_Static_assert(offsetof(Ath10kNativeBSSRecord, capabilities) == 560, "native ABI BSS capabilities offset");
_Static_assert(offsetof(Ath10kNativeBSSRecord, rates) == 565, "native ABI BSS rates offset");
_Static_assert(sizeof(Ath10kNativeKeyRequest) == 70, "native ABI key request size");
_Static_assert(sizeof(Ath10kNativeCapabilities) == 164, "native capabilities size");
_Static_assert(sizeof(Ath10kNativeLinkSnapshot) == 88, "native ABI link snapshot size");
_Static_assert(sizeof(Ath10kNativeJoinSnapshot) == 32, "native ABI join snapshot size");
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#undef ATH10K_NATIVE_PACKED
