#include "../src/kext/Ath10kNativeScanCursor.hpp"

int main() {
    Ath10kNativeScanCursor cursor;
    uint32_t index = 0xfeedu;

    if (cursor.begin(0, ATH10K_NATIVE_SCAN_SUCCEEDED, 2)) return 1;
    if (cursor.begin(7, ATH10K_NATIVE_SCAN_PENDING, 2)) return 2; // pending is not iterable
    if (cursor.next(7, &index) != Ath10kNativeScanCursor::kStaleGeneration) return 3;

    if (!cursor.begin(7, ATH10K_NATIVE_SCAN_SUCCEEDED, 2)) return 4;
    if (cursor.generation() != 7 || cursor.count() != 2 || cursor.position() != 0) return 5;
    if (cursor.next(8, &index) != Ath10kNativeScanCursor::kStaleGeneration) return 6;
    if (cursor.next(7, nullptr) != Ath10kNativeScanCursor::kNotReady) return 7;
    if (cursor.next(7, &index) != Ath10kNativeScanCursor::kRecord || index != 0) return 8;
    if (cursor.next(7, &index) != Ath10kNativeScanCursor::kRecord || index != 1) return 9;
    if (cursor.next(7, &index) != Ath10kNativeScanCursor::kEnd) return 10;
    if (cursor.next(7, &index) != Ath10kNativeScanCursor::kEnd) return 11;

    if (!cursor.begin(9, ATH10K_NATIVE_SCAN_SUCCEEDED, 0)) return 12;
    if (cursor.next(9, &index) != Ath10kNativeScanCursor::kEnd) return 13;
    cursor.reset();
    if (cursor.active() || cursor.generation() != 0) return 14;
    if (cursor.next(9, &index) != Ath10kNativeScanCursor::kStaleGeneration) return 15;
    return 0;
}
