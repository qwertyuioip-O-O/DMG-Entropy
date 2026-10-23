#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
using namespace rmc;
int main(int argc,char**argv){try{
 if(argc!=4)return 2;std::ifstream in(argv[2]);if(!in)throw std::runtime_error("bundle");std::vector<std::vector<Move>>games;std::string line;uint64_t n=0;
 while(std::getline(in,line))if(!line.empty()){auto g=parse_moves(line);n+=g.size();games.push_back(g);}
 std::cout<<"dataset,layout,rate,cap,games,plies,bits,bytes,mid_n,mid_cdf,open_cdf,end_cdf,mid_source,mid_conditional,roundtrip\n"<<std::setprecision(10);
 auto run=[&](int mode,int rate,int cap){MidDiagnostics d;auto a=mode==-2?mid_encode(games,MidProfile::R12JointAll,&d):mode==-1?mid_encode_structural(games,{95,7,1},&d):mid_encode_source_model(games,Dmt16Config{uint8_t(mode),uint8_t(rate),uint8_t(cap)},&d);if(mid_decode(a.bytes)!=games)throw std::runtime_error("roundtrip");
 std::cout<<argv[1]<<','<<mode<<','<<rate<<','<<cap<<','<<games.size()<<','<<n<<','<<a.payload_bits<<','<<a.bytes.size()<<','<<d.phase[1].moves;
 for(auto phase:{1,0,2})std::cout<<','<<(d.phase[phase].moves?d.phase[phase].logloss/d.phase[phase].moves:0);std::cout<<','<<(d.phase[1].moves?d.phase[1].source_logloss/d.phase[1].moves:0)<<','<<(d.phase[1].moves?(d.phase[1].logloss-d.phase[1].source_logloss)/d.phase[1].moves:0)<<",PASS\n"<<std::flush;};
 run(-2,4,32);run(-1,1,32);std::stringstream ss(argv[3]);while(std::getline(ss,line,',')){for(auto&c:line)if(c==':')c=' ';std::istringstream item(line);int mode,rate=1,cap=32;if(!(item>>mode>>rate>>cap))throw std::runtime_error("need layout:rate:cap");
 if(mode<0||mode>5||rate<1||rate>16||cap<1||cap>128)throw std::runtime_error("range");run(mode,rate,cap);}

}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
