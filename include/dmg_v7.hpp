#pragma once
#include "dmg_v6.hpp"
namespace rmc {
// New stream format: DMG7. v0.6 formats are not modified.
enum class V7Model : uint8_t {
    Baseline=0, ExactDictionary=1, GeometryPattern=2, SquarePattern=3,
    CombinedDictionary=4, ModelCompetition=5, SwitchingCompetition=6,
    OpportunityGeometry=7, OpportunitySquare=8, OpportunityCompetition=9, Auto=10, SelectiveCompetition=11
};
V6Archive v7_encode(const std::vector<std::vector<Move>>& games, V7Model model);
std::vector<std::vector<Move>> v7_decode(const std::vector<uint8_t>& bytes);
const char* v7_name(V7Model mode);
}
