#pragma once
#include "dmg_v8.hpp"
#include <array>
#include <cstdint>
namespace rmc {
enum class PhaseProfile : uint8_t { Shared=0, Residual1=1, Residual2=2, Residual4=3, Residual6=4, EndgameOnly=5, OpeningMixtureEndgame=6 };
struct PhaseStats { uint64_t plies=0, long_tail_64=0, long_tail_256=0, top1=0; double ideal_bits=0, long_tail_bits=0; };
struct PhaseDiagnostics { std::array<PhaseStats,3> stage{}; };
const char* phase_name(PhaseProfile p);
V6Archive phase_encode(const std::vector<std::vector<Move>>& games,PhaseProfile profile,PhaseDiagnostics* diagnostics=nullptr);
std::vector<std::vector<Move>> phase_decode(const std::vector<uint8_t>& bytes);
PhaseDiagnostics phase_v8_diagnostics(const std::vector<std::vector<Move>>& games,V8Model mode);
PhaseDiagnostics phase_baseline_diagnostics(const std::vector<std::vector<Move>>& games);
}
