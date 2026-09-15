#ifndef XIAOZHI_MODE_STORE_H_
#define XIAOZHI_MODE_STORE_H_

#include "boot_mode.h"

#include <esp_err.h>

class ModeStore {
public:
    static BootMode Load();
    static esp_err_t Save(BootMode mode);
    static BootMode Current();
private:
    static BootMode current_mode_;
};

#endif  // XIAOZHI_MODE_STORE_H_
