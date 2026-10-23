#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <string>
using namespace rmc;
int main(int argc,char**argv){
  if(argc<2)return 2;
  std::ifstream in(argv[1]);std::string s;std::vector<std::vector<Move>> games;
  while(std::getline(in,s))if(!s.empty())games.push_back(parse_moves(s));
  for(unsigned i=0;i<68;++i){auto a=mid_encode(games,MidProfile(i));
    uint64_t h=14695981039346656037ull;
    for(auto b:a.bytes){h^=b;h*=1099511628211ull;}
    std::cout<<i<<','<<a.payload_bits<<','<<a.bytes.size()<<','<<h<<'\n';
  }
}
