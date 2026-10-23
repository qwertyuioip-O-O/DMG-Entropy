#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
using namespace rmc;
int main(int argc,char**argv){
 if(argc!=3){std::cerr<<"usage: expert_analysis NAME BUNDLE\n";return 2;}
 std::ifstream f(argv[2]);if(!f)return 1;
 std::vector<std::vector<Move>>games;std::string line;
 while(std::getline(f,line)){if(line.empty())continue;games.push_back(parse_moves(line));}
 MidDiagnostics d;mid_encode(games,MidProfile::R8Control,&d);
 const char* names[]={"relation","tactical","rank","positional","full-dmt6"};
 std::cout<<"dataset,context,plies,expert,logloss,total_bits\n";
 for(int c=0;c<5;++c){
  auto num=c==4?d.phase[1].moves:d.r8_context_count[c];
  for(int k=0;k<5;++k){
   double loss=c==4?d.r8_expert_loss[k]:d.r8_context_loss[c][k];
   std::cout<<argv[1]<<','<<c<<','<<num<<','<<names[k]<<','<<std::fixed<<std::setprecision(7)<<(num?loss/num:0)<<','<<loss<<'\n';
  }
 }
}
