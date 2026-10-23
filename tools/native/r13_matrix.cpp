#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
using namespace rmc;
int main(int argc,char**argv){try{
 if(argc!=4)return 2;std::ifstream in(argv[2]);if(!in)throw std::runtime_error("bundle");std::vector<std::vector<Move>> games;std::string line;uint64_t n=0;
 while(std::getline(in,line))if(!line.empty()){auto g=parse_moves(line);n+=g.size();games.push_back(g);}
 std::cout<<"dataset,mask,games,plies,bits,bytes,mid_n,mid_cdf,open_cdf,end_cdf,roundtrip\n"<<std::setprecision(10);
 auto run=[&](int mask){MidDiagnostics d;auto a=mask<0?mid_encode(games,MidProfile::R12JointAll,&d):mid_encode_structure(games,uint8_t(mask),&d);if(mid_decode(a.bytes)!=games)throw std::runtime_error("roundtrip");
 std::cout<<argv[1]<<','<<mask<<','<<games.size()<<','<<n<<','<<a.payload_bits<<','<<a.bytes.size()<<','<<d.phase[1].moves;
 for(auto phase:{1,0,2})std::cout<<','<<(d.phase[phase].moves?d.phase[phase].logloss/d.phase[phase].moves:0);std::cout<<",PASS\n"<<std::flush;};
 run(-1);std::stringstream ss(argv[3]);while(std::getline(ss,line,',')){int mask=std::stoi(line);if(mask<0||mask>255)throw std::runtime_error("mask");run(mask);}
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
