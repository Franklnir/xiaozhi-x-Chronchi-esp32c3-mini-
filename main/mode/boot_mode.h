#ifndef XIAOZHI_BOOT_MODE_H_
#define XIAOZHI_BOOT_MODE_H_

#include <cstdint>

enum class BootMode : uint8_t {
    Xiaozhi = 0,
    Chronchi = 1,
};

inline const char* BootModeName(BootMode mode) {
    switch (mode) {
        case BootMode::Chronchi: return "Chronchi";
        default: return "Xiaozhi";
    }
}

#endif  // XIAOZHI_BOOT_MODE_H_
