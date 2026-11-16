#include "../src/shared/Ath10kNativeBridge.h"

int main(void) {
    Ath10kNativeMessageHeader header = {0};
    header.version = ATH10K_NATIVE_ABI_VERSION;
    header.operation = ATH10K_NATIVE_OP_GET_LINK_SNAPSHOT;
    if (sizeof(header) != 20u) return 1;
    if (sizeof(Ath10kNativeLinkSnapshot) != 88u) return 2;
    if (sizeof(Ath10kNativeBSSRecord) != 597u) return 3;

    Ath10kNativeScanRequest scan = {0};
    scan.header.version = ATH10K_NATIVE_ABI_VERSION;
    scan.header.size = (uint16_t)sizeof(scan);
    scan.header.operation = ATH10K_NATIVE_OP_SCAN_START;
    scan.header.generation = 1;
    scan.header.payload_bytes = (uint32_t)(sizeof(scan) - sizeof(scan.header));
    scan.scan_type = ATH10K_NATIVE_SCAN_PASSIVE;
    scan.channel_count = 1;
    scan.channels[0] = 1;
    if (!ath10kNativeValidateScanRequest(&scan)) return 4;
    scan.header.size++;
    if (ath10kNativeValidateScanRequest(&scan)) return 5;
    scan.header.size--;
    scan.channel_count = ATH10K_NATIVE_MAX_SCAN_CHANNELS + 1;
    if (ath10kNativeValidateScanRequest(&scan)) return 6;
    return 0;
}
