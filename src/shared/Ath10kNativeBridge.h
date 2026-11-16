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
    uint16_t dwell_min_ms;
    uint16_t dwell_max_ms;
    Ath10kNativeSSID ssids[ATH10K_NATIVE_MAX_SCAN_SSIDS];
    uint16_t channels[ATH10K_NATIVE_MAX_SCAN_CHANNELS];
} Ath10kNativeScanRequest;

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
        h->size >= minimum_size &&
        h->size >= sizeof(Ath10kNativeMessageHeader) &&
        h->payload_bytes == (uint32_t)(h->size - sizeof(Ath10kNativeMessageHeader));
}

static inline int ath10kNativeValidateScanRequest(const Ath10kNativeScanRequest *r) {
    if (!r || !ath10kNativeHeaderValid(&r->header, ATH10K_NATIVE_OP_SCAN_START,
                                       (uint16_t)sizeof(*r))) return 0;
    if (r->header.flags || r->scan_type > ATH10K_NATIVE_SCAN_ACTIVE ||
        r->ssid_count > ATH10K_NATIVE_MAX_SCAN_SSIDS ||
        r->channel_count > ATH10K_NATIVE_MAX_SCAN_CHANNELS ||
        r->has_bssid > 1 ||
        (r->dwell_min_ms && r->dwell_max_ms && r->dwell_min_ms > r->dwell_max_ms))
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
static_assert(offsetof(Ath10kNativeScanRequest, ssids) == 35, "native ABI scan SSID offset");
static_assert(offsetof(Ath10kNativeScanRequest, channels) == 563, "native ABI scan channel offset");
static_assert(sizeof(Ath10kNativeScanRequest) == 691, "native ABI scan request size");
static_assert(sizeof(Ath10kNativeBSSRecord) == 560, "native ABI BSS record size");
static_assert(sizeof(Ath10kNativeKeyRequest) == 70, "native ABI key request size");
static_assert(sizeof(Ath10kNativeLinkSnapshot) == 88, "native ABI link snapshot size");
#else
_Static_assert(sizeof(Ath10kNativeMessageHeader) == 20, "native ABI header size");
_Static_assert(sizeof(Ath10kNativeSSID) == 33, "native ABI SSID size");
_Static_assert(offsetof(Ath10kNativeScanRequest, ssids) == 35, "native ABI scan SSID offset");
_Static_assert(offsetof(Ath10kNativeScanRequest, channels) == 563, "native ABI scan channel offset");
_Static_assert(sizeof(Ath10kNativeScanRequest) == 691, "native ABI scan request size");
_Static_assert(sizeof(Ath10kNativeBSSRecord) == 560, "native ABI BSS record size");
_Static_assert(sizeof(Ath10kNativeKeyRequest) == 70, "native ABI key request size");
_Static_assert(sizeof(Ath10kNativeLinkSnapshot) == 88, "native ABI link snapshot size");
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#undef ATH10K_NATIVE_PACKED
