#include "chronos_protocol.h"

#include <algorithm>
#include <cstring>

void ChronosProtocol::Reset() {
    expected_ = 0;
    received_ = 0;
}

bool ChronosProtocol::FeedChunk(const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0) return false;

    const bool start = length >= 4 && (data[0] == 0xab || data[0] == 0xea) &&
                       (data[3] == 0xfe || data[3] == 0xff);
    if (start) {
        const size_t declared = (static_cast<size_t>(data[1]) << 8) | data[2];
        expected_ = declared + 3;
        received_ = 0;
        if (expected_ < 5 || expected_ > frame_.size() || length > expected_) {
            Reset();
            return false;
        }
        std::memcpy(frame_.data(), data, length);
        received_ = length;
    } else {
        if (expected_ == 0 || received_ < 20 || length < 2) {
            Reset();
            return false;
        }
        const size_t offset = 20 + static_cast<size_t>(data[0]) * 19;
        if (offset != received_ || length - 1 > expected_ - received_) {
            Reset();
            return false;
        }
        std::memcpy(frame_.data() + received_, data + 1, length - 1);
        received_ += length - 1;
    }

    if (received_ == expected_) {
        const bool parsed = state_.ApplyFrame(frame_.data(), expected_);
        Reset();
        return parsed;
    }
    return true;
}
