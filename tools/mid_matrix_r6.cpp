#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
using namespace rmc;
int main(int argc,char**argv){
 if(argc<3){std::cerr<<"usage: dmg_mid_matrix dataset bundle|file.uci...\n";return 2;}
 try{
  std::vector<std::vector<Move>> games;std::ifstream in(argv[2]);if(!in)throw std::runtime_error("missing data");
  if(argc==3){std::string line;while(std::getline(in,line))if(!line.empty())games.push_back(parse_moves(line));}
  else for(int i=2;i<argc;++i){std::ifstream gamefile(argv[i]);if(!gamefile)throw std::runtime_error("missing game");std::string all{std::istreambuf_iterator<char>(gamefile),{}};games.push_back(parse_moves(all));}
  uint64_t plies=0;for(const auto&g:games)plies+=g.size();
  std::cout<<"dataset,profile,games,plies,payload_bits,payload_bpp,total_bytes,opening_plies,opening_cdf_bpp,middle_plies,middle_cdf_bpp,end_plies,end_cdf_bpp,enc_plies_sec,dec_plies_sec,roundtrip\n";
  for(int id:{49,61,68,69,70,71,72,73,74,75,76,77,78,79}){
   MidDiagnostics d;auto start=std::chrono::steady_clock::now();auto a=mid_encode(games,MidProfile(id),&d);auto middle=std::chrono::steady_clock::now();auto dec=mid_decode(a.bytes);auto end=std::chrono::steady_clock::now();
   if(dec!=games)throw std::runtime_error("roundtrip failure");
   auto dt=std::chrono::duration<double>(middle-start).count(),dd=std::chrono::duration<double>(end-middle).count();
   std::cout<<std::fixed<<std::setprecision(7)<<argv[1]<<','<<mid_name(MidProfile(id))<<','<<games.size()<<','<<plies<<','<<a.payload_bits<<','<<double(a.payload_bits)/plies<<','<<a.bytes.size();
   for(int k=0;k<3;++k)std::cout<<','<<d.phase[k].moves<<','<<(d.phase[k].moves?d.phase[k].logloss/d.phase[k].moves:0);
   std::cout<<','<<plies/dt<<','<<plies/dd<<",PASS\n"<<std::flush;
  }
 }catch(std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
