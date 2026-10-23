#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <numeric>
#include <stdexcept>
#include <vector>
using namespace rmc;
using Clock=std::chrono::steady_clock;
int main(int argc,char**argv){
 if(argc<3){std::cerr<<"usage: mid_speed PROFILE *.uci | PROFILE --bundle games.uci-bundle\n";return 2;}
 try{
   unsigned id=unsigned(std::stoul(argv[1]));if(id>=MID_PROFILE_COUNT)throw std::runtime_error("invalid profile");
   MidProfile profile=MidProfile(id);std::vector<std::vector<Move>> games;
   uint64_t plies=0;
   if(std::string(argv[2])=="--bundle"){
      if(argc!=4)throw std::runtime_error("--bundle requires a single path");
      std::ifstream f(argv[3]);if(!f)throw std::runtime_error("bundle not found");
      std::string line;while(std::getline(f,line)){
          if(line.empty())continue;
          auto game=parse_moves(line);plies+=game.size();games.push_back(std::move(game));}
   } else for(int i=2;i<argc;++i){std::ifstream f(argv[i]);if(!f)throw std::runtime_error("file not found");
      std::string text{std::istreambuf_iterator<char>(f),{}};
      auto game=parse_moves(text);plies+=game.size();games.push_back(std::move(game));}
   constexpr int reps=8,trials=7;
   auto warm=mid_encode(games,profile);if(mid_decode(warm.bytes)!=games)throw std::runtime_error("warmup mismatch");
   std::vector<double> enc,dec;
   uint64_t checksum=0;
   for(int t=0;t<trials;++t){
      auto start=Clock::now();
      for(int k=0;k<reps;++k){auto a=mid_encode(games,profile);
         checksum+=a.payload_bits;if(a.bytes!=warm.bytes)throw std::runtime_error("encoder nondeterminism");}
      auto middle=Clock::now();
      for(int k=0;k<reps;++k){auto decoded=mid_decode(warm.bytes);
         if(decoded!=games)throw std::runtime_error("roundtrip failure");checksum+=decoded.size();}
      auto finish=Clock::now();
      enc.push_back(std::chrono::duration<double>(middle-start).count()/reps);
      dec.push_back(std::chrono::duration<double>(finish-middle).count()/reps);
   }
   std::sort(enc.begin(),enc.end());std::sort(dec.begin(),dec.end());
   MidDiagnostics diag;auto out=mid_encode(games,profile,&diag);
   std::cout<<std::fixed<<std::setprecision(7)
   <<mid_name(profile)<<","<<games.size()<<","<<plies<<","<<out.payload_bits<<","<<out.bytes.size()
   <<","<<plies/enc[trials/2]<<","<<plies/dec[trials/2]
   <<","<<(diag.phase[1].moves?diag.phase[1].logloss/diag.phase[1].moves:0)
   <<","<<checksum<<"\n";
 }catch(std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
}
