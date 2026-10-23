#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace rmc;
int main(int argc,char**argv){try{
 if(argc!=3)return 2;const unsigned profile=std::stoul(argv[1]);if(profile!=252&&profile!=254)throw std::runtime_error("profile252/254 only");std::ifstream in(argv[2]);if(!in)throw std::runtime_error("bundle");std::vector<std::vector<Move>>games;std::string line;uint64_t plies=0;
 while(std::getline(in,line))if(!line.empty()){auto g=parse_moves(line);plies+=g.size();games.push_back(g);}
 auto encode=[&](){return profile==252?mid_encode_structural(games,{95,7,1}):mid_encode_source_model(games,{0,2,128});};auto a=encode();if(mid_decode(a.bytes)!=games)throw std::runtime_error("warm roundtrip");
 auto w0=std::chrono::steady_clock::now();auto c0=std::clock();for(unsigned j=0;j<2;++j)if(encode().bytes!=a.bytes)throw std::runtime_error("nondeterminism");
 auto c1=std::clock();auto w1=std::chrono::steady_clock::now();for(unsigned j=0;j<2;++j)if(mid_decode(a.bytes)!=games)throw std::runtime_error("roundtrip");
 auto c2=std::clock();auto w2=std::chrono::steady_clock::now();
 std::cout<<std::setprecision(10)<<profile<<','<<(profile==252?-1:0)<<','<<(profile==252?1:2)<<','<<(profile==252?32:128)<<','<<plies<<','<<a.payload_bits<<','<<a.bytes.size()<<','<<std::chrono::duration<double>(w1-w0).count()/2<<','<<std::chrono::duration<double>(w2-w1).count()/2<<','<<double(c1-c0)/CLOCKS_PER_SEC/2<<','<<double(c2-c1)/CLOCKS_PER_SEC/2<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
