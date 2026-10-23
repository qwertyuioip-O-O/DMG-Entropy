#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace rmc;
int main(int argc,char**argv){
 if(argc<4){std::cerr<<"usage: experiment_matrix NAME UCI_BUNDLE PROFILE[,PROFILE...]\n";return 2;}
 std::ifstream f(argv[2]);if(!f)throw std::runtime_error("not found");
 std::vector<std::vector<Move>> games;std::string line;uint64_t plies=0;
 while(std::getline(f,line))if(!line.empty()){auto g=parse_moves(line);plies+=g.size();games.push_back(std::move(g));}
 std::cout<<"dataset,profile,plies,bits,bytes,mid_n,mid_cdf,open_cdf,end_cdf,roundtrip\n";
 std::stringstream ss(argv[3]);
 while(std::getline(ss,line,',')){
  auto id=unsigned(std::stoul(line));MidDiagnostics d;
  auto encoded=mid_encode(games,MidProfile(id),&d);
  auto result=mid_decode(encoded.bytes);if(result!=games)throw std::runtime_error("roundtrip mismatch");
  std::cout<<argv[1]<<','<<id<<','<<plies<<','<<encoded.payload_bits<<','<<encoded.bytes.size()
      <<','<<d.phase[1].moves<<','<<std::setprecision(8)<<d.phase[1].logloss/d.phase[1].moves
      <<','<<d.phase[0].logloss/d.phase[0].moves<<','<<d.phase[2].logloss/d.phase[2].moves<<",PASS\n"<<std::flush;
 }
}
