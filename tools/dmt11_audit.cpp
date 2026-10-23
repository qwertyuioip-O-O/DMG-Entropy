#include "dmg_mid.hpp"
#include "rmc.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace rmc;
int main(int argc,char**argv){try{
 if(argc!=4)return 2;std::ifstream input(argv[1]);if(!input)throw std::runtime_error("missing bundle");
 std::vector<std::vector<Move>> games;std::string line;
 while(std::getline(input,line))if(!line.empty())games.push_back(parse_moves(line));
 std::filesystem::create_directories(argv[2]);
 for(unsigned id=0;id<unsigned(std::stoul(argv[3]));++id){
  auto a=mid_encode(games,MidProfile(id));if(mid_decode(a.bytes)!=games)throw std::runtime_error("roundtrip");
  std::ofstream out(std::filesystem::path(argv[2])/(std::to_string(id)+".dmt"),std::ios::binary);
  out.write(reinterpret_cast<const char*>(a.bytes.data()),a.bytes.size());if(!out)throw std::runtime_error("write");
 }
 std::cout<<"archive audit PASS "<<argv[3]<<" profiles\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
