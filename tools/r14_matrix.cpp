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
 std::cout<<"dataset,features,rate,structure,games,plies,bits,bytes,mid_n,mid_cdf,open_cdf,end_cdf,roundtrip\n"<<std::setprecision(10);
 auto run=[&](int features,int rate,int structure){MidDiagnostics d;auto a=features==-2?mid_encode(games,MidProfile::R12JointAll,&d):features==-1?mid_encode_structure(games,uint8_t(structure),&d):mid_encode_structural(games,Dmt14Config{uint8_t(structure),uint8_t(features),uint8_t(rate)},&d);if(mid_decode(a.bytes)!=games)throw std::runtime_error("roundtrip");
 std::cout<<argv[1]<<','<<features<<','<<rate<<','<<structure<<','<<games.size()<<','<<n<<','<<a.payload_bits<<','<<a.bytes.size()<<','<<d.phase[1].moves;
 for(auto phase:{1,0,2})std::cout<<','<<(d.phase[phase].moves?d.phase[phase].logloss/d.phase[phase].moves:0);std::cout<<",PASS\n"<<std::flush;};
 run(-2,4,127);run(-1,4,95);std::stringstream ss(argv[3]);while(std::getline(ss,line,',')){for(auto&c:line)if(c==':')c=' ';std::istringstream item(line);int features,rate,structure=95;if(!(item>>features>>rate))throw std::runtime_error("need features:rate[:structure]");item>>structure;
 if(features<0||features>7||rate<1||rate>16||structure<0||structure>255)throw std::runtime_error("range");run(features,rate,structure);}
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
