#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
using namespace rmc;
int main(){
 try{
  std::vector<std::vector<Move>> games;
  for(const auto& path:{"examples/heldout/1852_anderssen_kieseritzky.uci","examples/heldout/1852_anderssen_dufresne.uci"}){
   std::ifstream in(path);if(in){std::string t{std::istreambuf_iterator<char>(in),{}};games.push_back(parse_moves(t));}
  }
  if(games.empty())games.push_back(parse_moves("e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 d2d3 f8c5"));
  std::mt19937_64 rng(912345);
  for(int i=0;i<4;++i){Position p=Position::standard();std::vector<Move>g;
   for(int j=0;j<80;++j){auto v=p.legal_moves();if(v.empty())break;auto m=v[rng()%v.size()];
    if(!p.apply_checked(m))throw std::runtime_error("illegal generated move");g.push_back(m);}
   games.push_back(g);
  }
  for(unsigned id=68;id<MID_PROFILE_COUNT;++id){
   auto x=mid_encode(games,MidProfile(id));
   if(mid_decode(x.bytes)!=games)throw std::runtime_error("roundtrip mismatch");
   if(mid_encode(games,MidProfile(id)).bytes!=x.bytes)throw std::runtime_error("nondeterminism");
   auto corrupt=x.bytes;corrupt[corrupt.size()-1]^=0x80;
   bool rejected=false;try{(void)mid_decode(corrupt);}catch(...){rejected=true;}
   if(!rejected)throw std::runtime_error("CRC not rejected");
  }
  std::cout<<"DMT6 sanitizer smoke: all new profiles PASS\n";
 }catch(std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
