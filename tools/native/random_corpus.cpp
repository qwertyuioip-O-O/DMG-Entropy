#include "rmc.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
int main(int argc,char**argv){
 int games=argc>=2?std::stoi(argv[1]):64;
 int maxplies=argc>=3?std::stoi(argv[2]):96;
 uint64_t seed=argc>=4?std::stoull(argv[3]):20261021ULL;
 std::mt19937_64 rng(seed);
 for(int g=0;g<games;++g){
   auto p=rmc::Position::standard();
   for(int ply=0;ply<maxplies;++ply){
       auto legals=p.legal_moves();if(legals.empty())break;
       auto move=legals[rng()%legals.size()];
       if(!p.apply_checked(move))throw std::runtime_error("bad legal move");
       std::cout<<rmc::to_uci(move)<<' ';
   }
   std::cout<<'\n';
 }
}
