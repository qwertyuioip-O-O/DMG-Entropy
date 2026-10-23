#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace rmc;
int main(int argc,char**argv){try{
 if(argc!=3)return 2;const int mask=std::stoi(argv[1]); auto encode=[&](const auto& games){return mask<0?mid_encode(games,MidProfile::R12JointAll):mid_encode_structure(games,uint8_t(mask));};
 std::ifstream in(argv[2]);if(!in)throw std::runtime_error("missing bundle");
 std::vector<std::vector<Move>> games;std::string line;uint64_t plies=0;
 while(std::getline(in,line))if(!line.empty()){auto g=parse_moves(line);plies+=g.size();games.push_back(g);}
 auto warm=encode(games);if(mid_decode(warm.bytes)!=games)throw std::runtime_error("warm roundtrip");
 std::vector<double> ew,dw,ec,dc;constexpr unsigned trials=3,reps=2;
 for(unsigned t=0;t<trials;++t){
  auto t0=std::chrono::steady_clock::now();auto c0=std::clock();
  for(unsigned k=0;k<reps;++k){auto a=encode(games);if(a.bytes!=warm.bytes)throw std::runtime_error("nondeterminism");}
  auto c1=std::clock();auto t1=std::chrono::steady_clock::now();
  for(unsigned k=0;k<reps;++k)if(mid_decode(warm.bytes)!=games)throw std::runtime_error("decode mismatch");
  auto c2=std::clock();auto t2=std::chrono::steady_clock::now();
  ew.push_back(std::chrono::duration<double>(t1-t0).count()/reps);dw.push_back(std::chrono::duration<double>(t2-t1).count()/reps);
  ec.push_back(double(c1-c0)/CLOCKS_PER_SEC/reps);dc.push_back(double(c2-c1)/CLOCKS_PER_SEC/reps);
 }
 auto median=[](auto v){std::sort(v.begin(),v.end());return v[v.size()/2];};
 std::cout<<std::fixed<<std::setprecision(4)<<mask<<','<<plies<<','<<warm.payload_bits<<','<<warm.bytes.size()<<','<<plies/median(ew)<<','<<plies/median(dw)<<','<<plies/median(ec)<<','<<plies/median(dc)<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
