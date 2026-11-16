#include "../src/shared/Ath10kNativeBridge.h"

int main(void) {
    Ath10kNativeMessageHeader header = {0};
    header.version = ATH10K_NATIVE_ABI_VERSION;
    header.operation = ATH10K_NATIVE_OP_GET_LINK_SNAPSHOT;
    if (sizeof(header) != 20u) return 1;
    if (sizeof(Ath10kNativeLinkSnapshot) != 88u) return 2;
    if (sizeof(Ath10kNativeJoinSnapshot) != 32u) return 13;
    if (offsetof(Ath10kNativeJoinSnapshot, station_beacon_serial) != 8u) return 14;
    if (sizeof(Ath10kNativeCapabilities) != 164u) return 12;
    if (sizeof(Ath10kNativeJoinRequest) != 64u) return 15;
    if (offsetof(Ath10kNativeJoinRequest, scan_generation) != 20u) return 16;
    if (offsetof(Ath10kNativeJoinRequest, bssid) != 26u) return 17;
    if (offsetof(Ath10kNativeJoinRequest, ssid) != 32u) return 18;
    if (sizeof(Ath10kNativeScanStatus) != 16u) return 7;
    if (ATH10K_NATIVE_LINK_IDLE != 0 || ATH10K_NATIVE_LINK_CONNECTED != 3) return 11;
    if (sizeof(Ath10kNativeBSSRecord) != 597u) return 3;
    if (offsetof(Ath10kNativeBSSRecord, capabilities) != 560u) return 8;
    if (offsetof(Ath10kNativeBSSRecord, rates) != 565u) return 9;

    if (!ath10kNativeGenerationIsNewer(1u,0u) ||
        ath10kNativeGenerationIsNewer(1u,1u) ||
        !ath10kNativeGenerationIsNewer(1u,0xffffffffu) ||
        ath10kNativeGenerationIsNewer(0x80000000u,0u)) return 21;

    Ath10kNativeBSSRecord bss = {0};
    bss.bssid[0]=0x02;bss.bssid[5]=0x01;
    bss.ssid_length=3;bss.ssid[0]='a';bss.ssid[1]='p';bss.ssid[2]='1';
    bss.capabilities=1;bss.beacon_interval=100;bss.rate_count=1;
    bss.flags=ATH10K_NATIVE_BSS_RATES_VALID;
    Ath10kNativeJoinRequest join = {0};
    join.header.version = ATH10K_NATIVE_ABI_VERSION;
    join.header.size = (uint16_t)sizeof(join);
    join.header.operation = ATH10K_NATIVE_OP_ASSOCIATE;
    join.header.generation = 1;
    join.header.payload_bytes = (uint32_t)(sizeof(join) - sizeof(join.header));
    join.scan_generation = 3;
    join.security_mode = ATH10K_NATIVE_SECURITY_OPEN;
    join.ssid_length = 3;
    join.bssid[0] = 0x02; join.bssid[5] = 0x01;
    join.ssid[0] = 'a'; join.ssid[1] = 'p'; join.ssid[2] = '1';
    if (!ath10kNativeValidateJoinRequest(&join)) return 19;
    if (!ath10kNativeOpenJoinMatchesBSS(&join,&bss)) return 22;
    bss.flags |= ATH10K_NATIVE_BSS_PRIVACY;
    if (ath10kNativeOpenJoinMatchesBSS(&join,&bss)) return 23;
    bss.flags = ATH10K_NATIVE_BSS_RATES_VALID;
    bss.capabilities = 2;
    if (ath10kNativeOpenJoinMatchesBSS(&join,&bss)) return 24;
    join.security_mode = ATH10K_NATIVE_SECURITY_WPA2_PSK;
    if (ath10kNativeOpenJoinMatchesBSS(&join,&bss)) return 25;
    bss.capabilities=1;bss.rate_count=33;
    if (ath10kNativeOpenJoinMatchesBSS(&join,&bss)) return 26;
    bss.rate_count=1;bss.ssid_length=33;
    if (ath10kNativeOpenJoinMatchesBSS(&join,&bss)) return 27;
    join.security_mode = ATH10K_NATIVE_SECURITY_OPEN;
    if (ath10kNativeValidateJoinRequest(&join)) return 20;

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
