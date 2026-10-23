#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <random>
#include <stdexcept>
#include <algorithm>
#include <array>
using namespace rmc;
using Games=std::vector<std::vector<Move>>;
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
std::vector<uint8_t> bytes_of(const std::filesystem::path& path){
 std::ifstream in(path,std::ios::binary);require(bool(in),"missing golden input");
 return {std::istreambuf_iterator<char>(in),{}};
}
int main(){try{
 std::filesystem::path root=RMC_SOURCE_DIR;
 std::vector<std::filesystem::path> paths;
 for(const auto& f:std::filesystem::directory_iterator(root/"examples"/"heldout"))
   if(f.path().extension()==".uci")paths.push_back(f.path());
 std::sort(paths.begin(),paths.end());Games human;
 for(const auto& f:paths){auto data=bytes_of(f);human.push_back(parse_moves(std::string(data.begin(),data.end())));}
 std::mt19937_64 gen(20261021);Games random;
 for(int i=0;i<8;++i){Position p=Position::standard();std::vector<Move> moves;
  for(int ply=0;ply<85;++ply){auto legal=p.legal_moves();if(legal.empty())break;
   auto m=legal[gen()%legal.size()];require(p.apply_checked(m),"random move illegal");moves.push_back(m);}
  random.push_back(std::move(moves));}
 std::vector<unsigned> profiles={243,244,245,246,247,248};
 for(auto p:profiles){
  for(const auto& games:std::array<Games,3>{Games{},human,random}){
    auto arc=mid_encode(games,MidProfile(p));require(mid_decode(arc.bytes)==games,"roundtrip mismatch");
    require(mid_encode(games,MidProfile(p)).bytes==arc.bytes,"nondeterminism");
    auto bad=arc.bytes;bad.back()^=0x20;bool rejected=false;
    try{(void)mid_decode(bad);}catch(...){rejected=true;}
    require(rejected,"CRC accepted corruption");
    if(p>=243)require(arc.bytes[3]=='B'&&arc.bytes[5]==p,"DMT11 header mismatch");
  }
 }
 for(const auto [id,path]:std::array<std::pair<unsigned,const char*>,9>{
      std::pair<unsigned,const char*>{14,"human23-mid-all.dmt"},
      {32,"human23-r2-best.dmt"},
      {42,"human23-r3-best.dmt"},
      {49,"human23-r4-context-rank.dmt"},
      {61,"human23-r5-quiet-fast.dmt"},
      {75,"human23-r6-best.dmt"},
      {82,"human23-r7-intent-quiet.dmt"},
      {119,"human23-r8-fused-uniform.dmt"},
      {182,"human23-r9-mix-sharp.dmt"}}){
    auto golden=bytes_of(root/"examples"/"golden"/path);
    require(mid_encode(human,MidProfile(id)).bytes==golden,"golden bytes changed");
    require(mid_decode(golden)==human,"golden decode failed");
 }
 auto a=mid_encode(human,MidProfile(234)).bytes;
 auto wrong=a;wrong[3]='9';bool rejected=false;
 try{(void)mid_decode(wrong);}catch(...){rejected=true;}
 require(rejected,"DMT9 accepted DMT10 ID");
 std::cout<<"DMT11 candidates: "<<profiles.size()<<" profiles, human23, random8, empty, 9 golden, CRC, deterministic PASS\n";
 }catch(const std::exception& e){std::cerr<<"DMT10 smoke FAIL: "<<e.what()<<"\n";return 1;}}
