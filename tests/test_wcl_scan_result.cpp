#include "../src/kext/Ath10kWCLScanResult.hpp"

int main() {
    Ath10kNativeBSSRecord record{};
    record.channel = 36;
    record.ssid_length = 3;
    record.ssid[0] = 'a'; record.ssid[1] = 'b'; record.ssid[2] = 'c';
    record.bssid[0] = 0x02; record.bssid[5] = 0x55;
    record.rssi_dbm = -61;
    record.noise_dbm = 0;
    record.beacon_interval = 100;
    record.capabilities = 0x431;
    record.flags = ATH10K_NATIVE_BSS_RSSI_ESTIMATED;
    const uint8_t ies[] = {0, 3, 'f', 'o', 'o', 1, 1, 0x82};
    record.ie_length = sizeof(ies);
    memcpy(record.ies, ies, sizeof(ies));

    uint8_t output[64 + sizeof(ies)]{};
    if (ath10kBuildWCLScanResult(&record, output, sizeof(output)-1) != 0) return 1;
    if (ath10kBuildWCLScanResult(&record, output, sizeof(output)) != sizeof(output)) return 2;

    Ath10kWCLBeaconMetadata metadata{};
    memcpy(&metadata, output, sizeof(metadata));
    if (metadata.ieLength != sizeof(ies) || metadata.channel != 36 ||
        metadata.channelSpec != 0xd024 || metadata.ssidLength != 3 ||
        metadata.rssi != -61 || metadata.noise != 0 || metadata.interval != 100 ||
        metadata.capability != 0x431) return 3;
    if ((metadata.flags & (ATH10K_WCL_BEACON_SSID_PRESENT | ATH10K_WCL_BEACON_RSSI_VALID)) !=
        (ATH10K_WCL_BEACON_SSID_PRESENT | ATH10K_WCL_BEACON_RSSI_VALID)) return 4;
    if (memcmp(output + sizeof(metadata), ies, sizeof(ies)) != 0) return 5;

    record.ssid_length = 0;
    if (ath10kBuildWCLScanResult(&record, output, sizeof(output)) != sizeof(output)) return 6;
    memcpy(&metadata, output, sizeof(metadata));
    if (metadata.flags & ATH10K_WCL_BEACON_SSID_PRESENT) return 7;

    record.ie_length = 2;
    record.ies[0] = 48; record.ies[1] = 10;
    if (ath10kBuildWCLScanResult(&record, output, sizeof(output)) != 0) return 8;
    record.ie_length = 0;
    record.channel = 0;
    if (ath10kBuildWCLScanResult(&record, output, sizeof(output)) != 0) return 9;
    return 0;
}
