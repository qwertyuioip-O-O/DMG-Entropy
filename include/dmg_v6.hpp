#pragma once
#include "rmc.hpp"
#include <cstdint>
#include <vector>

namespace rmc {
enum class V6Model : uint8_t { Baseline=0, Hierarchical=1, Mixture=2, Dictionary=3, All=4 };
struct V6Archive {
  std::vector<uint8_t> bytes;
  uint64_t payload_bits=0;
  uint64_t framing_bytes=0;
};
V6Archive v6_encode(const std::vector<std::vector<Move>>& games, V6Model model);
std::vector<std::vector<Move>> v6_decode(const std::vector<uint8_t>& bytes);
const char* v6_name(V6Model mode);
}
