#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>
using namespace rmc;
int main(int argc,char**argv){try{
 if(argc!=4){std::cerr<<"r12_search DATASET BUNDLE CONFIGS.csv\n";return 2;}
 std::ifstream in(argv[2]);if(!in)throw std::runtime_error("bundle missing");std::string line;std::vector<std::vector<Move>> games;uint64_t plies=0;
 while(std::getline(in,line))if(!line.empty()){auto g=parse_moves(line);plies+=g.size();games.push_back(g);}
 std::cout<<"dataset,config,families,rate,alpha,phase_mask,base,decay_log2,cap,plies,bits,bytes,mid_cdf,open_cdf,end_cdf,roundtrip\n";
 auto row=[&](std::string label,Dmt12Config c,bool baseline){
  MidDiagnostics d;auto a=baseline?mid_encode(games,MidProfile::R10ComboTact4Full,&d):mid_encode_experimental(games,c,&d);
  if(mid_decode(a.bytes)!=games)throw std::runtime_error("roundtrip mismatch");
  std::cout<<argv[1]<<','<<label<<','<<unsigned(c.families)<<','<<unsigned(c.rate)<<','<<unsigned(c.alpha)<<','<<unsigned(c.phase_mask)<<','<<unsigned(c.base)<<','<<unsigned(c.decay_log2)<<','<<unsigned(c.cap)<<','<<plies<<','<<a.payload_bits<<','<<a.bytes.size()<<std::setprecision(9);
  for(unsigned phase:{1u,0u,2u})std::cout<<','<<(d.phase[phase].moves?d.phase[phase].logloss/d.phase[phase].moves:0);
  std::cout<<",PASS\n"<<std::flush;
 };
 row("baseline234",{},true);
 std::ifstream configs(argv[3]);if(!configs)throw std::runtime_error("configs missing");
 while(std::getline(configs,line)){if(line.empty()||line[0]=='#')continue;
  std::stringstream ss(line);std::string token,label;std::getline(ss,label,',');unsigned v[7]{};
  for(auto& x:v){if(!std::getline(ss,token,','))throw std::runtime_error("bad config row");x=unsigned(std::stoul(token));if(x>255)throw std::runtime_error("config overflow");}
  row(label,{uint8_t(v[0]),uint8_t(v[1]),uint8_t(v[2]),uint8_t(v[3]),uint8_t(v[4]),uint8_t(v[5]),uint8_t(v[6])},false);
 }
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
