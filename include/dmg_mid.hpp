#pragma once
#include "dmg_v6.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace rmc {
enum class MidProfile : uint8_t {
    Baseline=0, FlatMild=1, FlatStrong=2, Sharp=3, Guard5=4,
    Guard12=5, TemperatureMixture=6, TemperatureConditional=7,
    SEE=8, NewThreat=9, OwnHistory=10, ForcedResponse=11,
    AdaptiveResidual=12, AllTactics=13, AllTacticsGuard=14, SEEProxy=15, AllFast=16,
    AllGuard2=17, AllGuard10=18, AllGuardAdaptive=19,
    AllGuardRate1=20, AllGuardRate3=21, AllGuardRate4=22, AllGuardRate6=23,
    OpeningOnly=24, OpeningGuard=25, OpeningMid=26, OpeningMidGuard=27,
    OpeningMidGuardRate3=28, OpeningMidGuardRate4=29,
    MidSEEValue=30, OpeningMidSEEValue=31, OpeningMidSEEValueRate4=32,
    R3Control=33, R3OpeningPosition=34, R3MiddlegamePlan=35,
    R3Both=36, R3OpeningPositionWeak=37, R3PlanRate4=38, R3BothRate4=39,
    R3OpeningStrong=40, R3BothStrong=41, R3BothStrongRate4=42,
    R3OpeningAggressive=43, R3BothAggressive=44,
    R4Control=45, R4Context=46, R4Rank=47, R4Guard=48,
    R4ContextRank=49, R4ContextGuard=50, R4RankGuard=51, R4All=52,
    R4AllMild=53,
    R5Control=54,R5Pawn=55,R5Minor=56,R5Rook=57,
    R5All=58,R5AllQuiet=59,R5AllQuietSlow=60,R5AllQuietFast=61,
    R5AllQuietGuard=62,R5RookQuiet=63,R5RookPiece=64,
    R5RookQuietFast=65,R5AllQuietNoPawn=66,R5AllQuietRookPawn=67,
    R6Control=68,R6Queen=69,R6Pawn=70,R6QueenPawn=71,
    R6QueenPawnQuiet=72,R6QueenQuiet=73,R6PawnQuiet=74,
    R6QueenPawnQuietRate4=75,R6QueenSafety=76,R6PawnPressure=77,
    R6QueenPiece=78,R6PawnPiece=79,
    R7Control=80,R7Intent=81,R7IntentQuiet=82,R7IntentStrong=83,
    R7Context=84,R7ContextQuiet=85,R7Mix50=86,R7Mix75=87,
    R7Adaptive=88,R7AdaptiveContext=89,R7Guard=90,R7ContextStrong=91,
    R7CalmGuard2=92,R7CalmGuard5=93,R7TacticalGuard2=94,
    R7TacticalGuard5=95,R7TailCalibrate=96,R7QuietChoiceGuard=97,
    R8Control=98,R8Uniform=99,R8Global=100,R8Context=101,
    R8ContextFast=102,R8ContextSlow=103,R8ContextTactical=104,
    R8ContextHeavyFinal=105,R8ContextTwo=106,
    R8SwitchGlobal=107,R8SwitchContext=108,R8SwitchConservative=109,
    R8SwitchTwo=110,R8SwitchQuick=111,
    R8SpecialControl=112,R8SpecialUniform=113,R8SpecialGlobal=114,
    R8SpecialContext=115,R8SpecialPiece=116,R8SpecialTactical=117,
    R8SpecialPlan=118,
    R8FusedUniform=119,R8FusedGlobal=120,R8FusedContext=121,
    R8FusedTacticalPiece=122,R8FusedTacticalPlan=123,R8FusedPiecePlan=124,
    R9Rate1=125,R9Rate3=126,R9Rate4=127,R9Rate6=128,
    R9Base20=129,R9Base40=130,R9Tactical50=131,R9Piece50=132,
    R9Plan50=133,R9TacticalPiece=134,R9BasePiece=135,
    R9Rate3Base20=136,R9Rate1Base20=137,R9Rate4Base20=138,
    R9Rate3Piece50=139,R9Sharp=140,R9Damp=141,
    R9TPRate1=142,R9TPRate3=143,R9TPRate4=144,R9TPSharp=145,R9TPDamp=146,
    R9TPSelf2=147,R9TPSelf3=148,R9TPSelf4=149,
    R9TPSelfBlend3=150,R9TPBetterTactics=151,R9TPFeatureMix=152,
    R9TPBetterTacticsRate3=153,R9TPSelfBetterTactics=154,
    R9TPAlternativePiece=155,R9TPAllNewFeatures=156,
    R9PhaseOpen1=157,R9PhaseOpen2=158,R9PhaseOpen4=159,
    R9PhaseEnd1=160,R9PhaseEnd2=161,R9PhaseEnd4=162,
    R9PhaseBoth1=163,R9PhaseBoth2=164,R9PhaseBoth4=165,
    R9PhaseOpenMix=166,R9PhaseBothMix=167,
    R9BothRate6=168,R9BothRate8=169,R9Open8End4=170,R9Open4End8=171,
    R9BothAlt2=172,R9BothAlt4=173,R9BothAlt6=174,
    R9BothAlt8=175,R9BothSelf4=176,R9BothSelf6=177,
    R9MixRate1=178,R9MixRate3=179,R9MixRate4=180,R9MixRate6=181,
    R9MixSharp=182,R9MixDamp=183,R9MixTact65=184,R9MixPiece65=185,
    R9MixTact35=186,R9MixSelf3=187,R9MixSelf4=188,
    R10Geom2Mix=189,R10Geom4Mix=190,R10Geom2Full=191,R10Geom4Full=192,
    R10Tact2Mix=193,R10Tact4Mix=194,R10Tact2Full=195,R10Tact4Full=196,
    R10Hybrid2Mix=197,R10Hybrid4Mix=198,R10Hybrid2Full=199,R10Hybrid4Full=200,
    R10Tact1Full=201,R10Tact3Full=202,R10Tact1Mix75=203,R10Tact3Mix75=204,
    R10Tact2Mix25=205,R10Tact4Mix25=206,R10Tact2Mix75=207,R10Tact4Mix75=208,
    R10GateTact2=209,R10GateTact4=210,R10GateTact2Mild=211,R10GateTact4Mild=212,
    R10GateGeom2=213,R10GateGeom4=214,R10GateGeom2Mild=215,R10GateGeom4Mild=216,
    R10OpenGeom2=217,R10EndGeom2=218,R10PhaseGeom2=219,R10PhaseGeom4=220,
    R10PhaseTact2=221,R10PhaseTact4=222,R10PhaseHybrid2=223,R10PhaseHybrid4=224,
    R10PhaseTact2Full=225,R10PhaseTact4Full=226,R10PhaseGeom2Full=227,R10PhaseGeom4Full=228,
    R10AllTact2=229,R10AllGeom2=230,
    R10ComboTact2Mix=231,R10ComboTact4Mix=232,
    R10ComboTact2Full=233,R10ComboTact4Full=234,
    R10ComboTact2Gate=235,R10ComboTact4Gate=236,
    R10ComboTact1Mix75=237,R10ComboTact2Mix25=238,
    R10ComboTact2Mix75=239,R10ComboGeom2Mix=240,
    R10ComboHybrid2Mix=241,R10ComboTact3Full=242, R11SingleRate2=243,R11SingleRate4=244,R11Joint30Rate2=245,R11Joint30Rate4=246,R11Joint40Rate2=247,R11Joint40Rate4=248,R12Configurable=249,R12JointAll=250,R13Structure=251,R14Structure=252,R15PawnSplit=253,R16SourceModel=254
};
constexpr unsigned MID_PROFILE_COUNT=255;
// Preferred research default as of 2026-10-22. Explicit archived profile IDs
// remain immutable. Decoder always reads the profile embedded in the header.
constexpr MidProfile MID_DEFAULT_PROFILE=MidProfile::R10ComboTact4Full;
struct MidStage {uint64_t moves=0, tails=0, top1=0; double logloss=0, tailbits=0, source_logloss=0; std::array<uint64_t,10> feature_selected{},feature_candidates{};};
struct MidDiagnostics {
    std::array<MidStage,3> phase{};
    std::array<double,5> r8_expert_loss{};
    std::array<std::array<double,5>,4> r8_context_loss{};
    std::array<uint64_t,4> r8_context_count{};
};
struct MidTrace {
    uint32_t game=0, ply=0, candidate_count=0;
    uint8_t stage=0, context=0, capture=0, check=0;
    uint16_t rank=0, selected_probability=0, selected_flags=0;
    double logloss=0;
    Move selected{};
};
// DMT12/C header serializes every model parameter; no external state required.
struct Dmt12Config {
    uint8_t families=31,rate=2,alpha=50,phase_mask=2,base=0,decay_log2=7,cap=24;
    bool operator==(const Dmt12Config&) const = default;
};
V6Archive mid_encode_experimental(const std::vector<std::vector<Move>>&,const Dmt12Config&,MidDiagnostics* =nullptr);
struct Dmt14Config {
    uint8_t structure=95,features=7,rate=1;
    bool operator==(const Dmt14Config&) const = default;
};
struct Dmt15Config {
    uint8_t mode=7,rate=1,capture_rate=4;
    bool operator==(const Dmt15Config&) const = default;
};
struct Dmt16Config {
    uint8_t layout=0,rate=2,cap=128;
    bool operator==(const Dmt16Config&) const = default;
};
V6Archive mid_encode_source_model(const std::vector<std::vector<Move>>&,const Dmt16Config&,MidDiagnostics* =nullptr,std::vector<MidTrace>* =nullptr);
V6Archive mid_encode_pawn_split(const std::vector<std::vector<Move>>&,const Dmt15Config&,MidDiagnostics* =nullptr,std::vector<MidTrace>* =nullptr);
V6Archive mid_encode_structural(const std::vector<std::vector<Move>>&,const Dmt14Config&,MidDiagnostics* =nullptr,std::vector<MidTrace>* =nullptr);
V6Archive mid_encode_structure(const std::vector<std::vector<Move>>&,uint8_t,MidDiagnostics* =nullptr,std::vector<MidTrace>* =nullptr);
const char* mid_name(MidProfile);
V6Archive mid_encode(const std::vector<std::vector<Move>>&,MidProfile = MID_DEFAULT_PROFILE,MidDiagnostics* =nullptr, std::vector<MidTrace>* =nullptr);
std::vector<std::vector<Move>> mid_decode(const std::vector<uint8_t>&);
}
