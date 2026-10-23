#pragma once
#include "dmg_v8.hpp"
namespace rmc {
// Experimental DMG9-R1: 16 signed relation-delta features, deterministic
// fixed-point online conditional-choice model. DMG8 streams are unchanged.
enum class V9Model : uint8_t { DeltaOpportunity=0, DeltaNoPattern=1, Auto=2 };
V6Archive v9_encode(const std::vector<std::vector<Move>>& games, V9Model model);
std::vector<std::vector<Move>> v9_decode(const std::vector<uint8_t>& bytes);
const char* v9_name(V9Model model);
}
