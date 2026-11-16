#include "../src/shared/Ath10kNativeBridge.h"

static_assert(ATH10K_NATIVE_ABI_VERSION == 1u, "ABI version");
static_assert(ATH10K_NATIVE_MAX_SCAN_SSIDS == 16u, "bounded SSID filters");
static_assert(ATH10K_NATIVE_MAX_SCAN_CHANNELS == 64u, "bounded channel list");
static_assert(ATH10K_NATIVE_MAX_BSS_IE_BYTES == 512u, "bounded information elements");
static_assert(sizeof(Ath10kNativeBSSRecord) == 597u, "BSS snapshot layout");
static_assert(sizeof(Ath10kNativeJoinRequest) == 64u, "Join request layout");
static_assert(offsetof(Ath10kNativeJoinRequest, scan_generation) == 20u, "Join scan generation offset");
static_assert(offsetof(Ath10kNativeJoinRequest, bssid) == 26u, "Join BSSID offset");
static_assert(offsetof(Ath10kNativeJoinRequest, ssid) == 32u, "Join SSID offset");
static_assert(sizeof(Ath10kNativeScanStatus) == 16u, "scan status layout");
static_assert(sizeof(Ath10kNativeLinkSnapshot) == 88u, "link snapshot layout");
static_assert(sizeof(Ath10kNativeJoinSnapshot) == 32u, "join snapshot layout");
static_assert(offsetof(Ath10kNativeJoinSnapshot, station_beacon_serial) == 8u, "join beacon serial offset");
static_assert(sizeof(Ath10kNativeCapabilities) == 164u, "capabilities layout");
static_assert(ATH10K_NATIVE_LINK_IDLE == 0 && ATH10K_NATIVE_LINK_CONNECTED == 3, "link state values");
static_assert(offsetof(Ath10kNativeBSSRecord, capabilities) == 560u, "BSS capabilities offset");
static_assert(offsetof(Ath10kNativeBSSRecord, rates) == 565u, "BSS rates offset");

static void writeLE32(uint8_t *p, uint32_t value) {
    p[0]=static_cast<uint8_t>(value);
    p[1]=static_cast<uint8_t>(value>>8);
    p[2]=static_cast<uint8_t>(value>>16);
    p[3]=static_cast<uint8_t>(value>>24);
}

int main() {
    static uint8_t appleScan[ATH10K_NATIVE_APPLE_SCAN_MAX_BYTES]{};
    writeLE32(appleScan+0x1c,3);
    appleScan[0x20]='w';appleScan[0x21]='i';appleScan[0x22]='f';
    appleScan[0x14]=0x02;appleScan[0x15]=0x11;appleScan[0x16]=0x22;
    appleScan[0x17]=0x33;appleScan[0x18]=0x44;appleScan[0x19]=0x55;
    writeLE32(appleScan+0x40,1);
    writeLE32(appleScan+0x54,4);
    writeLE32(appleScan+0x58+4,36);
    writeLE32(appleScan+0x58+8,0x10);
    writeLE32(appleScan+0x64+4,6);
    writeLE32(appleScan+0x64+8,0x8);
    writeLE32(appleScan+0x70+4,149);
    writeLE32(appleScan+0x70+8,0x2000);
    writeLE32(appleScan+0x7c+4,36);
    writeLE32(appleScan+0x7c+8,0x10);
    Ath10kNativeScanRequest decoded{};
    if (!ath10kNativeDecodeAppleScanRequest(appleScan,sizeof(appleScan),7,&decoded)) return 10;
    if (decoded.header.generation!=7 || decoded.scan_type!=ATH10K_NATIVE_SCAN_ACTIVE) return 11;
    if (decoded.ssid_count!=1 || decoded.ssids[0].length!=3 || decoded.has_bssid!=1) return 12;
    if (decoded.channel_count!=2 || decoded.channels[0]!=36 || decoded.channels[1]!=6) return 13;
    writeLE32(appleScan+0x40,2);
    if (!ath10kNativeDecodeAppleScanRequest(appleScan,sizeof(appleScan),8,&decoded) ||
        decoded.scan_type!=ATH10K_NATIVE_SCAN_PASSIVE) return 14;
    writeLE32(appleScan+0x54,1);
    writeLE32(appleScan+0x58+4,149);
    writeLE32(appleScan+0x58+8,0x2000);
    if (ath10kNativeDecodeAppleScanRequest(appleScan,sizeof(appleScan),9,&decoded)) return 15;

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
    scan.header.size++;
    if (ath10kNativeValidateScanRequest(&scan)) return 4;
    scan.header.size--;
    scan.ssids[0].length = 33;
    if (ath10kNativeValidateScanRequest(&scan)) return 5;
    scan.ssids[0].length = 3;
    scan.active_dwell_ms = 20;
    if (ath10kNativeValidateScanRequest(&scan)) return 6;
    scan.active_dwell_ms = 200;
    if (!ath10kNativeValidateScanRequest(&scan)) return 7;

    if (!ath10kNativeGenerationIsNewer(1u,0u) ||
        !ath10kNativeGenerationIsNewer(2u,1u) ||
        ath10kNativeGenerationIsNewer(1u,1u) ||
        ath10kNativeGenerationIsNewer(0u,0xffffffffu) ||
        !ath10kNativeGenerationIsNewer(1u,0xffffffffu) ||
        ath10kNativeGenerationIsNewer(0x80000000u,0u)) return 21;

    Ath10kNativeJoinRequest join{};
    join.header.version=ATH10K_NATIVE_ABI_VERSION;
    join.header.size=static_cast<uint16_t>(sizeof(join));
    join.header.operation=ATH10K_NATIVE_OP_ASSOCIATE;
    join.header.generation=1;
    join.header.payload_bytes=sizeof(join)-sizeof(join.header);
    join.scan_generation=7;
    join.security_mode=ATH10K_NATIVE_SECURITY_OPEN;
    join.ssid_length=3;
    join.bssid[0]=0x02;join.bssid[5]=0x01;
    join.ssid[0]='a';join.ssid[1]='p';join.ssid[2]='1';
    if (!ath10kNativeValidateJoinRequest(&join)) return 16;
    join.security_mode=ATH10K_NATIVE_SECURITY_WPA2_PSK;
    if (ath10kNativeValidateJoinRequest(&join)) return 17;
    join.security_mode=ATH10K_NATIVE_SECURITY_OPEN;
    join.bssid[0]=0x01;
    if (ath10kNativeValidateJoinRequest(&join)) return 18;
    join.bssid[0]=0x02;
    join.scan_generation=0;
    if (ath10kNativeValidateJoinRequest(&join)) return 19;
    join.scan_generation=7;
    join.ssid_length=33;
    if (ath10kNativeValidateJoinRequest(&join)) return 20;

    Ath10kNativeKeyRequest key{};
    key.header.version = ATH10K_NATIVE_ABI_VERSION;
    key.header.size = static_cast<uint16_t>(sizeof(key));
    key.header.operation = ATH10K_NATIVE_OP_SET_KEY;
    key.header.generation = 1;
    key.header.payload_bytes = sizeof(key) - sizeof(key.header);
    key.key_type = ATH10K_NATIVE_KEY_PAIRWISE;
    key.cipher = ATH10K_NATIVE_CIPHER_CCMP_128;
    key.key_length = 16;
    if (sizeof(key) != 70u || !ath10kNativeValidateKeyRequest(&key)) return 8;
    key.cipher = ATH10K_NATIVE_CIPHER_TKIP;
    key.key_length = 32;
    if (ath10kNativeValidateKeyRequest(&key)) return 9;
    return 0;
}
