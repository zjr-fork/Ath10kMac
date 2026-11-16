#include "../src/shared/Ath10kNativeBridge.h"

static_assert(ATH10K_NATIVE_ABI_VERSION == 1u, "ABI version");
static_assert(ATH10K_NATIVE_MAX_SCAN_SSIDS == 16u, "bounded SSID filters");
static_assert(ATH10K_NATIVE_MAX_SCAN_CHANNELS == 64u, "bounded channel list");
static_assert(ATH10K_NATIVE_MAX_BSS_IE_BYTES == 512u, "bounded information elements");

int main() {
    Ath10kNativeScanRequest scan{};
    scan.header.version = ATH10K_NATIVE_ABI_VERSION;
    scan.header.size = static_cast<uint16_t>(sizeof(scan));
    scan.header.operation = ATH10K_NATIVE_OP_SCAN_START;
    scan.header.generation = 1;
    scan.header.payload_bytes = sizeof(scan) - sizeof(scan.header);
    scan.scan_type = ATH10K_NATIVE_SCAN_PASSIVE;
    scan.ssid_count = 1;
    scan.ssids[0].length = 3;
    scan.channel_count = 1;
    scan.channels[0] = 36;
    if (scan.header.size != 691u) return 1;
    if (offsetof(Ath10kNativeScanRequest, channels) != 563u) return 2;
    if (!ath10kNativeValidateScanRequest(&scan)) return 3;
    scan.ssids[0].length = 33;
    if (ath10kNativeValidateScanRequest(&scan)) return 4;

    Ath10kNativeKeyRequest key{};
    key.header.version = ATH10K_NATIVE_ABI_VERSION;
    key.header.size = static_cast<uint16_t>(sizeof(key));
    key.header.operation = ATH10K_NATIVE_OP_SET_KEY;
    key.header.generation = 1;
    key.header.payload_bytes = sizeof(key) - sizeof(key.header);
    key.key_type = ATH10K_NATIVE_KEY_PAIRWISE;
    key.cipher = ATH10K_NATIVE_CIPHER_CCMP_128;
    key.key_length = 16;
    if (sizeof(key) != 70u || !ath10kNativeValidateKeyRequest(&key)) return 5;
    key.cipher = ATH10K_NATIVE_CIPHER_TKIP;
    key.key_length = 32;
    if (ath10kNativeValidateKeyRequest(&key)) return 6;
    return 0;
}
