#include "../src/shared/Ath10kNativeBridge.h"

int main(void) {
    Ath10kNativeMessageHeader header = {0};
    header.version = ATH10K_NATIVE_ABI_VERSION;
    header.operation = ATH10K_NATIVE_OP_GET_LINK_SNAPSHOT;
    if (sizeof(header) != 20u) return 1;
    if (sizeof(Ath10kNativeLinkSnapshot) != 88u) return 2;
    if (sizeof(Ath10kNativeBSSRecord) != 560u) return 3;
    return 0;
}
