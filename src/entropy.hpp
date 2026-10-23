#pragma once
#include "rmc.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rmc {
struct EntropyPayload { std::vector<uint8_t> bytes; uint32_t bit_count = 0; };
EntropyPayload entropy_encode(const std::vector<Move>& moves, Mode mode, Position start);
std::vector<Move> entropy_decode(const uint8_t* bytes, uint32_t bits, uint32_t count, Mode mode, Position start);
}
