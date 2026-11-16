#include "../src/kext/Ath10kWMI.hpp"

static_assert(Ath10k::kWMIScanChannelCount > 0, "QCA9377 scan plan must not be empty");

void nativeScanBuilderCompileCheck() {
    UInt8 frame[1024]{};
    Ath10kNativeScanRequest request{};
    request.header.version=ATH10K_NATIVE_ABI_VERSION;
    request.header.size=static_cast<UInt16>(sizeof(request));
    request.header.operation=ATH10K_NATIVE_OP_SCAN_START;
    request.header.generation=1;
    request.header.payload_bytes=sizeof(request)-sizeof(request.header);
    request.scan_type=ATH10K_NATIVE_SCAN_PASSIVE;
    request.channel_count=2;
    request.channels[0]=1;
    request.channels[1]=36;

    Ath10k::RegulatoryPolicy policy{};
    (void)Ath10k::buildWMIScanRequest(frame,sizeof(frame),1016,1,0xa037,false,
                                      &policy,request);
    request.scan_type=ATH10K_NATIVE_SCAN_ACTIVE;
    request.ssid_count=1;
    request.ssids[0].length=4;
    request.ssids[0].bytes[0]='t';
    request.ssids[0].bytes[1]='e';
    request.ssids[0].bytes[2]='s';
    request.ssids[0].bytes[3]='t';
    (void)Ath10k::buildWMIScanRequest(frame,sizeof(frame),1016,1,0xa038,false,
                                      &policy,sourceMAC,request);
}
