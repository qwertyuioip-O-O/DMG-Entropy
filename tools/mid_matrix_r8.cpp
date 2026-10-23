#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace rmc;
int main(int argc,char**argv){
 if(argc!=3){std::cerr<<"usage: dmt8_matrix NAME UCI_BUNDLE\n";return 2;}
 std::ifstream f(argv[2]);if(!f) return 1;
 std::vector<std::vector<Move>> games;std::string line;unsigned plies=0;
 while(std::getline(f,line)){if(line.empty())continue;auto g=parse_moves(line);plies+=g.size();games.push_back(std::move(g));}
 std::cout<<"dataset,profile,games,plies,payload_bits,payload_bpp,bytes,open_n,open_cdf,mid_n,mid_cdf,end_n,end_cdf,enc_speed,dec_speed,roundtrip\n";
 for(int id: {75,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124}){
  try {
   MidDiagnostics d;auto t0=std::chrono::steady_clock::now();
   auto a=mid_encode(games,MidProfile(id),&d);auto t1=std::chrono::steady_clock::now();
   auto recovered=mid_decode(a.bytes);auto t2=std::chrono::steady_clock::now();
   if(recovered!=games)throw std::runtime_error("roundtrip mismatch");
   const double ec=std::chrono::duration<double>(t1-t0).count();
   const double dc=std::chrono::duration<double>(t2-t1).count();
   std::cout<<std::fixed<<std::setprecision(7)<<argv[1]<<','<<mid_name(MidProfile(id))<<','<<games.size()<<','<<plies<<','<<a.payload_bits<<','<<double(a.payload_bits)/plies<<','<<a.bytes.size();
   for(auto&phase:d.phase)std::cout<<','<<phase.moves<<','<<(phase.moves?phase.logloss/phase.moves:0);
   std::cout<<','<<plies/ec<<','<<plies/dc<<",PASS\n"<<std::flush;
  }catch(const std::exception&e){std::cerr<<"profile "<<id<<" failed: "<<e.what()<<"\n";return 1;}
 }
}
