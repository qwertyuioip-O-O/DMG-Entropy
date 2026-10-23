#include "dmg_phase.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
using Games=std::vector<std::vector<rmc::Move>>;
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static void test(const Games&games){
    for(auto p:{rmc::PhaseProfile::Shared,rmc::PhaseProfile::Residual1,rmc::PhaseProfile::Residual2,rmc::PhaseProfile::Residual4,rmc::PhaseProfile::Residual6,rmc::PhaseProfile::EndgameOnly,rmc::PhaseProfile::OpeningMixtureEndgame}){
        auto archive=rmc::phase_encode(games,p);
        check(rmc::phase_decode(archive.bytes)==games,"phase roundtrip");
        check(rmc::phase_encode(games,p).bytes==archive.bytes,"phase deterministic bytes");
        auto corrupt=archive.bytes;corrupt.back()^=1;
        bool reject=false;try{(void)rmc::phase_decode(corrupt);}catch(...){reject=true;}
        check(reject,"CRC rejection");
    }
}
int main(){
 try{
    test({});test({{}});
    Games human;
    std::vector<std::filesystem::path> files;
    for(auto&e:std::filesystem::directory_iterator(std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"heldout"))
        if(e.path().extension()==".uci")files.push_back(e.path());
    std::sort(files.begin(),files.end());
    for(auto&file:files){std::ifstream f(file);std::string s{std::istreambuf_iterator<char>(f),{}};
        human.push_back(rmc::parse_moves(s));}
    test(human);
    auto gold_path=std::filesystem::path(RMC_SOURCE_DIR)/"examples"/"golden"/"human23-opening-mixture-endgame.dmp";
    std::ifstream gold_file(gold_path,std::ios::binary);
    check(bool(gold_file),"phase golden file missing");
    std::vector<uint8_t> gold_bytes{std::istreambuf_iterator<char>(gold_file),{}};
    auto hybrid=rmc::phase_encode(human,rmc::PhaseProfile::OpeningMixtureEndgame);
    check(hybrid.bytes==gold_bytes,"phase golden bytes changed");
    auto dm8=rmc::v8_encode(human,rmc::V8Model::RelationalJoint);
    auto dm1=rmc::phase_encode(human,rmc::PhaseProfile::Shared);
    check(dm8.payload_bits==dm1.payload_bits,"phase shared not DMG8 payload compatible");
    check(dm8.bytes.size()==dm1.bytes.size(),"shared framing size");
    auto baseline=rmc::phase_baseline_diagnostics(human);
    rmc::PhaseDiagnostics measured{};auto again=rmc::phase_encode(human,rmc::PhaseProfile::Shared,&measured);
    check(again.payload_bits==dm8.payload_bits,"shared payload mismatch");
    for(unsigned i=0;i<3;++i){
        check(baseline.stage[i].plies==measured.stage[i].plies,"diagnostic count mismatch");
        check(baseline.stage[i].ideal_bits==measured.stage[i].ideal_bits,"diagnostic probability mismatch");
    }
    std::mt19937_64 rng(20261020);Games random;
    for(int k=0;k<8;++k){rmc::Position p=rmc::Position::standard();std::vector<rmc::Move> g;
      for(int j=0;j<65;++j){auto legal=p.legal_moves();if(legal.empty())break;
        auto m=legal[rng()%legal.size()];check(p.apply_checked(m),"illegal random move");g.push_back(m);}
      random.push_back(std::move(g));}
    test(random);
    check(rmc::v8_decode(dm8.bytes)==human,"DMG8 legacy decode");
    std::cout<<"phase tests PASS "<<human.size()<<" games\n";
 }catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
