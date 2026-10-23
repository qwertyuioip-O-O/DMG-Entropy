#include "dmg_mid.hpp"
#include "dmg_v8.hpp"
#include "rmc.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
using Games=std::vector<std::vector<rmc::Move>>;
static void require(bool ok,const char*m){if(!ok)throw std::runtime_error(m);}
static void check(const Games& games){
  for(unsigned p=0;p<rmc::MID_PROFILE_COUNT;++p){
    auto mode=rmc::MidProfile(p); auto archive=rmc::mid_encode(games,mode);
    require(rmc::mid_decode(archive.bytes)==games,"DMT1 roundtrip");
    require(rmc::mid_encode(games,mode).bytes==archive.bytes,"deterministic output");
    auto corrupted=archive.bytes;corrupted.back()^=0x40;
    bool rejected=false;try{(void)rmc::mid_decode(corrupted);}catch(...){rejected=true;}
    require(rejected,"CRC corrupt not rejected");
  }
}
int main(){
  try{
    check({});check({{}, {}});
    Games games;std::vector<std::filesystem::path> paths;
    for(auto& e:std::filesystem::directory_iterator(std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"heldout"))
      if(e.path().extension()==".uci")paths.push_back(e.path());
    std::sort(paths.begin(),paths.end());
    for(auto&f:paths){std::ifstream input(f);std::string text{std::istreambuf_iterator<char>(input),{}};games.push_back(rmc::parse_moves(text));}
    check(games);
    auto golden_path=std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"golden"/"human23-mid-all.dmt";
    std::ifstream golden(golden_path,std::ios::binary);
    require(bool(golden),"missing DMT1 golden stream");
    std::vector<uint8_t> gold{std::istreambuf_iterator<char>(golden),{}};
    require(rmc::mid_encode(games,rmc::MidProfile::AllTacticsGuard).bytes==gold,"DMT1 golden mismatch");
    require(rmc::mid_decode(gold)==games,"DMT1 golden decode");
    auto r2_path=std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"golden"/"human23-r2-best.dmt";
    std::ifstream r2_file(r2_path,std::ios::binary);
    require(bool(r2_file),"missing DMT2 golden stream");
    std::vector<uint8_t> r2_gold{std::istreambuf_iterator<char>(r2_file),{}};
    require(r2_gold.size()>=6 && r2_gold[3]=='2' && r2_gold[5]==32,"wrong DMT2 header");
    require(rmc::mid_encode(games,rmc::MidProfile::OpeningMidSEEValueRate4).bytes==r2_gold,"DMT2 golden mismatch");
    require(rmc::mid_decode(r2_gold)==games,"DMT2 golden decode");
    // DMT3 keeps all DMT1/DMT2 legacy stream decoding and has a distinct magic.
    auto r3_path=std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"golden"/"human23-r3-best.dmt";
    std::ifstream r3_file(r3_path,std::ios::binary);
    require(bool(r3_file),"missing DMT3 golden stream");
    std::vector<uint8_t> r3_gold{std::istreambuf_iterator<char>(r3_file),{}};
    require(r3_gold.size()>=6 && r3_gold[3]=='3' && r3_gold[5]==42,"wrong DMT3 header");
    require(rmc::mid_encode(games,rmc::MidProfile::R3BothStrongRate4).bytes==r3_gold,"DMT3 golden mismatch");
    require(rmc::mid_decode(r3_gold)==games,"DMT3 golden decode");
    // DMT4 is a new model family: frozen DMT3 remains byte-for-byte identical.
    auto r4_path=std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"golden"/"human23-r4-context-rank.dmt";
    std::ifstream r4_file(r4_path,std::ios::binary);
    require(bool(r4_file),"missing DMT4 golden stream");
    std::vector<uint8_t> r4_gold{std::istreambuf_iterator<char>(r4_file),{}};
    require(r4_gold.size()>=6 && r4_gold[3]=='4' && r4_gold[5]==49,"wrong DMT4 header");
    require(rmc::mid_encode(games,rmc::MidProfile::R4ContextRank).bytes==r4_gold,"DMT4 golden mismatch");
    require(rmc::mid_decode(r4_gold)==games,"DMT4 golden decode");
    auto r4_control=rmc::mid_encode(games,rmc::MidProfile::R4Control);
    require(r4_control.payload_bits==rmc::mid_encode(games,rmc::MidProfile::R3BothStrongRate4).payload_bits,
            "DMT4 control does not reproduce DMT3 bit count");
    // Excluding changed magic/profile and CRC, DMT4 control must match DMT3 bytes.
    require(r4_control.bytes.size()==r3_gold.size(),"DMT4 control length differs");
    require(std::equal(r4_control.bytes.begin()+6,r4_control.bytes.end()-4,r3_gold.begin()+6),
            "DMT4 control DMT3 payload mismatch");
    for(unsigned id=0;id<45;++id){
        auto malformed=r4_gold;malformed[5]=uint8_t(id);
        bool rejected=false;try{(void)rmc::mid_decode(malformed);}catch(...){rejected=true;}
        require(rejected,"DMT4 accepted older profile in new version");
    }
    auto r3_control=rmc::mid_encode(games,rmc::MidProfile::R3Control);
    auto r2_control=rmc::mid_encode(games,rmc::MidProfile::OpeningMidSEEValueRate4);
    require(r3_control.payload_bits==r2_control.payload_bits,"DMT3 control does not preserve DMT2 probabilities");
    require(r3_control.bytes.size()==r2_control.bytes.size(),"DMT3 control byte length differs");
    require(std::equal(r3_control.bytes.begin()+6,r3_control.bytes.end()-4,r2_control.bytes.begin()+6),"DMT3 control payload differs");
    for(unsigned id=0;id<rmc::MID_PROFILE_COUNT;++id){
        auto data=r3_gold;data[5]=uint8_t(id);
        if(id<33){bool rejected=false;try{(void)rmc::mid_decode(data);}catch(...){rejected=true;}
            require(rejected,"DMT3 mis-versioned profile accepted");}
    }
    // DMT5 quiet-position research. Old streams must remain byte-for-byte frozen.
    auto r5_path=std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"golden"/"human23-r5-quiet-fast.dmt";
    std::ifstream r5_file(r5_path,std::ios::binary);
    require(bool(r5_file),"missing DMT5 golden stream");
    std::vector<uint8_t> r5_gold{std::istreambuf_iterator<char>(r5_file),{}};
    require(r5_gold.size()>=6 && r5_gold[3]=='5' && r5_gold[5]==61,"wrong DMT5 header");
    require(rmc::mid_encode(games,rmc::MidProfile::R5AllQuietFast).bytes==r5_gold,"DMT5 golden mismatch");
    require(rmc::mid_decode(r5_gold)==games,"DMT5 golden decode");
    auto r5_control=rmc::mid_encode(games,rmc::MidProfile::R5Control);
    require(r5_control.payload_bits==rmc::mid_encode(games,rmc::MidProfile::R4ContextRank).payload_bits,
            "DMT5 control does not preserve DMT4 coded bits");
    require(r5_control.bytes.size()==r4_gold.size(),"DMT5 control length differs");
    require(std::equal(r5_control.bytes.begin()+6,r5_control.bytes.end()-4,r4_gold.begin()+6),
            "DMT5 control payload differs");
    for(unsigned id=0;id<54;++id){
        auto malformed=r5_gold;malformed[5]=uint8_t(id);
        bool rejected=false;try{(void)rmc::mid_decode(malformed);}catch(...){rejected=true;}
        require(rejected,"DMT5 accepted old-format profile");
    }
    // DMT6 keeps all DMT1..5 frozen and introduces queen/pawn positional profiles.
    auto r6_path=std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"golden"/"human23-r6-best.dmt";
    std::ifstream r6_file(r6_path,std::ios::binary);
    require(bool(r6_file),"missing DMT6 golden stream");
    std::vector<uint8_t> r6_gold{std::istreambuf_iterator<char>(r6_file),{}};
    require(r6_gold.size()>=6 && r6_gold[3]=='6' && r6_gold[5]==75,"wrong DMT6 header");
    require(rmc::mid_encode(games,rmc::MidProfile::R6QueenPawnQuietRate4).bytes==r6_gold,
            "DMT6 golden mismatch");
    require(rmc::mid_decode(r6_gold)==games,"DMT6 golden decode");
    auto r6_control=rmc::mid_encode(games,rmc::MidProfile::R6Control);
    require(r6_control.payload_bits==rmc::mid_encode(games,rmc::MidProfile::R5AllQuietFast).payload_bits,
            "DMT6 control does not preserve DMT5 bits");
    require(r6_control.bytes.size()==r5_gold.size(),"DMT6 control length differs");
    require(std::equal(r6_control.bytes.begin()+6,r6_control.bytes.end()-4,r5_gold.begin()+6),
            "DMT6 control DMT5 payload differs");
    for(unsigned id=0;id<68;++id){
        auto malformed=r6_gold;malformed[5]=uint8_t(id);
        bool rejected=false;try{(void)rmc::mid_decode(malformed);}catch(...){rejected=true;}
        require(rejected,"DMT6 accepted an older profile ID");
    }
    // DMT7: frozen DMT6 profile 75 is embedded verbatim as control 80.
    auto r7_control=rmc::mid_encode(games,rmc::MidProfile::R7Control);
    require(r7_control.bytes.size()==r6_gold.size(),"DMT7 control length differs");
    require(r7_control.payload_bits==rmc::mid_encode(games,rmc::MidProfile::R6QueenPawnQuietRate4).payload_bits,
            "DMT7 control payload bit count differs");
    require(std::equal(r7_control.bytes.begin()+6,r7_control.bytes.end()-4,r6_gold.begin()+6),
            "DMT7 control payload is not byte-identical to DMT6");
    auto r7_path=std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"golden"/"human23-r7-intent-quiet.dmt";
    std::ifstream r7_file(r7_path,std::ios::binary);
    require(bool(r7_file),"missing DMT7 golden stream");
    std::vector<uint8_t> r7_gold{std::istreambuf_iterator<char>(r7_file),{}};
    require(r7_gold.size()>=6 && r7_gold[3]=='7' && r7_gold[5]==82,"wrong DMT7 header");
    require(rmc::mid_encode(games,rmc::MidProfile::R7IntentQuiet).bytes==r7_gold,"DMT7 golden mismatch");
    require(rmc::mid_decode(r7_gold)==games,"DMT7 golden decode");
    for(unsigned id=0;id<80;++id){
        auto bad=r7_gold;bad[5]=uint8_t(id);
        bool rejected=false;try{(void)rmc::mid_decode(bad);}catch(...){rejected=true;}
        require(rejected,"DMT7 accepted older profile ID");
    }
    // DMT8: the expert ensemble is a new wire format; old DMT1..7 remain frozen.
    auto r8_control=rmc::mid_encode(games,rmc::MidProfile::R8Control);
    auto r8_special_control=rmc::mid_encode(games,rmc::MidProfile::R8SpecialControl);
    require(r8_control.bytes[3]=='8' && r8_control.bytes[5]==98,"wrong DMT8 header");
    require(r8_control.payload_bits==rmc::mid_encode(games,rmc::MidProfile::R6QueenPawnQuietRate4).payload_bits,
            "DMT8 pipeline control bit count differs");
    require(std::equal(r8_control.bytes.begin()+6,r8_control.bytes.end()-4,r6_gold.begin()+6),
            "DMT8 pipeline control payload differs");
    require(std::equal(r8_special_control.bytes.begin()+6,r8_special_control.bytes.end()-4,r6_gold.begin()+6),
            "DMT8 specialist control payload differs");
    const auto r8_path=std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"golden"/"human23-r8-fused-uniform.dmt";
    std::ifstream r8_file(r8_path,std::ios::binary);
    require(bool(r8_file),"missing DMT8 golden stream");
    std::vector<uint8_t> r8_gold{std::istreambuf_iterator<char>(r8_file),{}};
    require(r8_gold.size()>=6 && r8_gold[3]=='8' && r8_gold[5]==119,"wrong DMT8 golden header");
    require(rmc::mid_encode(games,rmc::MidProfile::R8FusedUniform).bytes==r8_gold,
            "DMT8 golden mismatch");
    require(rmc::mid_decode(r8_gold)==games,"DMT8 golden decode");
    // DMT9 experimental extensions are version-isolated from all DMT1-DMT8
    // frozen streams. Their own selected sample is also byte-for-byte frozen.
    const auto r9_path=std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"golden"/"human23-r9-mix-sharp.dmt";
    std::ifstream r9_file(r9_path,std::ios::binary);
    require(bool(r9_file),"missing DMT9 golden stream");
    std::vector<uint8_t> r9_gold{std::istreambuf_iterator<char>(r9_file),{}};
    require(r9_gold.size()>=6 && r9_gold[3]=='9' && r9_gold[5]==182,"wrong DMT9 header");
    require(rmc::mid_encode(games,rmc::MidProfile::R9MixSharp).bytes==r9_gold,
            "DMT9 golden mismatch");
    require(rmc::mid_decode(r9_gold)==games,"DMT9 golden decode");
    auto incompatible_r8=r8_gold;incompatible_r8[5]=182;
    bool rejected_r9_id=false;
    try{(void)rmc::mid_decode(incompatible_r8);}catch(...){rejected_r9_id=true;}
    require(rejected_r9_id,"DMT8 accepted DMT9 profile id");
    auto incompatible_r9=r9_gold;incompatible_r9[5]=119;
    bool rejected_r8_id=false;
    try{(void)rmc::mid_decode(incompatible_r9);}catch(...){rejected_r8_id=true;}
    require(rejected_r8_id,"DMT9 accepted old DMT8 profile id");
    for(unsigned id=0;id<98;++id){
        auto malformed=r8_gold;malformed[5]=uint8_t(id);
        bool rejected=false;try{(void)rmc::mid_decode(malformed);}catch(...){rejected=true;}
        require(rejected,"DMT8 accepted an older profile ID");
    }
    // An actual transposition: the first four plies reach identical boards but
    // raw en-passant metadata differs and must be normalized when irrelevant.
    auto line_a=rmc::parse_moves("e2e4 e7e5 g1f3 b8c6 f1c4 g8f6");
    auto line_b=rmc::parse_moves("g1f3 b8c6 e2e4 e7e5 f1c4 g8f6");
    rmc::Position pos_a=rmc::Position::standard(),pos_b=rmc::Position::standard();
    for(int i=0;i<4;++i){require(pos_a.apply_checked(line_a[i]),"transposition A illegal");
                          require(pos_b.apply_checked(line_b[i]),"transposition B illegal");}
    require(pos_a.board==pos_b.board && pos_a.castling==pos_b.castling && pos_a.side==pos_b.side,
            "transposition final board mismatch");
    Games transpositions{line_a,line_b,line_a,line_b};
    auto tr=rmc::mid_encode(transpositions,rmc::MidProfile::R3BothStrongRate4);
    require(rmc::mid_decode(tr.bytes)==transpositions,"transposition archive roundtrip");
    auto bad_r3=r3_gold;bad_r3[5]=255;bool rejected_r3=false;
    try{(void)rmc::mid_decode(bad_r3);}catch(...){rejected_r3=true;}
    require(rejected_r3,"bad DMT3 profile accepted");
    auto bad_profile=r2_gold;bad_profile[5]=255;
    bool rejected_profile=false;try{(void)rmc::mid_decode(bad_profile);}catch(...){rejected_profile=true;}
    require(rejected_profile,"invalid DMT2 profile accepted");

    auto original=rmc::v8_encode(games,rmc::V8Model::RelationalJoint);
    auto baseline=rmc::mid_encode(games,rmc::MidProfile::Baseline);
    require(original.payload_bits==baseline.payload_bits,"base DMG8 bit count differs");
    require(rmc::v8_decode(original.bytes)==games,"frozen DMG8 decode");
    std::mt19937_64 rng(20261020);Games random;
    for(int i=0;i<8;++i){rmc::Position pos=rmc::Position::standard();std::vector<rmc::Move> seq;
      for(int j=0;j<85;++j){auto legal=pos.legal_moves();if(legal.empty())break;
        auto m=legal[rng()%legal.size()];require(pos.apply_checked(m),"random legal");seq.push_back(m);}
      random.push_back(seq);}
    check(random);
    std::cout<<"DMT1-DMT9: "<<rmc::MID_PROFILE_COUNT<<" profiles, human23, random8, transposition, CRC, golden, legacy PASS\n";
  }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
