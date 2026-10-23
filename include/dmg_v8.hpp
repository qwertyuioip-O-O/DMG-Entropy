#pragma once
#include "dmg_v7.hpp"
namespace rmc {
// DMG8 is a distinct wire format: v0.7 CDFs remain unchanged.
enum class V8Model : uint8_t {
    LegalOpportunity=0,
    RelationalAE=1,
    RelationalJoint=2,
    ExpertMixture=3,
    ExpertMixtureReset=4,
    Auto=5
};
V6Archive v8_encode(const std::vector<std::vector<Move>>& games, V8Model model);
std::vector<std::vector<Move>> v8_decode(const std::vector<uint8_t>& bytes);
const char* v8_name(V8Model model);
}
