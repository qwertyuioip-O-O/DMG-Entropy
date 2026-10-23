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
 std::cout<<"dataset,mode,rate,capture_rate,games,plies,bits,bytes,mid_n,mid_cdf,open_cdf,end_cdf,roundtrip\n"<<std::setprecision(10);
 auto run=[&](int mode,int rate,int capture_rate){MidDiagnostics d;auto a=mode==-2?mid_encode(games,MidProfile::R12JointAll,&d):mode==-1?mid_encode_structural(games,{95,7,1},&d):mid_encode_pawn_split(games,Dmt15Config{uint8_t(mode),uint8_t(rate),uint8_t(capture_rate)},&d);if(mid_decode(a.bytes)!=games)throw std::runtime_error("roundtrip");
 std::cout<<argv[1]<<','<<mode<<','<<rate<<','<<capture_rate<<','<<games.size()<<','<<n<<','<<a.payload_bits<<','<<a.bytes.size()<<','<<d.phase[1].moves;
 for(auto phase:{1,0,2})std::cout<<','<<(d.phase[phase].moves?d.phase[phase].logloss/d.phase[phase].moves:0);std::cout<<",PASS\n"<<std::flush;};
 run(-2,4,4);run(-1,1,1);std::stringstream ss(argv[3]);while(std::getline(ss,line,',')){for(auto&c:line)if(c==':')c=' ';std::istringstream item(line);int mode,rate=1,capture_rate=1;if(!(item>>mode>>capture_rate))throw std::runtime_error("need mode:capture_rate[:rate]");item>>rate;
 if(mode<0||mode>9||rate<1||rate>16||capture_rate<1||capture_rate>16)throw std::runtime_error("range");run(mode,rate,capture_rate);}

}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
