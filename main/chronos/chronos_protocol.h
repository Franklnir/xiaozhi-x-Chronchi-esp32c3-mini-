#ifndef XIAOZHI_CHRONOS_PROTOCOL_H_
#define XIAOZHI_CHRONOS_PROTOCOL_H_

#include "chronos_state.h"

#include <array>
#include <cstddef>
#include <cstdint>

class ChronosProtocol {
public:
    static constexpr size_t kMaximumFrame = 512;

    explicit ChronosProtocol(ChronosState& state) : state_(state) {}
    bool FeedChunk(const uint8_t* data, size_t length);
    void Reset();

private:
    ChronosState& state_;
    std::array<uint8_t, kMaximumFrame> frame_ = {};
    size_t expected_ = 0;
    size_t received_ = 0;
};

#endif  // XIAOZHI_CHRONOS_PROTOCOL_H_
