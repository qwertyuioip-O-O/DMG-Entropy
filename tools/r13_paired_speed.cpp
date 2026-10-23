#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iomanip>
using namespace rmc;
int main(int argc,char**argv){try{
 if(argc!=2)return 2;std::ifstream in(argv[1]);if(!in)throw std::runtime_error("bundle");std::vector<std::vector<Move>>games;std::string line;uint64_t plies=0;
 while(std::getline(in,line))if(!line.empty()){auto g=parse_moves(line);plies+=g.size();games.push_back(g);}
 auto encode=[&](int mask){return mask<0?mid_encode(games,MidProfile::R12JointAll):mid_encode_structure(games,uint8_t(mask));};
 auto base=encode(-1),candidate=encode(95);if(mid_decode(base.bytes)!=games||mid_decode(candidate.bytes)!=games)throw std::runtime_error("warm roundtrip");
 std::cout<<"round,order,mask,plies,encode_wall_s,decode_wall_s,encode_cpu_s,decode_cpu_s\n"<<std::setprecision(10);
 for(unsigned round=0;round<11;++round)for(unsigned order=0;order<2;++order){int mask=((round+order)%2)?95:-1;const auto&archive=mask<0?base:candidate;
  auto w0=std::chrono::steady_clock::now();auto c0=std::clock();
  for(unsigned j=0;j<2;++j)if(encode(mask).bytes!=archive.bytes)throw std::runtime_error("nondeterminism");
  auto c1=std::clock();auto w1=std::chrono::steady_clock::now();
  for(unsigned j=0;j<2;++j)if(mid_decode(archive.bytes)!=games)throw std::runtime_error("roundtrip");
  auto c2=std::clock();auto w2=std::chrono::steady_clock::now();
  std::cout<<round<<','<<order<<','<<mask<<','<<plies<<','<<std::chrono::duration<double>(w1-w0).count()/2<<','<<std::chrono::duration<double>(w2-w1).count()/2<<','<<double(c1-c0)/CLOCKS_PER_SEC/2<<','<<double(c2-c1)/CLOCKS_PER_SEC/2<<'\n'<<std::flush;
 }
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
